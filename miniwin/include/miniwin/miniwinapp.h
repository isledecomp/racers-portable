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

// Scales the relative mouse deltas fed to DirectInput into the game's cursor coordinate space
// (1.0 = no scaling). Used on the web, where the window fills the browser tab but the UI
// renders at a smaller fixed resolution, so unscaled deltas overshoot the cursor.
void MiniwinInput_SetMouseScale(float p_scaleX, float p_scaleY);

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
// is the desktop default, with OpenGL 3.3 as the fallback; OpenGL ES 3 / WebGL2 is the
// web backend (and an optional desktop one via --renderer opengles3). The value is read
// before the window is created.
enum MiniwinBackendId {
	MINIWIN_BACKEND_SDLGPU = 0,
	MINIWIN_BACKEND_OPENGL3 = 1,
	MINIWIN_BACKEND_OPENGLES3 = 2,
};
void MiniwinSetBackend(MiniwinBackendId p_backend);
MiniwinBackendId MiniwinGetBackend();

// Maps a --renderer name to an id ("sdlgpu"/"gpu"; "opengl3"/"opengl"/"gl";
// "opengles3"/"gles3"/"gles"/"webgl2"). Returns false on an unknown name (the caller
// warns and keeps the default).
bool MiniwinBackendFromName(const char* p_name, MiniwinBackendId* p_id);
const char* MiniwinBackendName(MiniwinBackendId p_backend);

// The persisted renderer choice: the single source of truth for which backend boots.
// Written by an explicit --renderer (so a CLI choice sticks) and by an in-game switch;
// read at startup, before the window is created, when --renderer is not given.
bool MiniwinBackendLoadPref(MiniwinBackendId* p_id);
void MiniwinBackendSavePref(MiniwinBackendId p_backend);

// Records the process command line so the in-game renderer switch can relaunch the
// process, replaying it without --renderer (the new backend comes from the saved
// preference). Call once at startup.
void MiniwinApp_SetCommandLine(int p_argc, char** p_argv);

// Configures SDL window attributes required by the render backend; returns extra
// SDL_WindowFlags to OR into SDL_CreateWindow. Main thread, before window creation.
// Resolves the active backend (falling back if the requested one is unavailable), so
// the window is created with flags matching the backend that will actually be used.
Uint32 MiniwinBackend_PrepareWindowFlags();

// Marks the resolved backend unusable and re-resolves to the next candidate. Call when
// SDL_CreateWindow failed for the flags PrepareWindowFlags returned (e.g. an OpenGL ES
// pref on a desktop with no ES driver); returns true if a *different* usable backend is
// now active so the caller can reset the GL attributes and retry, false if none remain.
bool MiniwinBackend_DemoteActiveBackend();

// Presents one decoded video frame (tightly packed RGBA8, p_width x p_height) through
// the active render backend, letterboxed into the window — so intro/cutscene playback
// uses the selected renderer like the rest of the game. Game thread (VideoPlayer).
void MiniwinBackend_PresentVideoFrame(SDL_Window* p_window, const void* p_rgba, int p_width, int p_height);

// Fullscreen carried from an Alt+Enter during video playback into the game's first
// display init. The video player records its toggle; Win32GolApp::InitializeDisplay
// consumes it once (returning false when none was recorded, e.g. -novideo).
void MiniwinApp_SetVideoFullscreenChoice(bool p_fullscreen);
bool MiniwinApp_ConsumeVideoFullscreenChoice(bool* p_fullscreen);

// True when the main window is in real (SDL) fullscreen. The menu cursor uses this: the relative
// (DirectInput) cursor accumulation is only correct in the captured-mouse fullscreen model;
// windowed mode positions the cursor from the absolute mouse instead. Safe on the game thread.
bool MiniwinApp_IsWindowFullscreen();

// Destroys the shared render backend. Call at application shutdown.
void MiniwinBackend_Shutdown();
