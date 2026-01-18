#include "wm.hpp"
#include "fmt/base.h"
#include "fmt/format.h"
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

// basic floating on x11 cuz why not

// todo:
// - [x] not crash
// - [x] basic focus on left click
// - [ ] focus next / previous clients on mod + tab / mod + shift + tab
// - [x] move / resize
// - [ ] learn how to do decorations (repareting)
// - [ ] implement basic ICCCM stuff
// - [ ] implement basic EWMH stuff

// goals:
// - [x] open basic apps (kitty, vkcube)
// - [x] move / resize clients
// - [ ] ICCCM
// - [ ] the bare minimium EWMH stuff (fullscreen, maximized, minimized)

#define LEFT_BUTTON 1
#define RIGHT_BUTTON 3
#define MOD Mod4Mask

int error_handler(Display* display, XErrorEvent* e) {
    char error_text[1024];
    XGetErrorText(display, e->error_code, error_text, sizeof(error_text));

    fmt::println("X Error: {}", error_text);
    fmt::println("Request: {}", e->request_code);
    fmt::println("Resource: {:#x}", e->resourceid);

    return 0;
}

void wm::run() {
    fmt::println(fmt::format("using display: {}", std::getenv("DISPLAY")));
    display = XOpenDisplay(std::getenv("DISPLAY"));

    if (display == nullptr) {
        throw fmt::system_error(1, "failed to open DISPLAY");
    }

    XSetErrorHandler(error_handler);

    root = DefaultRootWindow(display);
    XSync(display, False);

    // register substructredit on root
    XSelectInput(display, root, SubstructureRedirectMask | SubstructureNotifyMask);

    // mod + left for motion (move)
    XGrabButton(display, LEFT_BUTTON, MOD, DefaultRootWindow(display), True,
            ButtonPressMask|ButtonReleaseMask|PointerMotionMask, GrabModeAsync, GrabModeAsync, None, None);

    // mod + left for motion (resize)
    XGrabButton(display, RIGHT_BUTTON, MOD, DefaultRootWindow(display), True,
            ButtonPressMask|ButtonReleaseMask|PointerMotionMask, GrabModeAsync, GrabModeAsync, None, None);

    start.subwindow = None;

    // event loop
    for (;;) {
        // wait until next event
        XEvent e;
        XNextEvent(display, &e);

        switch (e.type) {
            // core events
            case MapRequest:
                on_map_request(e.xmaprequest);
                break;
            case UnmapNotify:
                on_map_notify(e.xunmap);
                break;
            case ConfigureRequest:
                on_configure_request(e.xconfigurerequest);
                break;
            case ConfigureNotify:
                on_configure_notify(e.xconfigure);
                break;
            case MotionNotify:
                on_motion_notify(e);
                break;

            // button events
            case ButtonPress:
                if (e.xbutton.state & MOD) {
                    XGetWindowAttributes(display, e.xbutton.subwindow, &attr);
                    start = e.xbutton;
                } else {
                    raise_window(e.xbutton.window);
                }
                XAllowEvents(display, ReplayPointer, CurrentTime);
                break;
            case ButtonRelease:
                start.subwindow = None;
                break;
        }
    }

    close();
}

void wm::focus_window(Window w) {
    XSetInputFocus(display, w, RevertToPointerRoot, CurrentTime);
    focused_window = w;
}

void wm::raise_window(Window w) {
    XRaiseWindow(display, w);

    // re order client list
    auto it = std::find(clients.begin(), clients.end(), w);

    if (it != clients.end()) {
        clients.erase(it);
        clients.push_back(w);
    }

    focus_window(w);
}

void wm::on_motion_notify(XEvent &event) {
    // todo: render outline instead of resizing the entire window every single time
    // tofix: we're not waiting until the last event
    while (XCheckTypedEvent(display, MotionNotify, &event));

    int xdiff = event.xbutton.x_root - start.x_root;
    int ydiff = event.xbutton.y_root - start.y_root;

    int new_x = attr.x;
    int new_y = attr.y;
    int new_w = attr.width;
    int new_h = attr.height;

    // if we're just gonna move, update the w / h
    if (start.button == LEFT_BUTTON) {
        new_x += xdiff;
        new_y += ydiff;
    } else if (start.button == RIGHT_BUTTON) {
        new_w += xdiff;
        new_h += ydiff;

        // get size hints
        XSizeHints hints;
        long supplited;
        XGetWMNormalHints(display, start.subwindow, &hints, &supplited);

        // apply mix / max constraits if needed
        if (hints.flags & PMinSize) {
            new_w = std::min(new_w, hints.min_width);
            new_h = std::min(new_h, hints.min_height);
        } else if (hints.flags & PMaxSize) {
            new_w = std::max(new_w, hints.min_width);
            new_h = std::max(new_h, hints.min_height);
        }

        // apply increments changes if needed
        if (hints.flags & PResizeInc && hints.width_inc > 0 && hints.height_inc > 0) {
            new_w -= hints.base_width;
            new_h -= hints.base_height;
            new_w = (new_w / hints.width_inc) * hints.width_inc + hints.base_width;
            new_h = (new_h / hints.height_inc) * hints.height_inc + hints.base_height;
        }
    }

    XMoveResizeWindow(display, start.subwindow, new_x, new_y, new_w, new_h);
}

void wm::on_map_request(const XMapRequestEvent& event) {
    // add left click button event (focus)
    XGrabButton(display, AnyButton, 0, event.window, False,
                ButtonPressMask, GrabModeSync, GrabModeAsync, None, None);

    // todo: repareting so i can place ts inside a frame or something
    XMapWindow(display, event.window);

    raise_window(event.window);
}

void wm::on_map_notify(const XUnmapEvent &event) {
    // todo: remove from client lists (when i have a client lists)
}

void wm::on_configure_request(const XConfigureRequestEvent &event) {
    fmt::println(fmt::format("x: {}, y: {}", event.x, event.y));

    XWindowChanges changes;
    changes.x = event.x;
    changes.y = event.y;
    changes.width = event.width;
    changes.height = event.height;
    XConfigureWindow(display, event.window, event.value_mask, &changes);
};

void wm::on_configure_notify(const XConfigureEvent &event) {
    // todo: i have no idea what i am supposed to do here
}

void wm::close() {
    XCloseDisplay(display);
}
