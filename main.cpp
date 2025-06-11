/*
 * lswt - list Wayland toplevels
 *
 * Copyright (C) 2021 - 2023 Leon Henrik Plickat
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "main.h"
#include "x_proxy_windows.h"

#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <setjmp.h>
#include <wayland-client.h>

#ifdef __linux__

#include <features.h>
#include <linux/landlock.h>
#include <sys/prctl.h>
#include <sys/syscall.h>

#ifdef __GLIBC__

#include<execinfo.h>

#endif
#endif

#include "wlr-foreign-toplevel-management-unstable-v1.h"
#include "ext-foreign-toplevel-list-v1.h"

#define BOOL_TO_STR(B) (B) ? "true" : "false"

#define VERSION "2.0.1-dev"

#define max_supported_landlock_abi 3

const char usage[] =
        "Usage: lswt [options...]\n"
        "  -h,       --help            Print this helpt text and exit.\n"
        "  -v,       --version         Print version and exit.\n"
        "  -j,       --json            Output data in JSON format.\n"
        "  -w,       --watch           Run continously and log title, identifier\n"
        "                              and app-id events.\n"
        "  -W,       --verbose-watch   Like --watch, but also log activated, fullscreen,\n"
        "                              minimized and maximized state.\n"
        "  -c <fmt>, --custom <fmt>    Define a custom line-based output format.\n"
        "  --force-protocol <name>     Use specified protocol, do not fall back onto others.";

enum Output_format {
    NORMAL,
    CUSTOM,
    JSON,
};
enum Output_format output_format = NORMAL;
char *custom_output_format = NULL;
const char custom_output_delim = ',';

enum Mode {
    LIST,
    WATCH,
    VERBOSE_WATCH,
};
enum Mode mode = WATCH;

/** Used for padding when printing output in NORMAL format. */
size_t longest_app_id = 7; // strlen("app-id:")
const size_t max_app_id_padding = 40;

int ret = EXIT_SUCCESS;
bool loop = true;
bool debug_log = false;

struct wl_display *wl_display = NULL;
struct wl_registry *wl_registry = NULL;
struct wl_callback *sync_callback = NULL;
struct wl_seat *seat = NULL;

/* We implement both the new protocol (ext-*) as well as the old one it is based
 * on (zwlr-*), since there likely are compositors still stuck with the legacy
 * one for a while.
 * NOTE: zwlr-foreign-toplevel-management-v1 support will be deprecated eventually!
 */
struct zwlr_foreign_toplevel_manager_v1 *zwlr_toplevel_manager = NULL;
struct ext_foreign_toplevel_list_v1 *ext_toplevel_list = NULL;

enum UsedProtocol {
    NONE,
    ZWLR_FOREIGN_TOPLEVEL,
    EXT_FOREIGN_TOPLEVEL,
};
enum UsedProtocol used_protocol;
bool force_protocol = true;

struct wl_list toplevels;

/* We want to cleanly exit on SIGINT (f.e. when Ctrl-C is pressed in WATCH mode)
 * however after exiting the signal handler wl_display_dispatch() will just
 * continue until the next event from the server. We can not sync in the signal
 * handler, so let's just long-jump to right before the main-loop and skip it.
 */
jmp_buf skip_main_loop;

/**********************
 *                    *
 *    Capabilities    *
 *                    *
 **********************/
bool support_fullscreen = false;
bool support_activated = false;
bool support_maximized = false;
bool support_minimized = false;
bool support_identifier = false;

bool print_state = false;

static void update_capabilities(void) {
    switch (used_protocol) {
        case ZWLR_FOREIGN_TOPLEVEL:
            support_fullscreen = true;
            support_activated = true;
            support_maximized = true;
            support_minimized = true;
            break;
        
        case EXT_FOREIGN_TOPLEVEL:
            support_identifier = true;
            break;
        
        case NONE: /* Unreachable. */
            assert(false);
            break;
    }
    
    if (support_fullscreen && support_activated && support_maximized && support_minimized)
        print_state = true;
}

