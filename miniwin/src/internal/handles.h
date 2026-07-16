#pragma once

// [library:synchronization] Tagged HANDLE model: Win32 HANDLEs from miniwin wrap one of
// these records so CloseHandle can dispatch by type.

#include <SDL2/SDL.h>
#include <miniwin/windows.h>

#include <mutex>

enum class MiniwinHandleType {
	Mutex,
	File,
};

struct MiniwinHandle {
	MiniwinHandleType type;

protected:
	MiniwinHandle(MiniwinHandleType p_type) : type(p_type) {}
};

struct MiniwinMutexHandle : MiniwinHandle {
	// Use std::recursive_mutex instead of SDL_mutex because Win32 mutexes are
	// recursive — the game's DirectMusic code locks its per-stream mutex and
	// then calls StopBuffer which locks the same mutex again. SDL_mutex is
	// non-recursive, so that sequence would self-deadlock.
	std::recursive_mutex* mutex = new std::recursive_mutex();

	MiniwinMutexHandle() : MiniwinHandle(MiniwinHandleType::Mutex) {}
	~MiniwinMutexHandle()
	{
		delete mutex;
	}
};

struct MiniwinFileHandle : MiniwinHandle {
	MiniwinFileHandle(SDL_RWops* p_stream) : MiniwinHandle(MiniwinHandleType::File), stream(p_stream) {}
	~MiniwinFileHandle()
	{
		if (stream) {
			SDL_RWclose(stream);
		}
	}

	SDL_RWops* stream;
};
