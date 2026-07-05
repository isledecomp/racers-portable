#pragma once

#include <SDL3/SDL.h>

#define LOG_CATEGORY_MINIWIN (SDL_LOG_CATEGORY_CUSTOM)

#ifdef _MSC_VER
#define MINIWIN_PRETTY_FUNCTION __FUNCSIG__
#else
#define MINIWIN_PRETTY_FUNCTION __PRETTY_FUNCTION__
#endif

#define MINIWIN_NOT_IMPLEMENTED()                                                                                      \
	do {                                                                                                               \
		static bool visited = false;                                                                                   \
		if (!visited) {                                                                                                \
			SDL_LogError(LOG_CATEGORY_MINIWIN, "%s: Not implemented", MINIWIN_PRETTY_FUNCTION);                        \
			visited = true;                                                                                            \
		}                                                                                                              \
	} while (0)

#define MINIWIN_ERROR(MSG)                                                                                             \
	do {                                                                                                               \
		SDL_LogError(LOG_CATEGORY_MINIWIN, "%s: %s", __func__, MSG);                                                   \
	} while (0)

#define MINIWIN_TRACE(...)                                                                                             \
	do {                                                                                                               \
		SDL_LogTrace(LOG_CATEGORY_MINIWIN, __VA_ARGS__);                                                               \
	} while (0)

// Stall diagnosis (RACERS_WATCHDOG=<ms>): the game thread bumps the heartbeat every
// event drain; layers mark what they are doing via MiniwinPhaseScope so a watchdog
// thread can attribute stalls. Phase 0 = game logic / unattributed.
enum MiniwinPhase {
	MINIWIN_PHASE_GAME = 0,
	MINIWIN_PHASE_FILE_IO = 1,
	MINIWIN_PHASE_TEXTURE_UPLOAD = 2,
	MINIWIN_PHASE_AUDIO = 3,
	MINIWIN_PHASE_PRESENT = 4,
	MINIWIN_PHASE_EVENT_QUEUE = 5,
};

extern SDL_AtomicInt g_miniwinHeartbeat;
extern SDL_AtomicInt g_miniwinPhase;

struct MiniwinPhaseScope {
	int m_previous;
	MiniwinPhaseScope(int p_phase)
	{
		m_previous = SDL_GetAtomicInt(&g_miniwinPhase);
		SDL_SetAtomicInt(&g_miniwinPhase, p_phase);
	}
	~MiniwinPhaseScope() { SDL_SetAtomicInt(&g_miniwinPhase, m_previous); }
};

// Logs any scope that takes longer than 30 ms ("[slow] <what> <detail> took N ms").
struct MiniwinSlowOpLog {
	const char* m_what;
	char m_detail[256];
	Uint64 m_startNs;

	MiniwinSlowOpLog(const char* p_what, const char* p_detail)
	{
		m_what = p_what;
		SDL_strlcpy(m_detail, p_detail ? p_detail : "", sizeof(m_detail));
		m_startNs = SDL_GetTicksNS();
	}
	~MiniwinSlowOpLog()
	{
		Uint64 elapsedNs = SDL_GetTicksNS() - m_startNs;
		if (elapsedNs > 30000000ull) {
			SDL_Log("[slow] %s %s took %.1f ms", m_what, m_detail, elapsedNs / 1e6);
		}
	}
};
