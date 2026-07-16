# LEGO Racers for Portmaster

## Changes from upstream

### SDL3 → SDL2

The upstream targets SDL3. This fork backports to SDL2 (`release-2.30.9`) via FetchContent for platforms that don't ship SDL3 yet. The SDL_GPU render backend, desktop OpenGL 3.3 backend, overlay system, Emscripten/web target, and touch input overlay were removed — only OpenGL ES 3.0 remains.

### Controller input

The default Player 1 keyboard bindings are remapped (`gamestate.cpp`) so gptokeyb can map A=Enter→Accelerate, B=Escape→Brake, X=M→Use Item without breaking menu confirm/cancel. F1 was added (`racesession.h`) as a hardcoded pause trigger for the Start button. Player 1's binding slot defaults to the keyboard entry so gptokeyb input is picked up correctly.

A gptokeyb config and PortMaster launch script are included.

### Quality-of-life

- `lmicmp.avi` (LEGO Media logo) is now skippable with any keypress.
- The in-game drawn cursor is hidden (`menuinputcursor.cpp`).
- The Exit button is removed from the main menu.

### Bug fixes

- `LegoS8` was `typedef char`, which becomes unsigned on ARM64 GCC. This corrupted opponent route data (rotations, positions, widths) and collision normals, causing AI racers to clip through the ground and spin wildly. Fixed by using `typedef signed char LegoS8`.

---

This initiative is a portable version of LEGO Racers (1999) based on the [decompilation project](https://github.com/isledecomp/racers). Our primary goal is to transform the codebase to achieve platform independence, thereby enhancing compatibility across various systems while preserving the original game's experience as faithfully as possible.

Please note: this project is primarily dedicated to achieving platform independence without altering the core gameplay or rewriting code for improvement's sake. While those are worthwhile objectives, they are not within the scope of this project.

## Status

| Platform | Status |
| - | - |
| Windows (x64) | [![CI](https://github.com/isledecomp/racers-portable/actions/workflows/ci.yml/badge.svg)](https://github.com/isledecomp/racers-portable/actions/workflows/ci.yml) |
| macOS (arm64) | [![CI](https://github.com/isledecomp/racers-portable/actions/workflows/ci.yml/badge.svg)](https://github.com/isledecomp/racers-portable/actions/workflows/ci.yml) |
| Linux (x64) | [![CI](https://github.com/isledecomp/racers-portable/actions/workflows/ci.yml/badge.svg)](https://github.com/isledecomp/racers-portable/actions/workflows/ci.yml) |
| [Web](https://warp.parts) | [![CI](https://github.com/isledecomp/racers-portable/actions/workflows/ci.yml/badge.svg)](https://github.com/isledecomp/racers-portable/actions/workflows/ci.yml) |

We are actively working to support more platforms. If you have experience with a particular platform, we encourage you to contribute to `racers-portable`.

## Usage

**An existing copy of LEGO Racers is required to use this project.**

Place the built `LEGORacers` executable next to the game's data files (`LEGO.JAM`, the `*.TUN` music files, and the intro `*.AVI` movies), or point it at them with `--path <directory>`. The original game's command-line arguments (`-window`, `-novideo`, `-horzres`, `-vertres`, …) are supported, plus:

- `--renderer sdlgpu|opengl3|opengles3` — select the render backend: SDL3 GPU (default; Vulkan/Metal/Direct3D 12), OpenGL 3.3, or OpenGL ES 3.0 / WebGL2. It can also be changed in-game under Options → Video (the game relaunches to apply it).
- `--scale letterbox|stretch` — fullscreen scaling: keep the 4:3 aspect ratio with black bars (default) or fill the screen.
- `--resolution native|original` — render 3D at the display resolution (default) or at the game's original 640x480, upscaled.
- `--language <index>` — seeds the language selection the original stored in the registry.

Alt+Enter toggles fullscreen. The intro movies play from the original Indeo 5 AVIs.

## Library substitutions

To achieve our goal of platform independence, we need to replace any Windows-only libraries with platform-independent alternatives. This ensures that our codebase remains versatile and compatible across various systems. The following table serves as an overview of major libraries / subsystems and their chosen replacements.

| Library/subsystem | Substitution | Status |
| - | - | - |
| Window, Events | [SDL3](https://www.libsdl.org/) | ✅ |
| Filesystem | [SDL3](https://www.libsdl.org/), POSIX shims | ✅ |
| Threads, Mutexes (Synchronization) | [SDL3](https://www.libsdl.org/) | ✅ |
| Keyboard/Mouse, DirectInput (Input) | [SDL3](https://www.libsdl.org/) | ✅ |
| Joystick/Gamepad + Force Feedback, DirectInput (Input) | [SDL3](https://www.libsdl.org/) | ✅ |
| WinMM (timers), DirectSound (Audio) | [SDL3](https://www.libsdl.org/), [miniaudio](https://miniaud.io/) | ✅ |
| DirectDraw (2D video) | [SDL3](https://www.libsdl.org/) | ✅ |
| Direct3D immediate mode (3D rendering) | [SDL3 GPU](https://www.libsdl.org/) (default; Vulkan/Metal/D3D12), OpenGL 3.3, and OpenGL ES 3.0 / WebGL2 | ✅ |
| DirectShow, Indeo 5 (Video playback) | In-tree AVI demuxer + Indeo 5 decoder (vendored from [FFmpeg](https://ffmpeg.org/) n4.4.4, byte-exact against ffmpeg), [SDL3](https://www.libsdl.org/) | ✅ |
| Windows Registry (Language selection) | Command line (`--language`) | ✅ |

## Building

This project uses the [CMake](https://cmake.org/) build system, which allows for a high degree of versatility regarding compilers and development environments.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
```

## Contributing

If you're interested in helping or contributing to this project, check out the [CONTRIBUTING](/CONTRIBUTING.md) page.
