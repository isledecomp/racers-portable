# Contributing

## General Guidelines

If you feel fit to contribute, feel free to create a pull request! Someone will review and merge it (or provide feedback) as soon as possible.

Please keep your pull requests small and understandable; large pull requests become significantly more unwieldy to review, and as such make it exponentially more likely for a mistake or error to go undetected. They also make it harder to merge other pull requests because the more files you modify, the more likely it is for a merge conflict to occur.

This repository has achieving platform independence as its primary goal. Any changes that modify decompiled game code (`LEGORacers`, `GolDP`, `common`) are unlikely to be accepted, unless they directly serve to increase platform compatibility.

## Decompilation Project Foundation

The [decompilation project](https://github.com/foxtacles/racers) serves as the foundation for this initiative. Improvements and changes to the decompilation project will be continually merged into our codebase. Therefore, please limit modifications to the code's structure/layout to the absolute minimum to facilitate easier merging.

Please also consider contributing improvements to function and variable names directly to the decompilation project. The goal is to maintain the decompilation project as the "source of truth" for all knowledge about the game.

See [docs/PORTING.md](/docs/PORTING.md) for the porting architecture and the complete inventory of decompiled files this project modifies.

## Overview

* [`3rdparty`](/3rdparty): Contains code obtained from third parties. As these are unaltered files, our style guide (see below) does not apply.
* [`LEGORacers`](/LEGORacers): Source code based on the decompiled `LEGORacers.exe` — the game's logic, menus, audio, input and application shell.
* [`GolDP`](/GolDP): Source code based on the decompiled `GolDP.dll` — the game's DirectDraw/Direct3D renderer backend ("Gol" engine).
* [`common`](/common): Decompiled engine code shared between `LEGORacers.exe` and `GolDP.dll` (compiled into both).
* [`miniwin`](/miniwin): Re-implementations of the Windows/DirectX APIs the game uses, built on SDL3 and miniaudio.
* [`video`](/video): Self-contained AVI demuxer and Indeo 5 video decoder (replaces DirectShow).
* [`tools`](/tools): A set of tools aiding in the modernization effort.
* [`util`](/util): Utility headers shared with the decompilation project.

## Code Style

In general, we're not exhaustively strict about coding style, but there are some preferable guidelines to follow that have been adopted from what we know about the original codebase:

### Formatting

We are currently using [clang-format](https://clang.llvm.org/docs/ClangFormat.html) and [clang-tidy](https://clang.llvm.org/extra/clang-tidy/) with configuration files that aim to replicate the code formatting employed by the original developers. There are [integrations](https://clang.llvm.org/docs/ClangFormat.html#vim-integration) available for most editors and IDEs.

### Naming conventions

We are currently using a customized version of [ncc](https://github.com/nithinn/ncc) with a configuration file that aims to replicate the naming conventions employed by the original developers; please refer to the [tool](/tools/ncc) for guidance.