/** Allocate a new Toplevel and initialize it. Returns pointer to the Toplevel. */
Toplevel *toplevel_new(void) {
    auto toplevel = new Toplevel;
    if (toplevel == NULL) {
        fprintf(stderr, "ERROR: calloc(): %s\n", strerror(errno));
        return NULL;
    }
    
    static size_t id_counter = 0;
    
    toplevel->id = id_counter++;
    toplevel->zwlr_handle = NULL;
    toplevel->ext_handle = NULL;
    toplevel->title = "";
    toplevel->app_id = "";
    toplevel->identifier = "";
    toplevel->listed = false;
    
    toplevel->fullscreen = false;
    toplevel->activated = false;
    toplevel->maximized = false;
    toplevel->minimized = false;
    
    if (mode == WATCH || mode == VERBOSE_WATCH || debug_log)
        fprintf(stdout, "toplevel %ld: created\n", toplevel->id);
    
    return toplevel;
}

/** Destroys a toplevel and removes it from the list, if it is listed. */
static void toplevel_destroy(struct Toplevel *self) {
    destroy_proxy_for(self);
    if (mode == WATCH || mode == VERBOSE_WATCH || debug_log)
        fprintf(stdout, "toplevel %ld: destroyed\n", self->id);
    
    if (self->zwlr_handle != NULL)
        zwlr_foreign_toplevel_handle_v1_destroy(self->zwlr_handle);
    if (self->ext_handle != NULL)
        ext_foreign_toplevel_handle_v1_destroy(self->ext_handle);
    if (self->listed)
        wl_list_remove(&self->link);
    free(self);
}

/** Set the title of the toplevel. Called from protocol implementations. */
static void toplevel_set_title(struct Toplevel *self, const char *title) {
    self->old_title = self->title;
    if (mode == WATCH || mode == VERBOSE_WATCH || debug_log) {
        if (self->title.empty())
            fprintf(
                    stdout,
                    "toplevel %ld: set title: '%s'\n",
                    self->id, title
            );
        
        else
            fprintf(
                    stdout,
                    "toplevel %ld: change title: '%s' -> '%s'\n",
                    self->id, self->title.c_str(), title
            );
    }
    
    if (!self->title.empty())
        self->title = "";
    self->title = strdup(title);
    if (self->title.empty())
        fprintf(stderr, "ERROR: strdup(): %s\n", strerror(errno));
    update_title_for(self);
}

/** Set the app-id of the toplevel. Called from protocol implementations. */
static size_t real_strlen(const char *str);

static void toplevel_set_app_id(struct Toplevel *self, const char *app_id) {
    if (mode == WATCH || mode == VERBOSE_WATCH || debug_log) {
        if (self->app_id.empty())
            fprintf(
                    stdout,
                    "toplevel %ld: set app-id: '%s'\n",
                    self->id, app_id
            );
        
        else
            fprintf(
                    stdout,
                    "toplevel %ld: change app-id: '%s' -> '%s'\n",
                    self->id, self->app_id.c_str(), app_id
            );
    }
    
    if (!self->app_id.empty())
        self->app_id;
    self->app_id = strdup(app_id);
    if (self->app_id.empty()) {
        fprintf(stderr, "ERROR: strdup(): %s\n", strerror(errno));
        return;
    }
    
    /* Used when printing output in the default human readable format. */
    const size_t len = real_strlen(app_id);
    if (len > longest_app_id && max_app_id_padding > len)
        longest_app_id = len;
    
    create_proxy_for(self);
}

/** Set the identifier of the toplevel. Called from protocol implementations. */
static void toplevel_set_identifier(struct Toplevel *self, const char *identifier) {
    if (mode == WATCH || mode == VERBOSE_WATCH || debug_log)
        fprintf(
                stdout,
                "toplevel %ld: set identifier: %s\n",
                self->id, identifier
        );
    
    if (!self->identifier.empty()) {
        fputs(
                "ERROR: protocol-error: Server changed identifier of toplevel, "
                "which is forbidden by the protocol. Continuing anyway...\n",
                stderr
        );
        self->identifier = "";
    }
    self->identifier = strdup(identifier);
    if (self->identifier.empty())
        fprintf(stderr, "ERROR: strdup(): %s\n", strerror(errno));
}

static void toplevel_set_fullscreen(struct Toplevel *self, bool fullscreen) {
    if (mode == VERBOSE_WATCH || debug_log)
        fprintf(
                stdout,
                "toplevel %ld: fullscreen: %s\n",
                self->id, BOOL_TO_STR(fullscreen)
        );
    
    self->fullscreen = fullscreen;
}

