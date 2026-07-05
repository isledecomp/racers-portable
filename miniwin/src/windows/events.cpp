// [library:window] Thread-safe SDL event queue between the main thread and the game
// thread.

#include "miniwin.h"

#include <miniwin/miniwinapp.h>
#include <vector>

// SDL frees dynamically allocated event payloads (text input strings) once the
// original event goes out of scope on the main thread, so the queue keeps its own
// copy of the text and repoints the event at it when handing it out.
struct QueuedEvent {
	SDL_Event m_event;
	char m_text[64];
};

static SDL_Mutex* g_queueMutex;
static std::vector<QueuedEvent> g_queue;
static size_t g_queueHead;

SDL_AtomicInt g_miniwinHeartbeat;
SDL_AtomicInt g_miniwinPhase;

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

		int beat = SDL_GetAtomicInt(&g_miniwinHeartbeat);
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
				SDL_GetAtomicInt(&g_miniwinPhase),
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
	entry.m_text[0] = '\0';
	if (p_event.type == SDL_EVENT_TEXT_INPUT && p_event.text.text) {
		SDL_strlcpy(entry.m_text, p_event.text.text, sizeof(entry.m_text));
	}
	g_queue.push_back(entry);

	SDL_UnlockMutex(g_queueMutex);
}

bool MiniwinApp_PollEvent(SDL_Event& p_event)
{
	EnsureWatchdog();
	SDL_AddAtomicInt(&g_miniwinHeartbeat, 1);

	EnsureQueue();
	SDL_LockMutex(g_queueMutex);

	bool result = false;
	if (g_queueHead < g_queue.size()) {
		static char textBuffer[64];
		QueuedEvent& entry = g_queue[g_queueHead];
		p_event = entry.m_event;
		if (p_event.type == SDL_EVENT_TEXT_INPUT) {
			SDL_strlcpy(textBuffer, entry.m_text, sizeof(textBuffer));
			p_event.text.text = textBuffer;
		}
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
