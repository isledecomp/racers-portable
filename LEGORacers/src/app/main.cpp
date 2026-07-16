// [library:window] WinMain replacement: traditional main() entry point with an event
// loop forwarding SDL2 events to the miniwin event queue. The game's own flow (GameMain
// -> LegoRacers::Run) is a blocking nested loop and runs on a dedicated game thread.
// SDL events are forwarded to it through the miniwin event queue.

#include <SDL2/SDL.h>

// clang-format off
#include <windows.h>
#include <mmsystem.h>
// clang-format on

#include "app/legoracers.h"
#include "app/main.h"
#include "decomp.h"
#include "golerror.h"
#include "types.h"

#include <miniwin/miniwinapp.h>
#include <objbase.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <unistd.h>
#else
#include <direct.h>
#endif

#include <thread>

DECOMP_SIZE_ASSERT(CommandLineArgs, 0x84)

// GLOBAL: LEGORACERS 0x004c47e4
HINSTANCE g_hInstance;

// GLOBAL: LEGORACERS 0x004c47e8
HINSTANCE g_hPrevInstance;

// GLOBAL: LEGORACERS 0x004c4a38
LegoRacers g_racers;

// GLOBAL: LEGORACERS 0x004c6ee8
LegoChar g_commandLine[256];

// GLOBAL: LEGORACERS 0x004c6fe8
CommandLineArgs g_commandLineArgs;

// FUNCTION: LEGORACERS 0x00449ce0
void SplitCommand()
{
	LegoS32 offset = 0;
	g_commandLineArgs.m_argc = 0;

	if (!g_commandLine[0]) {
		return;
	}

	LegoS32 i;
	for (; g_commandLineArgs.m_argc < (LegoS32) sizeOfArray(g_commandLineArgs.m_argv);
		 offset += i, g_commandLineArgs.m_argc++) {
		LegoChar* arg = &g_commandLine[offset];

		for (i = 0; arg[i] && arg[i] != ' '; i++) {
		}

		g_commandLineArgs.m_argv[g_commandLineArgs.m_argc] = arg;

		if (!g_commandLine[offset + i]) {
			g_commandLineArgs.m_argc++;
			break;
		}

		arg[i++] = '\0';
	}
}

static SDL_Thread* g_gameThread;
static SDL_atomic_t g_gameThreadDone;
static SDL_atomic_t g_gameResult;

static int SDLCALL GameThread(void*)
{
	// Quirk preserved from the original WinMain: the seed is the timeGetTime function
	// POINTER, not its return value.
	srand((unsigned int) (uintptr_t) timeGetTime);

	int result = GameMain(g_commandLineArgs.m_argc, g_commandLineArgs.m_argv);

	SDL_Log("Game loop finished (result %d)", result);
	SDL_AtomicSet(&g_gameResult, result);
	SDL_AtomicSet(&g_gameThreadDone, 1);
	return result;
}

static void DisplayArgumentHelp()
{
	SDL_Log("racers-portable options:");
	SDL_Log("  --path <directory>   game data directory (LEGO.JAM, *.TUN, *.AVI)");
	SDL_Log("  --language <index>   language index (seeds the emulated registry LangID)");
	SDL_Log("  --scale <mode>       fullscreen scaling: letterbox (default) or stretch");
	SDL_Log("  --resolution <mode>  render at native (default) or original (640x480) resolution");
	SDL_Log("  --renderer <name>    render backend: opengles3");
	SDL_Log("  --help               show this help");
	SDL_Log("Original game options (passed through):");
	SDL_Log("  -novideo -window -horzres <n> -vertres <n>");
}

// The Emacs, Vim, and Emacs file format settings are no longer needed.
static bool InitGame(int argc, char** argv)
{
	SDL_SetHint(SDL_HINT_MAC_OPENGL_ASYNC_DISPATCH, "1");

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) < 0) {
		SDL_Log("SDL_Init failed: %s", SDL_GetError());
		return false;
	}

	MiniwinApp_SetCommandLine(argc, argv);

	bool rendererFromCli = false;
	g_commandLine[0] = '\0';
	for (int i = 1; i < argc; i++) {
		if (SDL_strcmp(argv[i], "--help") == 0) {
			DisplayArgumentHelp();
			return false;
		}
		if (SDL_strcmp(argv[i], "--language") == 0 && i + 1 < argc) {
			MiniwinSetRegistryLangId((DWORD) SDL_atoi(argv[i + 1]));
			i++;
			continue;
		}
		if (SDL_strcmp(argv[i], "--resolution") == 0 && i + 1 < argc) {
			if (SDL_strcmp(argv[i + 1], "original") == 0) {
				MiniwinSetRenderResolution(MINIWIN_RESOLUTION_ORIGINAL);
			}
			else {
				MiniwinSetRenderResolution(MINIWIN_RESOLUTION_NATIVE);
			}
			i++;
			continue;
		}
		if (SDL_strcmp(argv[i], "--scale") == 0 && i + 1 < argc) {
			if (SDL_strcmp(argv[i + 1], "stretch") == 0) {
				MiniwinSetScaleMode(MINIWIN_SCALE_STRETCH);
			}
			else {
				MiniwinSetScaleMode(MINIWIN_SCALE_LETTERBOX);
			}
			i++;
			continue;
		}
		if (SDL_strcmp(argv[i], "--path") == 0 && i + 1 < argc) {
#if defined(_WIN32)
			_chdir(argv[i + 1]);
#else
			if (chdir(argv[i + 1]) != 0) {
				SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "--path: cannot enter '%s'", argv[i + 1]);
			}
#endif
			i++;
			continue;
		}

		if (g_commandLine[0]) {
			SDL_strlcat(g_commandLine, " ", sizeof(g_commandLine));
		}
		SDL_strlcat(g_commandLine, argv[i], sizeof(g_commandLine));
	}

	if (!rendererFromCli) {
		MiniwinBackendId saved;
		if (MiniwinBackendLoadPref(&saved)) {
			MiniwinSetBackend(saved);
		}
	}

	CoInitialize(0);

	g_hInstance = reinterpret_cast<HINSTANCE>(1);
	g_hPrevInstance = NULL;
	SplitCommand();

	SDL_AtomicSet(&g_gameThreadDone, 0);
	SDL_AtomicSet(&g_gameResult, 0);

	g_gameThread = SDL_CreateThread(GameThread, "GameThread", nullptr);
	if (!g_gameThread) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unable to start game thread: %s", SDL_GetError());
		return false;
	}

	return true;
}

