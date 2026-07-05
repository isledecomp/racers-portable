#pragma once

// [library:window] Support API for the SDL3 application shell: the thread-safe event
// queue bridging the main thread (SDL callbacks) and the game thread (blocking
// LegoRacers::Run()), main-thread dispatch, and portable configuration hooks.

#include <SDL3/SDL.h>

#include "windows.h"

// Event queue: the main thread pushes SDL events; the game thread drains them from
// Win32GolApp::Tick().
void MiniwinApp_PushEvent(const SDL_Event& p_event);
bool MiniwinApp_PollEvent(SDL_Event& p_event);

// Runs a callable synchronously on the main thread (window management must happen
// there on some platforms). Safe to call from the main thread itself.
template <typename F>
inline void MiniwinApp_RunOnMainThread(F&& p_fn)
{
	SDL_RunOnMainThread(
		[](void* p_userdata) { (*static_cast<F*>(p_userdata))(); },
		&p_fn,
		true
	);
}

// Seeds the emulated registry's LangID value (--language command-line argument).
void MiniwinSetRegistryLangId(DWORD p_langId);
