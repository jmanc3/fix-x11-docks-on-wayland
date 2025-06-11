//
// Created by jmanc3 on 6/11/25.
//

#ifndef FIX_X11_DOCKS_ON_WAYLAND_X_PROXY_WINDOWS_H
#define FIX_X11_DOCKS_ON_WAYLAND_X_PROXY_WINDOWS_H

#include "main.h"

void open_x_connection();

void stop_x_connection();


void create_proxy_for(Toplevel *topLevel);

void update_title_for(Toplevel *topLevel);

void destroy_proxy_for(Toplevel *toplevel);



#endif //FIX_X11_DOCKS_ON_WAYLAND_X_PROXY_WINDOWS_H
