// [library:synchronization] Win32 mutex + generic handle lifetime over SDL3.

#include "handles.h"
#include "miniwin.h"

#include <miniwin/windows.h>

HANDLE CreateMutex(LPSECURITY_ATTRIBUTES lpMutexAttributes, BOOL bInitialOwner, LPCSTR lpName)
{
	MiniwinMutexHandle* handle = new MiniwinMutexHandle();
	if (!handle->mutex) {
		delete handle;
		return nullptr;
	}

	if (bInitialOwner) {
		SDL_LockMutex(handle->mutex);
	}

	return handle;
}

DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds)
{
	MiniwinHandle* handle = static_cast<MiniwinHandle*>(hHandle);
	if (!handle || handle->type != MiniwinHandleType::Mutex) {
		return WAIT_FAILED;
	}

	SDL_Mutex* mutex = static_cast<MiniwinMutexHandle*>(handle)->mutex;

	if (dwMilliseconds == 0) {
		return SDL_TryLockMutex(mutex) ? WAIT_OBJECT_0 : WAIT_TIMEOUT;
	}

	// The game only ever waits with timeout 0 or INFINITE.
	SDL_LockMutex(mutex);
	return WAIT_OBJECT_0;
}

BOOL ReleaseMutex(HANDLE hMutex)
{
	MiniwinHandle* handle = static_cast<MiniwinHandle*>(hMutex);
	if (!handle || handle->type != MiniwinHandleType::Mutex) {
		return FALSE;
	}

	SDL_UnlockMutex(static_cast<MiniwinMutexHandle*>(handle)->mutex);
	return TRUE;
}

BOOL CloseHandle(HANDLE hObject)
{
	MiniwinHandle* handle = static_cast<MiniwinHandle*>(hObject);
	if (!handle) {
		return FALSE;
	}

	switch (handle->type) {
	case MiniwinHandleType::Mutex:
		delete static_cast<MiniwinMutexHandle*>(handle);
		return TRUE;
	case MiniwinHandleType::File:
		delete static_cast<MiniwinFileHandle*>(handle);
		return TRUE;
	}

	return FALSE;
}