static void toplevel_set_activated(struct Toplevel *self, bool activated) {
    if (mode == VERBOSE_WATCH || debug_log)
        fprintf(
                stdout,
                "toplevel %ld: activated (focused): %s\n",
                self->id, BOOL_TO_STR(activated)
        );
    self->activated = activated;
}

static void toplevel_set_maximized(struct Toplevel *self, bool maximized) {
    if (mode == VERBOSE_WATCH || debug_log)
        fprintf(
                stdout,
                "toplevel %ld: maximized: %s\n",
                self->id, BOOL_TO_STR(maximized)
        );
    self->maximized = maximized;
}

static void toplevel_set_minimized(struct Toplevel *self, bool minimized) {
    if (mode == VERBOSE_WATCH || debug_log)
        fprintf(
                stdout,
                "toplevel %ld: minimized: %s\n",
                self->id, BOOL_TO_STR(minimized)
        );
    self->minimized = minimized;
}

static void toplevel_done(struct Toplevel *self) {
    if (debug_log)
        fprintf(stderr, "[toplevel %ld: done]\n", self->id);
    
    if (self->listed)
        return;
    self->listed = true;
    
    wl_list_insert(&toplevels, &self->link);
}

/*****************************************************
 *                                                   *
 *    ext-foreign-toplevel-list-v1 implementation    *
 *                                                   *
 *****************************************************/
static void ext_foreign_handle_handle_identifier
        (
                void *data,
                struct ext_foreign_toplevel_handle_v1 *handle,
                const char *identifier
        ) {
    struct Toplevel *toplevel = (struct Toplevel *) data;
    toplevel_set_identifier(toplevel, identifier);
}

static void ext_foreign_handle_handle_title
        (
                void *data,
                struct ext_foreign_toplevel_handle_v1 *handle,
                const char *title
        ) {
    struct Toplevel *toplevel = (struct Toplevel *) data;
    toplevel_set_title(toplevel, title);
}

static void ext_foreign_handle_handle_app_id
        (
                void *data,
                struct ext_foreign_toplevel_handle_v1 *handle,
                const char *app_id
        ) {
    struct Toplevel *toplevel = (struct Toplevel *) data;
    toplevel_set_app_id(toplevel, app_id);
}

static void ext_foreign_handle_handle_done
        (
                void *data,
                struct ext_foreign_toplevel_handle_v1 *handle
        ) {
    struct Toplevel *toplevel = (struct Toplevel *) data;
    toplevel_done(toplevel);
}

static void ext_foreign_handle_handle_closed
        (
                void *data,
                struct ext_foreign_toplevel_handle_v1 *handle
        ) {
    /* We only care when watching for events. */
    if (mode == WATCH || mode == VERBOSE_WATCH) {
        struct Toplevel *toplevel = (struct Toplevel *) data;
        toplevel_destroy(toplevel);
    }
}

static const struct ext_foreign_toplevel_handle_v1_listener ext_handle_listener = {
        .closed       = ext_foreign_handle_handle_closed,
        .done         = ext_foreign_handle_handle_done,
        .title        = ext_foreign_handle_handle_title,
        .app_id       = ext_foreign_handle_handle_app_id,
        .identifier   = ext_foreign_handle_handle_identifier,
};

static void ext_toplevel_list_handle_toplevel
        (
                void *data,
                struct ext_foreign_toplevel_list_v1 *list,
                struct ext_foreign_toplevel_handle_v1 *handle
        ) {
    if (used_protocol != EXT_FOREIGN_TOPLEVEL) {
        assert(used_protocol != NONE);
        ext_foreign_toplevel_handle_v1_destroy(handle);
        return;
    }
    
    struct Toplevel *toplevel = toplevel_new();
    if (toplevel == NULL)
        return;
    toplevel->ext_handle = handle;
    ext_foreign_toplevel_handle_v1_add_listener(
            handle,
            &ext_handle_listener,
            toplevel
    );
}

static void ext_toplevel_list_handle_finished
        (
                void *data,
                struct ext_foreign_toplevel_list_v1 *list
        ) {
    /* deliberately left empty */
}

