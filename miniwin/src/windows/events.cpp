// [library:window] Thread-safe SDL event queue between the main thread and the game
// thread.

#include "miniwin.h"

#include <miniwin/miniwinapp.h>
#include <vector>

struct QueuedEvent {
	SDL_Event m_event;
};

static SDL_mutex* g_queueMutex;
static std::vector<QueuedEvent> g_queue;
static size_t g_queueHead;

static MiniwinScaleMode g_scaleMode = MINIWIN_SCALE_LETTERBOX;

void MiniwinSetScaleMode(MiniwinScaleMode p_mode)
{
	g_scaleMode = p_mode;
}

MiniwinScaleMode MiniwinGetScaleMode()
{
	return g_scaleMode;
}

static MiniwinRenderResolution g_renderResolution = MINIWIN_RESOLUTION_NATIVE;

void MiniwinSetRenderResolution(MiniwinRenderResolution p_resolution)
{
	g_renderResolution = p_resolution;
}

MiniwinRenderResolution MiniwinGetRenderResolution()
{
	return g_renderResolution;
}

// The video player toggles fullscreen directly on the window (the game's display does not
// exist yet during the movies), so this one-shot carries the choice into the game's first
// display init — otherwise the main menu snaps back to the launch mode. 0 = no choice
// (the zero-initialized default), 1 = windowed, 2 = fullscreen.
static SDL_atomic_t g_videoFullscreenChoice;

void MiniwinApp_SetVideoFullscreenChoice(bool p_fullscreen)
{
	SDL_AtomicSet(&g_videoFullscreenChoice, p_fullscreen ? 2 : 1);
}

bool MiniwinApp_ConsumeVideoFullscreenChoice(bool* p_fullscreen)
{
	int value = SDL_AtomicSet(&g_videoFullscreenChoice, 0);
	if (value == 0) {
		return false;
	}
	*p_fullscreen = value == 2;
	return true;
}

SDL_atomic_t g_miniwinHeartbeat;
SDL_atomic_t g_miniwinPhase;

// RACERS_WATCHDOG=<ms>: report game-thread stalls longer than the threshold along
// with the miniwin phase the thread is stuck in.
static int SDLCALL WatchdogThread(void* p_threshold)
{
	Uint64 thresholdMs = (Uint64) (uintptr_t) p_threshold;
	int lastBeat = -1;
	Uint64 lastChangeMs = SDL_GetTicks();
	bool reported = false;

	for (;;) {
		SDL_Delay(25);

		int beat = SDL_AtomicGet(&g_miniwinHeartbeat);
		Uint64 nowMs = SDL_GetTicks();
		if (beat != lastBeat) {
			if (reported) {
				SDL_Log("[watchdog] game thread resumed after %llu ms", (unsigned long long) (nowMs - lastChangeMs));
			}
			lastBeat = beat;
			lastChangeMs = nowMs;
			reported = false;
		}
		else if (!reported && nowMs - lastChangeMs > thresholdMs) {
			reported = true;
			SDL_Log(
				"[watchdog] game thread stalled %llu ms (phase %d) at t=%llu",
				(unsigned long long) (nowMs - lastChangeMs),
				SDL_AtomicGet(&g_miniwinPhase),
				(unsigned long long) nowMs
			);
		}
	}

	return 0;
}

static void EnsureWatchdog()
{
	static bool started;
	if (started) {
		return;
	}
	started = true;

	const char* spec = getenv("RACERS_WATCHDOG");
	if (spec && spec[0]) {
		int thresholdMs = SDL_atoi(spec);
		if (thresholdMs <= 0) {
			thresholdMs = 250;
		}
		SDL_DetachThread(SDL_CreateThread(WatchdogThread, "MiniwinWatchdog", (void*) (uintptr_t) thresholdMs));
	}
}

static void EnsureQueue()
{
	if (!g_queueMutex) {
		g_queueMutex = SDL_CreateMutex();
	}
}

void MiniwinApp_PushEvent(const SDL_Event& p_event)
{
	EnsureQueue();
	SDL_LockMutex(g_queueMutex);

	QueuedEvent entry;
	entry.m_event = p_event;
	g_queue.push_back(entry);

	SDL_UnlockMutex(g_queueMutex);
}

bool MiniwinApp_PollEvent(SDL_Event& p_event)
{
	EnsureWatchdog();
	SDL_AtomicAdd(&g_miniwinHeartbeat, 1);
	EnsureQueue();
	SDL_LockMutex(g_queueMutex);

	bool result = false;
	if (g_queueHead < g_queue.size()) {
		p_event = g_queue[g_queueHead].m_event;
		g_queueHead++;
		result = true;
	}
	else if (!g_queue.empty()) {
		g_queue.clear();
		g_queueHead = 0;
	}

	SDL_UnlockMutex(g_queueMutex);
	return result;
}
