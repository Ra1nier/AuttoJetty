# AuttoJetty

AuttoJetty is a Linux/Wayland screen bot for JettyBoot. It captures gameplay through the desktop screencast portal, detects the player and obstacles with OpenCV, and sends jump input through Linux `uinput`.

The project provides a reliable rule-based controller and an experimental LibTorch-powered AI mode with optional training and policy persistence.

This project demonstrates real-time computer vision, native Linux desktop integration, virtual input, and online imitation learning in modern C++.

> [!IMPORTANT]
> AuttoJetty is currently Linux-only and requires a Wayland desktop session. The computer-vision thresholds are tuned to the game's current colors and layout, so different scaling, filters, or visual settings may require code changes.

## Features

- Native Wayland screen capture through XDG Desktop Portal and PipeWire
- Interactive game and status-region calibration before each run
- OpenCV detection for the player, pillars, gaps, and remaining lives
- Rule-based controller that accounts for the target gap and vertical movement
- Experimental neural-network inference and training with LibTorch
- Automatic round restart when no lives remain
- Live detection preview for calibration and debugging
- Automatic build-and-launch script with support for custom build directories

## How it works

```text
XDG Desktop Portal → PipeWire frames → OpenCV detection → controller / neural policy → uinput
```

The capture layer requests a user-approved Wayland screen stream and converts PipeWire buffers into OpenCV frames. The vision layer isolates the boot, pillars, gaps, and life indicators using color masks and contour filtering. A deterministic controller or a small LibTorch model then decides when to emit the game's jump key through a virtual keyboard.

The AI mode uses the deterministic controller as a teacher. It stores a bounded replay buffer, trains on randomized mini-batches, clips gradients, and versions saved policies so incompatible checkpoints fail safely.

## Requirements

- Linux with a Wayland desktop environment, such as GNOME or KDE Plasma
- A working XDG Desktop Portal screencast backend
- A C++17 compiler
- CMake 3.18 or newer
- OpenCV development files
- LibTorch
- libportal, PipeWire, GLib, and GIO development files
- Write access to `/dev/uinput`

### Fedora dependencies

Install the packaged build dependencies with:

```bash
sudo dnf install \
  cmake gcc-c++ pkgconf-pkg-config \
  opencv-devel libportal-devel pipewire-devel glib2-devel
```

Download the LibTorch distribution appropriate for your system from the [PyTorch website](https://pytorch.org/get-started/locally/) and extract it to a convenient location. Choose the CPU build unless you specifically need CUDA support and already have a compatible CUDA toolchain.

If CUDA on Fedora rejects the default GCC version, install GCC 15:

```bash
sudo dnf install gcc15 gcc15-c++
```

The launch script automatically selects `/usr/bin/g++-15` when it is available.

## Installation

1. Clone the repository:

   ```bash
   git clone https://github.com/Ra1nier/AuttoJetty.git
   cd AuttoJetty
   ```

2. Configure CMake with the extracted LibTorch directory:

   ```bash
   cmake -S . -B build -DCMAKE_PREFIX_PATH=/absolute/path/to/libtorch
   ```

3. Build the application:

   ```bash
   cmake --build build --parallel
   ```

The executable will be created at `build/AuttoJetty`.

### Configure input permissions

AuttoJetty creates a virtual keyboard through `/dev/uinput`. Confirm that the module and device are available:

```bash
sudo modprobe uinput
ls -l /dev/uinput
```

Your user must have permission to write to that device. Configure an appropriate udev rule or group assignment for your distribution, then sign out and back in so the new group membership takes effect. Avoid running AuttoJetty with `sudo`; desktop portal access normally belongs to your logged-in desktop session.

## Usage

Start JettyBoot and place its window where it will remain during the run. Then launch AuttoJetty from the repository root:

```bash
./launch.sh
```

`launch.sh` builds the project when the executable is missing or has broken shared-library links, changes into the build directory, and starts the application. To use another build directory:

```bash
AUTTOJETTY_BUILD_DIR=/absolute/path/to/build ./launch.sh
```

### Startup walkthrough

1. Enter the screen width and height when prompted (`1920x1080` is shown as the suggested resolution).
2. Choose a decision mode:
   - `c` — rule-based controller
   - `a` — experimental AI mode
3. In AI mode, choose whether to restore, train, and save the policy.
4. Approve screen capture in the desktop portal dialog and select the monitor or window containing the game.
5. Check the colored rectangles in the **Capture Region Setup** window:
   - Orange: gameplay area
   - Magenta: status/lives area
6. Adjust the regions if necessary, then begin capture.

Capture-region controls:

| Key | Action |
| --- | --- |
| `s` or any unassigned key | Start the bot |
| `e` | Edit region coordinates in the terminal |
| `r` | Refresh the preview |
| `q` | Quit setup |

Runtime controls:

| Key | Action |
| --- | --- |
| `j` | Send a manual test jump |
| `q` | Quit AuttoJetty |

AuttoJetty sends the `E` key to jump. Keep the game focused and use the **Detected Objects** window to verify that the boot, pillars, target gap, and lives are being recognized correctly.

## AI policies

AI policies are read from and written to `jettybot_policy.pt` inside the active build directory because the launcher runs the executable from that directory.

New AI sessions begin with a safe built-in policy and can be refined through online imitation learning. Training uses the rule-based controller as a teacher, retains a bounded replay buffer, and periodically saves when policy saving is enabled. Legacy policy files that do not contain the current format marker are rejected safely instead of being loaded with incompatible input features.

Episode reward is reported as a training diagnostic. Score recognition is not yet implemented, so score-based rewards are currently inactive.

## Manual build and run

After configuration, you can bypass the launch script:

```bash
cmake --build build --parallel
cd build
./AuttoJetty
```

Run the executable from the build directory if you want policy loading and saving to use the same location as `launch.sh`.

## Troubleshooting

- **No screen-capture dialog appears:** verify that an XDG Desktop Portal backend for your desktop environment is installed and running.
- **`Failed to open /dev/uinput`:** load the `uinput` module and verify that your user has write permission on `/dev/uinput`.
- **Objects are not detected:** confirm the capture rectangles, keep the game's scale and colors consistent, and inspect the detection preview.
- **LibTorch is not found:** pass its absolute extracted path through `-DCMAKE_PREFIX_PATH` when configuring CMake.
- **A shared library is missing after a system update:** run `./launch.sh`; it detects broken dynamic-library links and rebuilds the executable.
- **CUDA configuration fails with the system compiler:** install GCC 15 as shown above, or use the CPU-only LibTorch distribution.

## Project structure

```text
AuttoJetty/
├── AuttoJetty/
│   ├── include/          # Capture, controller, and AI declarations
│   ├── src/              # Application implementation
│   └── jettybot_model.pt # Included experimental model
├── CMakeLists.txt        # Linux CMake build configuration
├── launch.sh             # Build and launch helper
└── README.md
```

## Current limitations

- Linux and Wayland only
- Color- and layout-specific computer-vision thresholds
- Manual screen dimensions and capture-region calibration
- Experimental AI behavior
- No score recognition yet

## Responsible use

AuttoJetty is an educational project intended for local experimentation. Automated input may violate the rules of online games or services; use it only where automation is permitted.

## Roadmap

- Add automated tests for geometry and controller decisions
- Replace color-specific thresholds with configurable calibration profiles
- Implement score recognition and meaningful score-based rewards
- Add recorded-frame playback for repeatable vision testing
