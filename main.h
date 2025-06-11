//
// Created by jmanc3 on 6/11/25.
//

#ifndef FIX_X11_DOCKS_ON_WAYLAND_MAIN_H
#define FIX_X11_DOCKS_ON_WAYLAND_MAIN_H

#include <string>
#include <wayland-util.h>

/******************
 *                *
 *    Toplevel    *
 *                *
 ******************/
class Toplevel {
public:
    struct wl_list link;
    
    /** Internal id, used in WATCH mode. */
    size_t id;
    
    int x11_proxy_window_id = 0;
    
    struct zwlr_foreign_toplevel_handle_v1 *zwlr_handle;
    struct ext_foreign_toplevel_handle_v1 *ext_handle;
    
    std::string old_title;
    std::string title;
    std::string app_id;
    
    /**
     * Optional data. Whether these are supported depends on the bound
     * protocol(s). See update_capabilities() and related globals.
     */
    std::string identifier;
    bool fullscreen;
    bool activated;
    bool maximized;
    bool minimized;
    
    /**
     * True if this toplevel has already been added to the list, false
     * otherwise. Used to prevent accidentally appending the same toplevel
     * multiple times if toplevel_handle_done is called more than once.
     */
    bool listed;
};

void activate_toplevel(int window);

void close_toplevel(int window);


#endif //FIX_X11_DOCKS_ON_WAYLAND_MAIN_H
