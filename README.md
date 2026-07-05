# LEGO Racers, portable

[Contributing](/CONTRIBUTING.md)

This initiative is a portable version of LEGO Racers (1999) based on the [decompilation project](https://github.com/foxtacles/racers). Our primary goal is to transform the codebase to achieve platform independence, thereby enhancing compatibility across various systems while preserving the original game's experience as faithfully as possible.

Please note: this project is primarily dedicated to achieving platform independence without altering the core gameplay or rewriting code for improvement's sake. While those are worthwhile objectives, they are not within the scope of this project.

## Status

| Platform | Status |
| - | - |
| Windows (x64, arm64) | [![CI](https://github.com/foxtacles/racers-portable/actions/workflows/ci.yml/badge.svg)](https://github.com/foxtacles/racers-portable/actions/workflows/ci.yml) |
| MacOS (arm64) | [![CI](https://github.com/foxtacles/racers-portable/actions/workflows/ci.yml/badge.svg)](https://github.com/foxtacles/racers-portable/actions/workflows/ci.yml) |
| Linux (x64, arm64) | [![CI](https://github.com/foxtacles/racers-portable/actions/workflows/ci.yml/badge.svg)](https://github.com/foxtacles/racers-portable/actions/workflows/ci.yml) |
| Web | [![CI](https://github.com/foxtacles/racers-portable/actions/workflows/ci.yml/badge.svg)](https://github.com/foxtacles/racers-portable/actions/workflows/ci.yml) |

We are actively working to support more platforms. If you have experience with a particular platform, we encourage you to contribute to `racers-portable`.

## Usage

**An existing copy of LEGO Racers is required to use this project.**

Place the built `LEGORacers` executable next to the game's data files (`LEGO.JAM`, the `*.TUN` music files, and the intro `*.AVI` movies), or point it at them with `--path <directory>`. The original game's command-line arguments (`-window`, `-novideo`, `-horzres`, `-vertres`, …) are supported.

## Library substitutions

To achieve our goal of platform independence, we need to replace any Windows-only libraries with platform-independent alternatives. This ensures that our codebase remains versatile and compatible across various systems. The following table serves as an overview of major libraries / subsystems and their chosen replacements.

| Library/subsystem | Substitution | Status |
| - | - | - |
| Window, Events | [SDL3](https://www.libsdl.org/) | 🚧 |
| Filesystem | [SDL3](https://www.libsdl.org/), POSIX shims | 🚧 |
| Threads, Mutexes (Synchronization) | [SDL3](https://www.libsdl.org/) | 🚧 |
| Keyboard/Mouse, DirectInput (Input) | [SDL3](https://www.libsdl.org/) | 🚧 |
| Joystick/Gamepad + Force Feedback, DirectInput (Input) | [SDL3](https://www.libsdl.org/) | 🚧 |
| WinMM, DirectSound (Audio) | [SDL3](https://www.libsdl.org/), [miniaudio](https://miniaud.io/) | 🚧 |
| DirectDraw (2D video) | [SDL3](https://www.libsdl.org/) | 🚧 |
| Direct3D immediate mode (3D video) | [SDL3 GPU (Vulkan, Metal, D3D12)](https://www.libsdl.org/), OpenGL 3.3, OpenGL ES 3.0 | 🚧 |
| DirectShow, Indeo 5 (Video playback) | Custom AVI demuxer + Indeo 5 decoder (based on [FFmpeg](https://ffmpeg.org/)'s), [SDL3](https://www.libsdl.org/) | 🚧 |
| Windows Registry (Language selection) | Command line (`--language`) | 🚧 |

## Building

This project uses the [CMake](https://cmake.org/) build system, which allows for a high degree of versatility regarding compilers and development environments.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
```

## Contributing

If you're interested in helping or contributing to this project, check out the [CONTRIBUTING](/CONTRIBUTING.md) page.