static const struct ext_foreign_toplevel_list_v1_listener ext_toplevel_list_listener = {
        .toplevel = ext_toplevel_list_handle_toplevel,
        .finished = ext_toplevel_list_handle_finished,
};

void activate_toplevel(int window) {
    struct Toplevel *t, *tmp;
    if (!seat)
        return;
    printf("search\n");
    wl_list_for_each_reverse_safe(t, tmp, &toplevels, link) {
        if (t->x11_proxy_window_id == window) {
            if (t->zwlr_handle) {
                zwlr_foreign_toplevel_handle_v1_activate(t->zwlr_handle, seat);
            }
        }
    }
}

void close_toplevel(int window) {
    struct Toplevel *t, *tmp;
    wl_list_for_each_reverse_safe(t, tmp, &toplevels, link) {
        if (t->x11_proxy_window_id == window) {
            if (t->zwlr_handle) {
                printf("closing...: %d\n", window);
                zwlr_foreign_toplevel_handle_v1_destroy(t->zwlr_handle);
            }
        }
    }
}

/************************************************************
 *                                                          *
 *    zwlr-foreign-toplevel-management-v1 implementation    *
 *                                                          *
 ************************************************************/
static void zwlr_foreign_handle_handle_title
        (
                void *data,
                struct zwlr_foreign_toplevel_handle_v1 *handle,
                const char *title
        ) {
    struct Toplevel *toplevel = (struct Toplevel *) data;
    toplevel_set_title(toplevel, title);
}

static void zwlr_foreign_handle_handle_app_id
        (
                void *data,
                struct zwlr_foreign_toplevel_handle_v1 *handle,
                const char *app_id
        ) {
    struct Toplevel *toplevel = (struct Toplevel *) data;
    toplevel_set_app_id(toplevel, app_id);
}

static void zwlr_foreign_handle_handle_state
        (
                void *data,
                struct zwlr_foreign_toplevel_handle_v1 *handle,
                struct wl_array *states
        ) {
    struct Toplevel *toplevel = (struct Toplevel *) data;
    
    bool fullscreen = false;
    bool activated = false;
    bool minimized = false;
    bool maximized = false;
    
    uint32_t *state;
    for (state = (uint32_t *) (states)->data;
         (states)->size != 0 && (const char *) state < ((const char *) (states)->data + (states)->size); (state)++) {
        switch (*state) {
            case ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MAXIMIZED:
                maximized = true;
                break;
            case ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_MINIMIZED:
                minimized = true;
                break;
            case ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_ACTIVATED:
                activated = true;
                break;
            case ZWLR_FOREIGN_TOPLEVEL_HANDLE_V1_STATE_FULLSCREEN:
                fullscreen = true;
                break;
        }
    }
    
    toplevel_set_fullscreen(toplevel, fullscreen);
    toplevel_set_activated(toplevel, activated);
    toplevel_set_minimized(toplevel, minimized);
    toplevel_set_maximized(toplevel, maximized);
}

static void zwlr_foreign_handle_handle_done
        (
                void *data,
                struct zwlr_foreign_toplevel_handle_v1 *handle
        ) {
    struct Toplevel *toplevel = (struct Toplevel *) data;
    toplevel_done(toplevel);
}

static void zwlr_foreign_handle_handle_closed
        (
                void *data,
                struct zwlr_foreign_toplevel_handle_v1 *handle
        ) {
    /* We only care when watching for events. */
    if (mode == WATCH || mode == VERBOSE_WATCH) {
        struct Toplevel *toplevel = (struct Toplevel *) data;
        toplevel_destroy(toplevel);
    }
}

static void zwlr_foreign_handle_handle_output_enter
        (
                void *data,
                struct zwlr_foreign_toplevel_handle_v1 *handle,
                struct wl_output *output
        ) {
    /* deliberately left empty */
}

static void zwlr_foreign_handle_handle_output_leave
        (
                void *data,
                struct zwlr_foreign_toplevel_handle_v1 *handle,
                struct wl_output *output
        ) {
    /* deliberately left empty */
}

static void zwlr_foreign_handle_handle_parent
        (
                void *data,
                struct zwlr_foreign_toplevel_handle_v1 *handle,
                struct zwlr_foreign_toplevel_handle_v1 *parent
        ) {
    /* deliberately left empty */
}

