#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xutil.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/Xfixes.h>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrender.h>
#include <stdio.h>
#include <stdlib.h>

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
    char *wm_class = malloc(len * 2 + 2);
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
void set_window_title(Display *display, Window win, const char *title) {
    XStoreName(display, win, title);
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

int main() {
    Display *display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Failed to open X display\n");
        return 1;
    }

    int screen = DefaultScreen(display);
    Window root = RootWindow(display, screen);

	Window my_window = create_argb_window(display, screen, 0, 1, 1, 1);
    printf("%d\n", my_window);

    // Set title and custom atom
    set_window_title(display, my_window, "My Override Redirect Window");
    set_custom_atom(display, my_window);
    set_wm_class(display, my_window, "firefox");
    XMapWindow(display, my_window);
    force_window_position(display, my_window, 0, 1);
    make_window_click_through(display, my_window);
    disable_decorations(display, my_window);

    // Select for MapRequest events on the root window
    XSelectInput(display, root, SubstructureNotifyMask);

    printf("Listening for MapRequest events. Press Ctrl+C to quit.\n");

    XEvent event;
    while (1) {
        XNextEvent(display, &event);
        printf("here we %d\n", event.type);

        if (event.type == MapNotify) {
            XMapRequestEvent *map_event = (XMapRequestEvent *)&event;
            printf("MapRequest for window: 0x%lx\n", map_event->window);

            // Do NOT map the window – we ignore the request
        }
    }

    XCloseDisplay(display);
    return 0;
}

