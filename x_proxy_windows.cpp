//
// Created by jmanc3 on 6/11/25.
//

#include "x_proxy_windows.h"

#include "main.h"

#include <thread>
#include <cstdio>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xutil.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/Xfixes.h>
#include <unistd.h>     // for pipe(), read(), write()
#include <fcntl.h>      // for fcntl()
#include <sys/poll.h>
#include <vector>
#include <mutex>
#include <iostream>

Display *display;
int wakeup_pipe[2];
std::mutex mutex;

struct FutureWork {
    void (*func)(FutureWork *w) = nullptr;
    Toplevel *top_level = nullptr;
    int id = 0;
    std::string new_title;
    Atom wm_delete;
};

std::vector<FutureWork *> queued_work;

std::string proxy_tag = "[PROXY]";

int x_main() {
    display = XOpenDisplay(NULL);
    if (pipe(wakeup_pipe) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }
    
    
    // Set read end non-blocking (optional)
    int flags = fcntl(wakeup_pipe[0], F_GETFL, 0);
    fcntl(wakeup_pipe[0], F_SETFL, flags | O_NONBLOCK);
    
    // This returns the FD of the X11 display (or something like that)
    int x11_fd = ConnectionNumber(display);
    
    int MAX_POLLING_EVENTS_AT_THE_SAME_TIME = 1000;
    pollfd fds[MAX_POLLING_EVENTS_AT_THE_SAME_TIME];
    
    std::vector<int> descriptors_being_polled;
    descriptors_being_polled.push_back(x11_fd);
    descriptors_being_polled.push_back(wakeup_pipe[0]);
    
    
    int BUFFER_SIZE = 400;
    char buffer[BUFFER_SIZE];
    XEvent event;
    
    Atom wm_delete = XInternAtom(display, "WM_DELETE_WINDOW", False);
    
     // Main loop
    while(1) {
        memset(fds, 0, sizeof(fds));
        for (int i = 0; i < descriptors_being_polled.size(); i++) {
            fds[i].fd = descriptors_being_polled[i];
            fds[i].events = POLLIN;
        }
        
        // Wait for X Event or a Timer
        int num_ready_fds = poll(fds, descriptors_being_polled.size(), -1);
        printf("woke up\n");
        if (num_ready_fds < 0) {
            perror("error in main poll loop\n");
            exit(1);
        }
        std::lock_guard<std::mutex> lock(mutex);
        
        for (int i = descriptors_being_polled.size() - 1; i >= 0; i--) {
            if (fds[i].revents & POLLIN) {
                if (fds[i].fd == wakeup_pipe[0]) {
                    printf("read from wakeup pipe\n");
                    read(wakeup_pipe[0], buffer, BUFFER_SIZE);
                }
            }
        }
        // Handle XEvents and flush the input
        while(XPending(display)) {
            XNextEvent(display, &event);
            printf("xevent type: %d\n", event.type);
            if (event.type == FocusIn) {
                activate_toplevel(event.xfocus.window);
            } else if (event.type == ClientMessage) {
                if ((Atom)event.xclient.data.l[0] == wm_delete) {
                    printf("handle close\n");
                    close_toplevel(event.xfocus.window);
                    break;
                }
            }
        }
        
        for (int i = 0; i < queued_work.size(); i++) {
            auto work = queued_work[i];
            if (work->func) {
                work->wm_delete = wm_delete;
                work->func(work);
            }
            delete work;
        }
        if (!queued_work.empty())
            queued_work.clear();
        
        XFlush(display);
        
    }
    
    return 0;
}

void wakeup() {
    const char* msg = "wakeup\n";
    write(wakeup_pipe[1], msg, strlen(msg));
}

void open_x_connection() {
    std::thread t(x_main);
    t.detach();
}

/*
 ***************************************************
 */


Window create_argb_window(Display *display, int screen, int x, int y, int width, int height) {
    XVisualInfo vinfo;
    if (!XMatchVisualInfo(display, screen, 32, TrueColor, &vinfo)) {
        fprintf(stderr, "No 32-bit TrueColor visual found\n");
        exit(1);
    }
    
    XSetWindowAttributes attr;
    attr.colormap = XCreateColormap(display, RootWindow(display, screen), vinfo.visual, AllocNone);
    attr.background_pixel = 0x00000000;  // fully transparent
    attr.border_pixel = 0;
    attr.override_redirect = False; // optional
    
    Window win = XCreateWindow(
            display,
            RootWindow(display, screen),
            x, y, width, height,
            0,
            vinfo.depth,
            InputOutput,
            vinfo.visual,
            CWColormap | CWBackPixel | CWBorderPixel | CWOverrideRedirect,
            &attr
    );
    
    return win;
}


