#ifndef RACERS_EMSCRIPTEN_FILESYSTEM_H
#define RACERS_EMSCRIPTEN_FILESYSTEM_H

// [library:filesystem] Web (Emscripten) data delivery and persistence.

#ifndef RACERS_EMSCRIPTEN_HOST
#define RACERS_EMSCRIPTEN_HOST ""
#endif

// Mount point for the streamed game data (LEGO.JAM, *.tun, *.avi). MiniwinResolvePath maps
// the game's data-file opens to absolute paths under this mount (no chdir required).
inline static const char* Racers_dataMount = "/racers";

// Persistent (OPFS-backed) tree for saves and the renderer preference. Must stay in
// sync with MiniwinGetUserDataPath's Emscripten branch (miniwin/src/windows/files.cpp).
inline static const char* Racers_userDataMount = "/racers-user";

// Host URL prefix for streamed data. Empty (the default) streams from the serving
// origin; set -DRACERS_EMSCRIPTEN_HOST="https://host" to stream cross-origin.
inline static const char* Racers_streamHost = RACERS_EMSCRIPTEN_HOST;

// Mounts the WASMFS fetch backend for game data (registering each file for case-folded
// resolution) and the OPFS backend for user data. Call once from SDL_AppInit, before the
// game performs any file I/O. Only defined in the Emscripten build.
void Racers_SetupWebFilesystem();

#endif // RACERS_EMSCRIPTEN_FILESYSTEM_H
