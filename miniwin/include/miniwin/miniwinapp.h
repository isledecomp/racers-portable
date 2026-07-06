#pragma once

// [library:window] Support API for the SDL3 application shell: the thread-safe event
// queue bridging the main thread (SDL callbacks) and the game thread (blocking
// LegoRacers::Run()), main-thread dispatch, and portable configuration hooks.

#include "windows.h"

#include <SDL3/SDL.h>

// Event queue: the main thread pushes SDL events; the game thread drains them from
// Win32GolApp::Tick().
void MiniwinApp_PushEvent(const SDL_Event& p_event);
bool MiniwinApp_PollEvent(SDL_Event& p_event);

// Feeds a drained SDL event into the DirectInput emulation's buffered device data
// (keyboard/mouse rings). Call once per polled event, on the game thread.
void MiniwinInput_HandleEvent(const SDL_Event& p_event);

// Runs a callable synchronously on the main thread (window management must happen
// there on some platforms). Safe to call from the main thread itself.
template <typename F>
inline void MiniwinApp_RunOnMainThread(F&& p_fn)
{
	SDL_RunOnMainThread([](void* p_userdata) { (*static_cast<F*>(p_userdata))(); }, &p_fn, true);
}

// Seeds the emulated registry's LangID value (--language command-line argument).
void MiniwinSetRegistryLangId(DWORD p_langId);

// Fullscreen/window scaling of the logical resolution: preserve aspect ratio with
// black bars (default) or stretch to fill the drawable.
enum MiniwinScaleMode {
	MINIWIN_SCALE_LETTERBOX = 0,
	MINIWIN_SCALE_STRETCH = 1,
};
void MiniwinSetScaleMode(MiniwinScaleMode p_mode);
MiniwinScaleMode MiniwinGetScaleMode();

// Internal rasterization resolution: the native drawable size (default) or the
// game's original logical resolution (640x480) upscaled at present time.
enum MiniwinRenderResolution {
	MINIWIN_RESOLUTION_NATIVE = 0,
	MINIWIN_RESOLUTION_ORIGINAL = 1,
};
void MiniwinSetRenderResolution(MiniwinRenderResolution p_resolution);

// Mutes/unmutes the DirectSound emulation (used while the app is deactivated, like
// the original's non-GLOBALFOCUS DirectSound buffers behaved).
void MiniwinSound_SetSuspended(bool p_suspended);
MiniwinRenderResolution MiniwinGetRenderResolution();

// Render backend selection. The backend rasterizes the game's Direct3D output. SDL_GPU
// is the default; OpenGL 3.3 is the fallback (used on --renderer opengl3, or when
// SDL_GPU is unavailable). The value is read before the window is created.
enum MiniwinBackendId {
	MINIWIN_BACKEND_SDLGPU = 0,
	MINIWIN_BACKEND_OPENGL3 = 1,
};
void MiniwinSetBackend(MiniwinBackendId p_backend);
MiniwinBackendId MiniwinGetBackend();

// Maps a --renderer name to an id ("sdlgpu"/"gpu"; "opengl3"/"opengl"/"gl"). Returns
// false on an unknown name (the caller warns and keeps the default).
bool MiniwinBackendFromName(const char* p_name, MiniwinBackendId* p_id);
const char* MiniwinBackendName(MiniwinBackendId p_backend);

// Loads the persisted in-game renderer choice (written when switched via the menu).
// Read at startup, before the window is created, when --renderer is not given.
bool MiniwinBackendLoadPref(MiniwinBackendId* p_id);

// Records the process command line so the in-game renderer switch can relaunch with a
// different --renderer. Call once at startup.
void MiniwinApp_SetCommandLine(int p_argc, char** p_argv);

// Configures SDL window attributes required by the render backend; returns extra
// SDL_WindowFlags to OR into SDL_CreateWindow. Main thread, before window creation.
// Resolves the active backend (falling back if the requested one is unavailable), so
// the window is created with flags matching the backend that will actually be used.
Uint32 MiniwinBackend_PrepareWindowFlags();

// Presents one decoded video frame (tightly packed RGBA8, p_width x p_height) through
// the active render backend, letterboxed into the window — so intro/cutscene playback
// uses the selected renderer like the rest of the game. Game thread (VideoPlayer).
void MiniwinBackend_PresentVideoFrame(SDL_Window* p_window, const void* p_rgba, int p_width, int p_height);

// Fullscreen carried from an Alt+Enter during video playback into the game's first
// display init. The video player records its toggle; Win32GolApp::InitializeDisplay
// consumes it once (returning false when none was recorded, e.g. -novideo).
void MiniwinApp_SetVideoFullscreenChoice(bool p_fullscreen);
bool MiniwinApp_ConsumeVideoFullscreenChoice(bool* p_fullscreen);

// Destroys the shared render backend. Call at application shutdown.
void MiniwinBackend_Shutdown();
