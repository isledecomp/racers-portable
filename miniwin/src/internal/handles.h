#pragma once

// [library:synchronization] Tagged HANDLE model: Win32 HANDLEs from miniwin wrap one of
// these records so CloseHandle can dispatch by type.

#include <SDL3/SDL.h>
#include <miniwin/windows.h>

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
	MiniwinMutexHandle() : MiniwinHandle(MiniwinHandleType::Mutex), mutex(SDL_CreateMutex()) {}
	~MiniwinMutexHandle()
	{
		if (mutex) {
			SDL_DestroyMutex(mutex);
		}
	}

	SDL_Mutex* mutex;
};

struct MiniwinFileHandle : MiniwinHandle {
	MiniwinFileHandle(SDL_IOStream* p_stream) : MiniwinHandle(MiniwinHandleType::File), stream(p_stream) {}
	~MiniwinFileHandle()
	{
		if (stream) {
			SDL_CloseIO(stream);
		}
	}

	SDL_IOStream* stream;
};

// Resolves a game-supplied path (possibly with '\\' separators and wrong case) to an
// on-disk path. Returns true and fills p_resolved on success; on failure p_resolved holds
// the normalized-but-unresolved path (usable for creation). Implemented in files.cpp.
bool MiniwinResolvePath(const char* p_path, char* p_resolved, size_t p_resolvedSize);
