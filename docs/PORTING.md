# Porting notes — racers-portable

This repository is a cross-platform port (Windows, Linux, macOS, Web/Emscripten) of the
[LEGO Racers decompilation](https://github.com/foxtacles/racers), modeled on
[isle-portable](https://github.com/isledecomp/isle-portable).

## Philosophy

The decompilation project remains the **source of truth**. Its sources are kept
byte-faithful — reccmp annotations (`// FUNCTION:`, `// GLOBAL:`, …), file layout, and
naming all stay intact — so that upstream improvements merge in continuously and cheaply.
All platform work happens in:

1. **`miniwin/`** — re-implementations of the Windows/DirectX APIs the game uses
   (DirectDraw/Direct3D 6 immediate mode, DirectSound, DirectInput 5, and a small Win32
   surface), built on SDL3 and miniaudio.
2. **`video/`** — a self-contained AVI demuxer + Indeo 5 decoder (trimmed port of
   FFmpeg's `indeo5`/`ivi` decoder; LGPL-2.1+, attribution preserved) replacing DirectShow.
3. **A tiny set of rewritten app-boundary files** (inventory below), mirroring how
   isle-portable rewrote `ISLE/`.

Platform-specific hooks inside otherwise-decomp-adjacent code are tagged with
`// [library:xxx]` comments (taxonomy: `window`, `filesystem`, `synchronization`,
`input`, `audio`, `2d`, `3d`, `video`, `config`).

## Library substitutions

| Original | Replacement |
|---|---|
| Win32 window/message pump | SDL3 (callback entry points; game loop on a dedicated thread) |
| DirectDraw 4 / Direct3D 3 (DX6 IM) | `miniwin` emulation over SDL3 GPU / OpenGL 3.3 / OpenGL ES 3 |
| DirectSound (DX6) | `miniwin` emulation over miniaudio (no-device engine) + SDL3 audio stream |
| DirectInput 5 (kbd/mouse/joystick/FF) | `miniwin` emulation over SDL3 keyboard/mouse/gamepad/haptics |
| DirectShow (Indeo 5 AVIs) | in-tree AVI demuxer + Indeo 5 decoder + SDL3 presentation |
| WinMM (`timeGetTime`, `timeSetEvent`) | SDL3 ticks/timers |
| Win32 mutexes (`CreateMutex`, …) | SDL3 mutexes behind tagged `HANDLE`s |
| Registry (`LangID` read) | emulated key seeded by `--language` |
| `<io.h>` CRT file API | POSIX shim with `\`→`/` translation and case-insensitive resolution |

## Touched decomp files (keep this list tiny)

Rewritten (app boundary):
- `LEGORacers/src/app/main.cpp` — WinMain → SDL3 callback shell + game thread
- `LEGORacers/src/app/win32golapp.cpp` (+ header types) — Win32 window/pump → SDL3;
  `LoadGolLibrary()` → direct `GolEntry()` (goldp is linked, not dlopen'd)
- `LEGORacers/src/video/videoplayer.cpp` (+ header internals) — DirectShow → `video/` module

Lightly touched:
- `util/decomp.h` — `DECOMP_SIZE_ASSERT` gated to 32-bit builds
- `LEGORacers/src/app/guids.cpp` — excluded from portable builds (miniwin owns GUID storage)
- `LEGORacers/racers.rc` — Windows-resource icon; portable builds embed via SDL

Excluded from portable builds (software rasterizer, deferred):
`GolDP/src/render/golrasterizers1.cpp`, `golrasterizers2.cpp`,
`golsoftwarerenderer.cpp`, `golsoftwarematerial*.cpp`. The two `__declspec(naked)`
functions there (`FUN_1003b930`, `FUN_1003ba30`) get equivalent-C++ rewrites when the
software renderer milestone lands.

Everything else — all of `GolDP/`, `common/`, and the exe's audio/input/menu/race/save
code — compiles **unmodified** against the `miniwin` headers.

## Upstream merges

The upstream decomp is the git remote `racers`. Merge regularly:

```bash
git fetch racers
git merge racers/master
```

Conflicts should only ever appear in the files listed above. After merging, rebuild and
run; if upstream renamed symbols used by `miniwin` glue or the rewritten files, fix those
call sites (the decomp side wins naming).

## Removed decompilation artifacts

Byte-accuracy verification (reccmp), the MSVC 6.0 toolchain support, and related tooling
live exclusively in the decompilation repository — they were removed from this fork
(mirroring how isle-portable relates to isle): `reccmp/`, `reccmp-project.yml`,
`cmake/reccmp.cmake`, the MSVC6/Wine `docker/` image, the patched `3rdparty/libcmt`, and
the reccmp-oriented `tools/` scripts. The reccmp **annotations in the source code are
kept** so upstream merges apply cleanly. `3rdparty/dx6` (the DirectX 6 SDK headers) is
retained for a possible future native-DirectX Windows build, analogous to isle-portable's
`3rdparty/dx5`.
