#!/bin/bash
#
# This is how you generate .c and .h files from the wayland_protocol.xml files
#

wayland-scanner private-code < wlr-foreign-toplevel-management-unstable-v1.xml > wlr-foreign-toplevel-management-unstable-v1.c

wayland-scanner client-header < wlr-foreign-toplevel-management-unstable-v1.xml > wlr-foreign-toplevel-management-unstable-v1.h

wayland-scanner private-code < ext-foreign-toplevel-list-v1.xml > ext-foreign-toplevel-list-v1.c

wayland-scanner client-header < ext-foreign-toplevel-list-v1.xml > ext-foreign-toplevel-list-v1.h
