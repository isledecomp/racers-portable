# racers-portable — agent notes

Cross-platform port (SDL3) of the [LEGO Racers decompilation](https://github.com/foxtacles/racers), modeled on [isle-portable](https://github.com/isledecomp/isle-portable). Windows/Linux/macOS today; emscripten deferred.

## Ground rules

- **The decompilation is the source of truth.** `LEGORacers/`, `GolDP/`, `common/` and `util/` come from the upstream decomp (git remote `racers`) and must stay mergeable: keep reccmp annotations (`// FUNCTION:`, `// GLOBAL:`, …) intact and do not rename or reorganize decomp code here.
- **Upstream-first policy.** Any change the original 32-bit binary could still byte-match (type modeling, renames, restructuring) is made in the decomp repo first — verified there with a full reccmp report diff — and arrives here by merge/rebase. Only genuine portability work is made directly in this repo.
- **Keep the touched-file surface tiny.** Rewritten app boundary: `LEGORacers/src/app/main.cpp`, `win32golapp.cpp`, `LEGORacers/src/video/` (player, AVI demuxer). Everything else in decomp directories should compile unmodified against the `miniwin/` headers; pointer-width fixes are a few lines each and tagged `// 64-bit compatibility:`. Platform hooks in decomp-adjacent code are tagged `// [library:xxx]` (window, filesystem, synchronization, input, audio, 2d, 3d, video, config).
- All platform work lives in `miniwin/` (DirectDraw/D3D6-IM, DirectSound, DirectInput 5 and a small Win32 surface over SDL3 + miniaudio) and `3rdparty/` (vendored FFmpeg n4.4.4 Indeo 5 decoder behind a minimal shim; `Indeo5_Open/Decode/Close`).
- `clang-format` everything you touch (CI-enforced; the vendored FFmpeg sources under `3rdparty/ffmpeg-indeo5/` except the wrapper are exempt and must stay byte-unmodified).

## Building

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
```

`DOWNLOAD_DEPENDENCIES=ON` (default) fetches SDL3 at a pinned release; with it OFF, a system SDL3 is found via `find_package`. A game installation (LEGO.JAM, `*.TUN`, Indeo 5 `*.AVI`) is required to run: `./build/LEGORacers --path <gamedir>`.

## Testing aids (environment variables)

- `RACERS_AUTOKEY="ms:scancode[:holdMs],..."` — scripted key injection (SDL scancodes; Alt/Ctrl/Shift chord state is tracked).
- `RACERS_DUMP_EVERY=ms:prefix` / `RACERS_DUMP_FRAME=N:path` — periodic / single-frame screenshots.
- `RACERS_WATCHDOG=<ms>` — reports game-thread stalls with the miniwin phase they occurred in; slow operations (>30 ms) log themselves.
- `RACERS_GL_STATS=1` — per-100-frame draw stats and frame-time min/avg/max (run with `SDL_LOGGING="*=info"`).
- `RACERS_VSYNC=1`, `RACERS_WINDOW_TITLE=...` — vsync A/B lever; distinct window title for scripted test instances.

## Player-facing options

`--path <dir>`, `--language <index>`, `--scale letterbox|stretch`, `--resolution native|original`, plus the original arguments (`-window`, `-novideo`, …). No `-window` means fullscreen. Alt+Enter toggles fullscreen.
