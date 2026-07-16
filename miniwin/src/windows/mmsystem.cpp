// [library:audio] WinMM timers over SDL2. timeSetEvent callbacks fire on SDL's timer
// thread, matching the original's multimedia-timer-thread model.

#include "miniwin.h"

#include <miniwin/mmsystem.h>

DWORD timeGetTime(void)
{
	return (DWORD) SDL_GetTicks();
}

struct MiniwinTimer {
	LPTIMECALLBACK callback;
	DWORD_PTR user;
	UINT id;
	UINT period;
	bool periodic;
};

static Uint32 SDLCALL MiniwinTimerCallback(Uint32 interval, void* param)
{
	MiniwinTimer* timer = static_cast<MiniwinTimer*>(param);
	timer->callback(timer->id, 0, timer->user, 0, 0);

	if (timer->periodic) {
		return timer->period;
	}

	delete timer;
	return 0;
}

MMRESULT timeSetEvent(UINT uDelay, UINT uResolution, LPTIMECALLBACK fptc, DWORD_PTR dwUser, UINT fuEvent)
{
	MiniwinTimer* timer = new MiniwinTimer();
	timer->callback = fptc;
	timer->user = dwUser;
	timer->period = uDelay;
	timer->periodic = (fuEvent & TIME_PERIODIC) != 0;

	SDL_TimerID id = SDL_AddTimer(uDelay, MiniwinTimerCallback, timer);
	if (!id) {
		delete timer;
		return 0;
	}

	timer->id = (UINT) id;
	return (MMRESULT) id;
}

MMRESULT timeKillEvent(UINT uTimerID)
{
	// Note: the MiniwinTimer allocation for periodic timers is intentionally leaked on
	// kill; SDL may still be delivering a callback. The game creates exactly one
	// periodic timer for its whole lifetime.
	SDL_RemoveTimer((SDL_TimerID) uTimerID);
	return TIMERR_NOERROR;
}
