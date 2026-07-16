#pragma once

// [library:3d] Per-backend factory entry points. Each backend translation unit
// (compiled only when its USE_* define is set) provides these; backends.cpp dispatches
// MiniwinBackend_* to the selected one. Kept separate from the public
// MiniwinBackend_Acquire/PrepareWindowFlags seam so adding a backend is local.

#include <SDL2/SDL.h>
#include <miniwin/miniwinapp.h>

class MiniwinRenderBackend;
struct GUID;

// Backend registry, used by the ddraw/d3d driver enumeration and the re-exec path.
// One selectable "display driver" is exposed per compiled+usable backend.
const GUID* MiniwinBackendGuid(MiniwinBackendId p_backend);
const char* MiniwinBackendDisplayName(MiniwinBackendId p_backend);
bool MiniwinBackendFromGuid(const GUID* p_guid, MiniwinBackendId* p_id);
bool MiniwinBackendUsable(MiniwinBackendId p_backend);

// The backend actually in use (the requested one, or a fallback if it was unavailable).
MiniwinBackendId MiniwinBackendActive();

// Fills p_out with the compiled, usable backends (active first); returns the count.
int MiniwinBackend_EnumDrivers(MiniwinBackendId* p_out, int p_max);

// Re-launches the process for the in-game renderer switch (SavePref is in miniwinapp.h).
void MiniwinBackend_Relaunch(MiniwinBackendId p_backend);

#ifdef USE_OPENGL3
// Cheap viability check (OpenGL 3.3 core is universally available on desktop).
bool MiniwinGl3_Available();
Uint32 MiniwinGl3_PrepareWindowFlags();
MiniwinRenderBackend* MiniwinGl3_Create(SDL_Window* p_window, int p_width, int p_height);
#endif

#ifdef USE_SDL_GPU
// Probes whether an SDL_GPU device can be created on this system (a create/destroy).
bool MiniwinSdlGpu_Available();
Uint32 MiniwinSdlGpu_PrepareWindowFlags();
MiniwinRenderBackend* MiniwinSdlGpu_Create(SDL_Window* p_window, int p_width, int p_height);
#endif

#ifdef USE_OPENGLES3
// OpenGL ES 3 / WebGL2. Reached through SDL_GL_GetProcAddress like the GL3 backend; a
// GL context can't be probed before a window exists, so Available reports true and
// Create fails cleanly on platforms with no ES driver.
bool MiniwinGles3_Available();
Uint32 MiniwinGles3_PrepareWindowFlags();
MiniwinRenderBackend* MiniwinGles3_Create(SDL_Window* p_window, int p_width, int p_height);
#endif
