// [library:window] Thread-safe SDL event queue between the main thread and the game
// thread.

#include "miniwin.h"

#include <miniwin/miniwinapp.h>
#include <vector>

static SDL_Mutex* g_queueMutex;
static std::vector<SDL_Event> g_queue;
static size_t g_queueHead;

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
	g_queue.push_back(p_event);
	SDL_UnlockMutex(g_queueMutex);
}

bool MiniwinApp_PollEvent(SDL_Event& p_event)
{
	EnsureQueue();
	SDL_LockMutex(g_queueMutex);

	bool result = false;
	if (g_queueHead < g_queue.size()) {
		p_event = g_queue[g_queueHead];
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
