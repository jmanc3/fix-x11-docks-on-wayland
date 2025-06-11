#!/usr/bin/env bash

#exit when any of the following commands fails
set -e

#make and enter the build directory if it doesn't exist already
mkdir -p newbuild
cd newbuild

#let cmake find dependencies on system
# NEEDS TO BE SUDO SO IT CAN INSTALL RESOURCES
cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_POLICY_VERSION_MINIMUM=3.5 ../

#actually compile
make -j 16

echo "Trying to install to /usr/bin/fix_x11_docks"

sudo mkdir -p /usr/bin
sudo make -j 16 install