static const struct zwlr_foreign_toplevel_handle_v1_listener zwlr_handle_listener = {
        .title        = zwlr_foreign_handle_handle_title,
        .app_id       = zwlr_foreign_handle_handle_app_id,
        .output_enter = zwlr_foreign_handle_handle_output_enter,
        .output_leave = zwlr_foreign_handle_handle_output_leave,
        .state        = zwlr_foreign_handle_handle_state,
        .done         = zwlr_foreign_handle_handle_done,
        .closed       = zwlr_foreign_handle_handle_closed,
        .parent       = zwlr_foreign_handle_handle_parent,
};

static void zwlr_toplevel_manager_handle_toplevel
        (
                void *data,
                struct zwlr_foreign_toplevel_manager_v1 *manager,
                struct zwlr_foreign_toplevel_handle_v1 *handle
        ) {
    if (used_protocol != ZWLR_FOREIGN_TOPLEVEL) {
        assert(used_protocol != NONE);
        zwlr_foreign_toplevel_handle_v1_destroy(handle);
        return;
    }
    
    struct Toplevel *toplevel = toplevel_new();
    if (toplevel == NULL)
        return;
    toplevel->zwlr_handle = handle;
    printf("we actually create the handle\n");
    zwlr_foreign_toplevel_handle_v1_add_listener(
            handle,
            &zwlr_handle_listener,
            toplevel
    );
}

static void zwlr_toplevel_manager_handle_finished
        (
                void *data,
                struct zwlr_foreign_toplevel_manager_v1 *manager
        ) {
    /* deliberately left empty */
}

static const struct zwlr_foreign_toplevel_manager_v1_listener zwlr_toplevel_manager_listener = {
        .toplevel = zwlr_toplevel_manager_handle_toplevel,
        .finished = zwlr_toplevel_manager_handle_finished,
};

/************************
 *                      *
 *    Command output    *
 *                      *
 ************************/
static bool string_needs_quotes(char *str) {
    for (; *str != '\0'; str++)
        if (isspace(*str) || *str == '"' || *str == '\'' || !isascii(*str))
            return true;
    return false;
}

static void quoted_fputs(size_t *len, char *str, FILE *f) {
    if (str == NULL) {
        if (len != NULL)
            *len = 0;
        return;
    }
    
    size_t l = 2; // Two bytes for the two mandatory quotes.
    
    fputc('"', f);
    for (; *str != '\0'; str++) {
        if (*str == '"') {
            l += 2;
            fputs("\\\"", f);
        } else if (*str == '\n') {
            l += 2;
            fputs("\\n", f);
        } else if (*str == '\t') {
            l += 2;
            fputs("\\t", f);
        } else if (*str == '\\') {
            l += 2;
            fputs("\\\\", f);
        } else {
            l += 1;
            fputc(*str, f);
        }
    }
    fputc('"', f);
    
    if (len != NULL)
        *len = l;
}

static void write_padding(size_t used_len, size_t padding, FILE *f) {
    if (padding > used_len)
        for (size_t i = padding - used_len; i > 0; i--)
            fputc(' ', f);
}

static void write_padded(size_t padding, char *str, FILE *f) {
    size_t len = 0;
    if (str == NULL) {
        fputs("<NULL>", f);
        len = strlen("<NULL>");
    } else {
        len = strlen(str);
        fputs(str, f);
    }
    write_padding(len, padding, f);
}

static void write_padded_maybe_quoted(size_t padding, char *str, FILE *f) {
    size_t len = 0;
    if (str == NULL) {
        fputs("<NULL>", f);
        len = strlen("<NULL>");
    } else if (string_needs_quotes(str))
        quoted_fputs(&len, str, f);
    else {
        len = strlen(str);
        fputs(str, f);
    }
    write_padding(len, padding, f);
}

static void write_maybe_quoted(char *str, FILE *f) {
    if (str == NULL)
        fputs("<NULL>", f);
    else if (string_needs_quotes(str))
        quoted_fputs(NULL, str, f);
    else
        fputs(str, f);
}

/** Always quote strings, except if they are NULL. */
static void write_json(char *str, FILE *f) {
    if (str == NULL)
        fputs("null", f);
    else
        quoted_fputs(NULL, str, f);
}

