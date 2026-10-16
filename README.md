# GNOME Wayland ImGui Overlay

A transparent, Dear ImGui overlay for GNOME on Wayland. ImGui
windows receive mouse and touch input while every transparent area passes input
through to applications underneath.

The project uses a bundled GNOME Shell extension to position the overlay, size
it to the monitor, and keep it above normal application windows. These window
management controls are not available directly to the app on GNOME Wayland.

![Platform](https://img.shields.io/badge/platform-GNOME%20Wayland-4A86CF)
![Language](https://img.shields.io/badge/language-C%2B%2B20-00599C)
[![Views](https://hits.sh/github.com/ankitrawatgit/GNOME-Wayland-ImGui-Overlay.svg?label=views&color=3A56D4)](https://github.com/ankitrawatgit/GNOME-Wayland-ImGui-Overlay)

Built with the help of Codex.

## Features

- Transparent SDL3/OpenGL overlay
- Mouse and touch pass-through outside visible ImGui windows
- Automatic monitor-sized geometry
- Automatic always-on-top through a small GNOME Shell extension

## Requirements

- GNOME 48, 49, or 50 running on Wayland
- A C++20 compiler, GNU Make, and `pkg-config`
- SDL3, Wayland, and OpenGL development headers
- `gnome-extensions`, `gsettings`, and Python 3 for first-launch setup

## Build

Clone the Dear ImGui submodule with the repository:

```bash
git clone --recurse-submodules https://github.com/ankitrawatgit/GNOME-Wayland-ImGui-Overlay.git
cd GNOME-Wayland-ImGui-Overlay
```

Install the SDL3, Wayland client, and OpenGL development packages using your
distribution's package manager. Their `pkg-config` package names must be
`sdl3`, `wayland-client`, and `gl`. Then build:

```bash
make -j"$(nproc)"
```

## First launch

```bash
./build/gnome-imgui-overlay
```

The first launch installs and pre-enables the bundled GNOME Shell controller.
GNOME Wayland cannot discover brand-new extension code inside an already-running
Shell session, so log out and back in once when notified. This is a one-time
step; launch the same binary normally afterward.

The extension may also be installed explicitly:

```bash
make install-extension
```

Use `Ctrl+C` in the launching terminal to close the overlay.

## How it works

The C++ application owns a transparent SDL3 Wayland surface and an OpenGL
context. Every frame it builds a Wayland `wl_region` from active ImGui window
rectangles and assigns it with `wl_surface_set_input_region`. Pointer and touch
events inside those rectangles reach ImGui; events elsewhere pass through.

### Why a GNOME Shell extension?

On GNOME Wayland, the compositor controls window placement and stacking. The
app cannot force its own screen position or always-on-top state, so the bundled
extension handles those operations from inside GNOME Shell. It positions and
sizes the overlay to the monitor and keeps it above normal application windows.
Rendering, transparency, and mouse/touch pass-through remain in the C++ app.

The extension listens for the `window-created` and `shown` signals, recognizes
the overlay by its app ID or window title, and applies its geometry and Mutter's
`make_above()` once when managing the window. It uses event signals rather than
polling or a per-frame loop.

## Project layout

```text
extension/     GNOME Shell controller
scripts/       GNOME Shell extension installer
src/           C++ overlay and Wayland input-region implementation
third_party/   Dear ImGui Git submodule
```

## Contributing

Improvements are welcome! Feel free to open an issue or submit a pull request
with bug fixes, GNOME/Wayland compatibility improvements, documentation updates,
or new ideas. Suggestions for simpler or more reliable ways to handle overlay
positioning and always-on-top behavior are especially welcome.