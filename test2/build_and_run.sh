#!/bin/bash
#
#gcc -o map_listener map_listener.c $(pkg-config --cflags --libs xfixes x11)
gcc -o map_listener map_listener.c -lX11 -lXfixes
./map_listener

