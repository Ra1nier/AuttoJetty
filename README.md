# AuttoJetty

Linux/X11 implementation of the JettyBoot screen bot.

## Build

Install OpenCV, libtorch, X11, and XTest development packages, then build with CMake. On Debian/Ubuntu, the system packages include `libopencv-dev`, `libx11-dev`, and `libxtst-dev`; libtorch still needs to be downloaded separately from PyTorch.

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/libtorch
cmake --build build
```

Run from an X11 or XWayland session:

```sh
./build/AuttoJetty
```