void make_window_click_through(Display *display, Window win) {
    // Create an empty region
    XserverRegion region = XFixesCreateRegion(display, NULL, 0);
    
    // Apply it as the input region (not the bounding region!)
    XFixesSetWindowShapeRegion(display, win, ShapeInput, 0, 0, region);
    
    // Clean up
    XFixesDestroyRegion(display, region);
    XFlush(display);
}

void force_window_position(Display *display, Window win, int x, int y) {
    XSizeHints *size_hints = XAllocSizeHints();
    if (!size_hints) return;
    
    size_hints->flags = PPosition;
    size_hints->x = x;
    size_hints->y = y;
    
    XSetWMNormalHints(display, win, size_hints);
    XFree(size_hints);
    
    // Also move it in case the WM doesn't use hints
    XMoveWindow(display, win, x, y);
    XFlush(display);
}

typedef struct {
    unsigned long flags;
    unsigned long functions;
    unsigned long decorations;
    long inputMode;
    unsigned long status;
} MotifWmHints;

void disable_decorations(Display *display, Window win) {
    Atom property = XInternAtom(display, "_MOTIF_WM_HINTS", False);
    
    MotifWmHints hints;
    hints.flags = 2;           // MWM_HINTS_DECORATIONS
    hints.functions = 0;
    hints.decorations = 0;     // 0 means disable all decorations
    hints.inputMode = 0;
    hints.status = 0;
    
    XChangeProperty(
            display,
            win,
            property,
            property,
            32,
            PropModeReplace,
            (unsigned char *)&hints,
            sizeof(MotifWmHints) / 4 // number of 32-bit elements
    );
    XFlush(display);
}

// Sets WM_CLASS to "stackingname" for both instance and class
void set_wm_class(Display *display, Window win, const char *stackingname) {
    // WM_CLASS is two null-terminated strings concatenated
    // Allocate buffer for "stackingname\0stackingname\0"
    size_t len = strlen(stackingname);
    char *wm_class = static_cast<char *>(malloc(len * 2 + 2));
    if (!wm_class) {
        fprintf(stderr, "Failed to allocate memory for WM_CLASS\n");
        return;
    }
    strcpy(wm_class, stackingname);
    wm_class[len] = '\0';
    strcpy(wm_class + len + 1, stackingname);
    wm_class[len * 2 + 1] = '\0';
    
    Atom wm_class_atom = XInternAtom(display, "WM_CLASS", False);
    XChangeProperty(
            display,
            win,
            wm_class_atom,
            XA_STRING,
            8,                 // format: 8 bits per element
            PropModeReplace,
            (unsigned char *)wm_class,
            (int)(len * 2 + 2) // total length including both null terminators
    );
    XFlush(display);
    free(wm_class);
}


// Sets the window title using XStoreName
void set_window_title(Display *display, Window win, std::string title) {
    XStoreName(display, win, title.c_str());
    XFlush(display);
}

// Sets a custom atom property "IS_WAYLAND_TOPLEVEL" of type INTEGER with value 1
void set_custom_atom(Display *display, Window win) {
    Atom atom = XInternAtom(display, "IS_WAYLAND_TOPLEVEL_PROXY", False);
    
    // We store an integer value 1 in that property
    int value = 1;
    XChangeProperty(
            display,
            win,
            atom,
            XA_INTEGER,
            32,               // format: 32-bit
            PropModeReplace,
            (unsigned char *)&value,
            1                 // number of elements
    );
    XFlush(display);
}



