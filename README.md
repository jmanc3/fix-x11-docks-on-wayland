# Fix X11 docks, in wayland

This program watches for the creation of wayland toplevel windows, and then creates an X11 proxy so that X11 docks can see wayland windows. When the proxy x11 is invisible visually but should be able to be picked up by X11 docks. When a proxy window receives focus, it means the wayland toplevel window should be brought to focus, and we do that.

Similar to what snixembed did. 

## Packages required for building

* Void Linux

```bash
sudo xbps-install -S git gcc cmake make pkg-config libxcb-devel libXfixes-devel libX11-devel
```

## Installation

* Download the source and enter the folder

```bash
git clone https://github.com/jmanc3/fix-x11-docks-on-wayland
cd fix-x11-docks-on-wayland
```

* Run the install script.

```bash
./install.sh
``` 