/** Never quote strings, escape commas, print "<NULL>" on NULL. */
static void write_custom(char *str, FILE *f) {
    if (str == NULL)
        fputs("<NULL>", f);
    else {
        for (; *str != '\0'; str++) {
            if (*str == ',')
                fputs("\\,", f);
            else
                fputc(*str, f);
        }
    }
}

static void write_custom_optional(bool supported, char *str, FILE *f) {
    if (supported)
        write_custom(str, f);
    else
        fputs("unsupported", f);
}

static void write_custom_optional_bool(bool supported, bool b, FILE *f) {
    if (supported)
        fputs(b ? "true" : "false", f);
    else
        fputs("unsupported", f);
}

/** Return the amount of bytes printed when printing the given string. */
static size_t real_strlen(const char *str) {
    size_t i = 0;
    bool has_space = false;
    for (; *str != '\0'; str++) {
        switch (*str) {
            case '"':
            case '\\':
            case '\n':
            case '\t':
                i += 2;
                break;
            
            default:
                if (isspace(*str))
                    has_space = true;
                i++;
                break;
        }
    }
    if (has_space)
        i += 2;
    return i;
}


/********************************
 *                              *
 *    main and Wayland logic    *
 *                              *
 ********************************/
static void registry_handle_global
        (
                void *data,
                struct wl_registry *registry,
                uint32_t name,
                const char *interface,
                uint32_t version
        ) {
    if (strcmp(interface, zwlr_foreign_toplevel_manager_v1_interface.name) == 0) {
        printf("found zwlr zwlr\n");
        if (version < 3)
            return;
        if (debug_log)
            fputs("[Binding zwlr-foreign-toplevel-manager-v1.]\n", stderr);
        zwlr_toplevel_manager = static_cast<zwlr_foreign_toplevel_manager_v1 *>(wl_registry_bind(
                wl_registry,
                name,
                &zwlr_foreign_toplevel_manager_v1_interface,
                3
        ));
        zwlr_foreign_toplevel_manager_v1_add_listener(
                zwlr_toplevel_manager,
                &zwlr_toplevel_manager_listener,
                NULL
        );
    } else if (strcmp(interface, ext_foreign_toplevel_list_v1_interface.name) == 0) {
        printf("found ext\n");
        if (debug_log)
            fputs("[Binding ext-foreign-toplevel-list-v1.]\n", stderr);
        ext_toplevel_list = static_cast<ext_foreign_toplevel_list_v1 *>(wl_registry_bind(
                wl_registry,
                name,
                &ext_foreign_toplevel_list_v1_interface,
                1
        ));
        ext_foreign_toplevel_list_v1_add_listener(
                ext_toplevel_list,
                &ext_toplevel_list_listener,
                NULL
        );
    }else if (strcmp(interface, wl_seat_interface.name) == 0 && seat == NULL) {
        seat = static_cast<wl_seat *>(wl_registry_bind(registry, name, &wl_seat_interface, version));
        printf("seat set\n");
    }
}

static void registry_handle_global_remove
        (
                void *data,
                struct wl_registry *registry,
                uint32_t name
        ) {
    /* deliberately left empty */
}

static const struct wl_registry_listener registry_listener = {
        .global        = registry_handle_global,
        .global_remove = registry_handle_global_remove,
};

static void sync_handle_done
        (
                void *data,
                struct wl_callback *wl_callback,
                uint32_t other_data
        );

static const struct wl_callback_listener sync_callback_listener = {
        .done = sync_handle_done,
};