std::string get_window_title(Display* display, Window window) {
    Atom net_wm_name = XInternAtom(display, "_NET_WM_NAME", False);
    Atom utf8_string = XInternAtom(display, "UTF8_STRING", False);
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char* prop = nullptr;
    
    if (XGetWindowProperty(display, window, net_wm_name, 0, (~0L), False,
                           utf8_string, &actual_type, &actual_format,
                           &nitems, &bytes_after, &prop) == Success) {
        if (prop) {
            std::string title(reinterpret_cast<char*>(prop));
            XFree(prop);
            return title;
        }
    }
    
    // Fallback to WM_NAME
    char* name = nullptr;
    if (XFetchName(display, window, &name) > 0 && name) {
        std::string title(name);
        XFree(name);
        return title;
    }
    
    return "";
}

std::vector<Window> get_window_stack(Display* display, Window root) {
    Atom atom = XInternAtom(display, "_NET_CLIENT_LIST_STACKING", True);
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char* prop = nullptr;
    std::vector<Window> windows;
    
    if (XGetWindowProperty(display, root, atom, 0, (~0L), False,
                           XA_WINDOW, &actual_type, &actual_format,
                           &nitems, &bytes_after, &prop) == Success) {
        if (prop) {
            Window* list = reinterpret_cast<Window*>(prop);
            for (unsigned long i = 0; i < nitems; ++i) {
                windows.push_back(list[i]);
            }
            XFree(prop);
        }
    }
    
    return windows;
}



void create_proxy_for(Toplevel *top_level) {
    // TODO: we need a mutex on the queued work
    if (top_level) {
        if (top_level->title.empty()) {
            return;
        }
    }
    
    auto work = new FutureWork;
    work->func = [](FutureWork *w) {
        // TODO: if there exists already an x window with the same title
        //  we then assume the toplevel is xwayland surface and we then don't need to do this
        
        Window root = DefaultRootWindow(display);
        std::vector<Window> stack = get_window_stack(display, root);
        
        for (Window win : stack) {
            std::string title = get_window_title(display, win);
            std::cout << "Window ID: " << win << " Title: " << title << "\n";
            
            if (title == w->top_level->title) {
                std::cout << "Found window with title 'asdf': " << win  << "\n";
                return;
            }
        }
        
        auto top_level = w->top_level;
        int screen = DefaultScreen(display);
        
        Window my_window = create_argb_window(display, screen, 0, 1, 1, 1);
        XSetWMProtocols(display, my_window, &w->wm_delete, 1);
        XSelectInput(display, my_window, StructureNotifyMask | FocusChangeMask );
        top_level->x11_proxy_window_id = my_window;
        
        printf("%d\n", my_window);
        
        // Set title and custom atom
        std::string t;
        if (top_level->title.empty()) {
            t = proxy_tag;
        } else {
            t = top_level->title + " " + proxy_tag;
        }
        set_window_title(display, my_window, t);
        top_level->old_title = t;
        set_custom_atom(display, my_window);
        set_wm_class(display, my_window, top_level->app_id.c_str());
        XMapWindow(display, my_window);
        force_window_position(display, my_window, 0, 1);
        make_window_click_through(display, my_window);
        disable_decorations(display, my_window);
        XFlush(display);
    };
    work->top_level = top_level;
    std::lock_guard<std::mutex> lock(mutex);
    queued_work.push_back(work);
    wakeup();
}

void update_title_for(Toplevel *top_level) {
    if (top_level->x11_proxy_window_id == 0)
        return;
    auto work = new FutureWork;
    work->func = [](FutureWork *w) {
        printf("set the title: %s\n", w->new_title.c_str());
        printf("set the title: %s\n", w->new_title.c_str());
        if (w->new_title.empty()) {
            set_window_title(display, w->id, proxy_tag);
        } else {
            set_window_title(display, w->id, w->new_title + " " + proxy_tag);
        }
        XFlush(display);
    };
    work->top_level = top_level;
    work->id = top_level->x11_proxy_window_id;
    work->new_title = top_level->title;
    std::lock_guard<std::mutex> lock(mutex);
    queued_work.push_back(work);
    wakeup();
}

void destroy_proxy_for(Toplevel *top_level) {
    if (top_level->x11_proxy_window_id == 0)
        return;
    auto work = new FutureWork;
    work->func = [](FutureWork *w) {
        XDestroyWindow(display, w->id);
        XFlush(display);
    };
    work->top_level = top_level;
    work->id = top_level->x11_proxy_window_id;
    std::lock_guard<std::mutex> lock(mutex);
    queued_work.push_back(work);
    wakeup();
}

void stop_x_connection() {

}