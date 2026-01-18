#pragma once
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include "common.hpp"

namespace wm {
    // core
    void run();
    void close();

    // wm core
    void focus_window(Window w);
    void raise_window(Window w);

    // event handlers
    void on_map_request(const XMapRequestEvent& event);
    void on_configure_request(const XConfigureRequestEvent& event);
    void on_map_notify(const XUnmapEvent& event);
    void on_motion_notify(XEvent& event);
    void on_configure_notify(const XConfigureEvent& event);

    inline Display* display;
    inline Window root;

    // clients
    inline std::vector<Window> clients;
    inline Window focused_window;

    // resize stuff
    inline XButtonEvent start;
    inline XWindowAttributes attr;
};