static void sync_handle_done
        (
                void *data,
                struct wl_callback *wl_callback,
                uint32_t other_data
        ) {
    static int sync = 0;
    if (debug_log)
        fprintf(stderr, "[Sync callback: %d]\n", sync);
    
    wl_callback_destroy(wl_callback);
    sync_callback = NULL;
    
    if (sync == 0) {
        /* First sync: The registry finished advertising globals.
         * Now we can check whether we have everything we need.
         */
        if (force_protocol) {
            switch (used_protocol) {
                case ZWLR_FOREIGN_TOPLEVEL:
                    if (zwlr_toplevel_manager == NULL) {
                        fputs("ERROR: Wayland server does not support zwlr-foreign-toplevel-management-unstable-v1 version 3.\n",
                              stderr);
                        ret = EXIT_FAILURE;
                        loop = false;
                        return;
                    }
                    break;
                
                case EXT_FOREIGN_TOPLEVEL:
                    if (ext_toplevel_list == NULL) {
                        fputs("ERROR: Wayland server does not support ext-foreign-toplevel-list-v1 version 1.\n",
                              stderr);
                        ret = EXIT_FAILURE;
                        loop = false;
                        return;
                    }
                    break;
                
                case NONE: /* Unreachable. */
                    assert(false);
                    break;
            }
        } else if (ext_toplevel_list != NULL)
            used_protocol = EXT_FOREIGN_TOPLEVEL;
        else if (zwlr_toplevel_manager != NULL)
            used_protocol = ZWLR_FOREIGN_TOPLEVEL;
        else {
            const char *err_message =
                    "ERROR: Wayland server supports none of the protocol extensions required for getting toplevel information:\n"
                    "    -> zwlr-foreign-toplevel-management-unstable-v1, version 3 or higher\n"
                    "    -> ext-foreign-toplevel-list-v1, version 1 or higher\n"
                    "\n";
            fputs(err_message, stderr);
            ret = EXIT_FAILURE;
            loop = false;
            return;
        }
        update_capabilities();
        
        sync++;
        sync_callback = wl_display_sync(wl_display);
        wl_callback_add_listener(sync_callback, &sync_callback_listener, NULL);
        
        // TODO if there are extension protocol for ext_foreign_toplevel_list
        //      to get extra information, we may need one additional sync.
        //      So check if any of those are bound and then add one step
        //      if necessary.
    } else if (mode == LIST) {
        /* Second sync: Now we have received all toplevel handles and
         * their events. Time to leave the main loop, print all data and
         * exit.
         */
        loop = false;
    }
}

static void dump_and_free_data(void) {
    assert(mode == LIST);
    struct Toplevel *t, *tmp;
    wl_list_for_each_reverse_safe(t, tmp, &toplevels, link) {
        toplevel_destroy(t);
    }
}

static void free_data(void) {
    /* If we are in WATCH mode, destroying a toplevel will print a message
     * to the user. However, in this case that message would be wrong,
     * because the toplevel isn't closed, we just destroy our internal
     * representation of it. So set mode to LIST for cleanup.
     */
    mode = LIST;
    
    struct Toplevel *t, *tmp;
    wl_list_for_each_safe(t, tmp, &toplevels, link)toplevel_destroy(t);
}

static void handle_interrupt(int signum) {
    fputs("Killed.\n", stderr);
    loop = false;
    
    /* In WATCH mode, Ctrl-C is the expected way to exit lswt, so don't
     * set the return value to EXIT_FAILURE. However in LIST mode we
     * generally don't expect SIGINT, so we probably encountered an error
     * and should set the return value accordingly.
     */
    if (mode == LIST)
        ret = EXIT_FAILURE;
    
    longjmp(skip_main_loop, 1);
}

/**
 * Intercept error signals (like SIGSEGV and SIGFPE) so that we can try to
 * print a fancy error message and a backtracke before letting the system kill us.
 */
static void handle_error(int signum) {
    /* Set up the default handlers to deal with the rest. We do this before
     * attempting to get a backtrace, because sometimes that could also
     * cause a SEGFAULT and we don't want a funny signal loop to happen.
     */
    signal(signum, SIG_DFL);

#ifdef __linux__
#ifdef __GLIBC__
    fputs("Attempting to get backtrace:\n", stderr);
    void *buffer[255];
    const int calls = backtrace(buffer, sizeof(buffer) / sizeof(void *));
    backtrace_symbols_fd(buffer, calls, fileno(stderr));
    fputs("\n", stderr);
#endif
#endif
    
    /* Easiest way of calling the default signal handler. */
    kill(getpid(), signum);
}

#ifdef __linux__