// Synthetic input for automated testing: RACERS_AUTOKEY="ms:scancode[:holdMs],..."
static void PumpAutoKeys()
{
	static const char* spec = getenv("RACERS_AUTOKEY");
	if (!spec || !spec[0]) {
		return;
	}

	struct AutoKey {
		Uint64 m_atMs;
		SDL_Scancode m_scancode;
		bool m_up;
	};
	static AutoKey keys[64];
	static int keyCount = 0;
	static int cursor = 0;
	static Uint64 startMs = 0;

	if (!startMs) {
		startMs = SDL_GetTicks();
		char buffer[512];
		SDL_strlcpy(buffer, spec, sizeof(buffer));

		char* savePtr = nullptr;
		for (char* entry = strtok_r(buffer, ",", &savePtr); entry && keyCount + 2 <= (int) SDL_arraysize(keys);
			 entry = strtok_r(nullptr, ",", &savePtr)) {
			char* colon = SDL_strchr(entry, ':');
			if (!colon) {
				continue;
			}
			*colon = '\0';

			Uint64 at = (Uint64) SDL_atoi(entry);
			Uint64 hold = 80;
			char* holdSep = SDL_strchr(colon + 1, ':');
			if (holdSep) {
				*holdSep = '\0';
				hold = (Uint64) SDL_atoi(holdSep + 1);
			}

			SDL_Scancode scancode = (SDL_Scancode) SDL_atoi(colon + 1);
			keys[keyCount].m_atMs = at;
			keys[keyCount].m_scancode = scancode;
			keys[keyCount].m_up = false;
			keyCount++;
			keys[keyCount].m_atMs = at + hold;
			keys[keyCount].m_scancode = scancode;
			keys[keyCount].m_up = true;
			keyCount++;
		}
		for (int i = 1; i < keyCount; i++) {
			AutoKey pending = keys[i];
			int j = i - 1;
			for (; j >= 0 && keys[j].m_atMs > pending.m_atMs; j--) {
				keys[j + 1] = keys[j];
			}
			keys[j + 1] = pending;
		}
	}

	static SDL_Keymod heldMods = KMOD_NONE;

	Uint64 elapsed = SDL_GetTicks() - startMs;
	while (cursor < keyCount && keys[cursor].m_atMs <= elapsed) {
		SDL_Keymod mod = KMOD_NONE;
		switch (keys[cursor].m_scancode) {
		case SDL_SCANCODE_LALT:
			mod = KMOD_LALT;
			break;
		case SDL_SCANCODE_LCTRL:
			mod = KMOD_LCTRL;
			break;
		case SDL_SCANCODE_LSHIFT:
			mod = KMOD_LSHIFT;
			break;
		default:
			break;
		}
		if (keys[cursor].m_up) {
			heldMods = (SDL_Keymod) (heldMods & ~mod);
		}
		else {
			heldMods = (SDL_Keymod) (heldMods | mod);
		}

		SDL_Event event;
		SDL_zero(event);
		event.type = keys[cursor].m_up ? SDL_KEYUP : SDL_KEYDOWN;
		event.key.keysym.scancode = keys[cursor].m_scancode;
		event.key.keysym.sym = SDL_GetKeyFromScancode(keys[cursor].m_scancode);
		event.key.keysym.mod = heldMods;
		event.key.state = keys[cursor].m_up ? SDL_RELEASED : SDL_PRESSED;
		MiniwinApp_PushEvent(event);
		cursor++;
	}
}

static void PumpEvents()
{
	SDL_Event event;
	while (SDL_PollEvent(&event)) {
		MiniwinApp_PushEvent(event);
	}
}

int main(int argc, char* argv[])
{
	if (!InitGame(argc, argv)) {
		return 1;
	}

	int heartbeat = 0;
	while (true) {
		if (SDL_AtomicGet(&g_gameThreadDone)) {
			break;
		}

	PumpEvents();
	PumpAutoKeys();
	std::this_thread::sleep_for(std::chrono::milliseconds(1));
	}

	if (g_gameThread) {
		SDL_Event quitEvent;
		SDL_zero(quitEvent);
		quitEvent.type = SDL_QUIT;
		MiniwinApp_PushEvent(quitEvent);

		while (!SDL_AtomicGet(&g_gameThreadDone)) {
			SDL_PumpEvents();
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}

		int status = 0;
		SDL_WaitThread(g_gameThread, &status);
		g_gameThread = nullptr;
	}

	MiniwinBackend_Shutdown();
	CoUninitialize();
	return 0;
}
