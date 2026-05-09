# AuttoJetty

Linux Wayland implementation of the JettyBoot screen bot.

## Build

Install OpenCV, libtorch, the desktop portal wrapper, and PipeWire development files. Fedora examples:

```sh
sudo dnf install opencv-devel libportal-devel pipewire-devel glib2-devel
```

Capture uses the GNOME/KDE desktop portal and PipeWire. Key injection uses `/dev/uinput`, so your user needs permission to write to that device.

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/libtorch
cmake --build build
```

Run from a Wayland session:

```sh
./build/AuttoJetty
```