static void lock_the_land(void) {
    /* For ABI versions 1 to 3 repsectively. */
    static uint64_t landlock_access_rights[max_supported_landlock_abi] = {
            (1ULL << 13) - 1, (1ULL << 14) - 1, (1ULL << 15) - 1
    };
    
    /* Query for supported ABI, if any. */
    long int abi = syscall(
            SYS_landlock_create_ruleset,
            NULL, 0, LANDLOCK_CREATE_RULESET_VERSION
    );
    
    if (abi < 0) {
        /* Landlock unsupported or disabled. */
        if (errno == ENOSYS || errno == EOPNOTSUPP)
            return;
        
        fprintf(stderr, "ERROR: landlock_create_ruledset: %s\n", strerror(errno));
        return;
    }
    
    if (abi > max_supported_landlock_abi)
        abi = max_supported_landlock_abi;
    
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) < 0) {
        fprintf(stderr, "ERROR: prctl: %s\n", strerror(errno));
        return;
    }
    
    struct landlock_ruleset_attr attr = {
            .handled_access_fs = landlock_access_rights[abi - 1],
    };
    
    int ruleset_fd = (int) syscall(
            SYS_landlock_create_ruleset,
            &attr, sizeof(attr), 0
    );
    if (ruleset_fd < 0) {
        fprintf(stderr, "ERROR: landlock_create_ruleset: %s\n", strerror(errno));
        return;
    }
    
    if (syscall(SYS_landlock_restrict_self, ruleset_fd, 0) != 0)
        fprintf(stderr, "ERROR: landlock_restrict_self: %s\n", strerror(errno));
    
    close(ruleset_fd);
}

#endif

int main(int argc, char *argv[]) {
    open_x_connection();
    
    signal(SIGSEGV, handle_error);
    signal(SIGFPE, handle_error);
    signal(SIGINT, handle_interrupt);

#ifdef __linux__
    lock_the_land();
#endif
    const char *display_name = getenv("WAYLAND_DISPLAY");
    
    used_protocol = ZWLR_FOREIGN_TOPLEVEL;
    
    if ((mode == WATCH || mode == VERBOSE_WATCH) && output_format != NORMAL) {
        fputs("ERROR: Alternative output formats are not supported in watch mode.\n", stderr);
        ret = EXIT_FAILURE;
        goto cleanup;
    }
    
    
    /* We query the display name here instead of letting wl_display_connect()
     * figure it out itself, because libwayland (for legacy reasons) falls
     * back to using "wayland-0" when $WAYLAND_DISPLAY is not set, which is
     * generally not desirable.
     */
    if (display_name == NULL) {
        fputs("ERROR: WAYLAND_DISPLAY is not set.\n", stderr);
        ret = EXIT_FAILURE;
        goto cleanup;
    }
    
    if (debug_log)
        fprintf(stderr, "[Trying to connect to display: '%s']\n", display_name);
    
    /* Behold: If this succeeds, we may no longer goto cleanup, because
     * Wayland magic happens, which can cause Toplevels to be allocated.
     */
    wl_display = wl_display_connect(display_name);
    if (wl_display == NULL) {
        fprintf(stderr, "ERROR: Can not connect to wayland display: %s\n", strerror(errno));
        ret = EXIT_FAILURE;
        goto cleanup;
    }
    
    wl_list_init(&toplevels);
    wl_registry = wl_display_get_registry(wl_display);
    wl_registry_add_listener(wl_registry, &registry_listener, NULL);
    
    sync_callback = wl_display_sync(wl_display);
    wl_callback_add_listener(sync_callback, &sync_callback_listener, NULL);
    
    if (debug_log)
        fputs("[Entering main loop.]\n", stderr);
    if (setjmp(skip_main_loop) == 0)
        while (loop && wl_display_dispatch(wl_display) > 0);
    
    stop_x_connection();
    /* If nothing went wrong in the main loop we can print and free all data,
     * otherwise just free it.
     */
    if (ret == EXIT_SUCCESS && mode == LIST)
        dump_and_free_data();
    else
        free_data();
    
    if (debug_log)
        fputs("[Cleaning up Wayland interfaces.]\n", stderr);
    if (sync_callback != NULL)
        wl_callback_destroy(sync_callback);
    if (zwlr_toplevel_manager != NULL)
        zwlr_foreign_toplevel_manager_v1_destroy(zwlr_toplevel_manager);
    if (ext_toplevel_list != NULL)
        ext_foreign_toplevel_list_v1_destroy(ext_toplevel_list);
    if (wl_registry != NULL)
        wl_registry_destroy(wl_registry);
    wl_display_disconnect(wl_display);

cleanup:
    if (custom_output_format != NULL)
        free(custom_output_format);
    
    return ret;
}
