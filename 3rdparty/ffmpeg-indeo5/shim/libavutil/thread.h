/* Shim for FFmpeg's libavutil/thread.h — one-time init only. */
#ifndef AVUTIL_THREAD_H
#define AVUTIL_THREAD_H

#ifdef _WIN32

#include <windows.h>

typedef INIT_ONCE AVOnce;
#define AV_ONCE_INIT INIT_ONCE_STATIC_INIT

static BOOL CALLBACK ff_thread_once_thunk(PINIT_ONCE once, PVOID param, PVOID* context)
{
	(void) once;
	(void) context;
	((void (*)(void)) param)();
	return TRUE;
}

static inline int ff_thread_once(AVOnce* control, void (*routine)(void))
{
	return InitOnceExecuteOnce(control, ff_thread_once_thunk, (PVOID) routine, NULL) ? 0 : -1;
}

#else

#include <pthread.h>

typedef pthread_once_t AVOnce;
#define AV_ONCE_INIT PTHREAD_ONCE_INIT

static inline int ff_thread_once(AVOnce* control, void (*routine)(void))
{
	return pthread_once(control, routine);
}

#endif

#endif /* AVUTIL_THREAD_H */
