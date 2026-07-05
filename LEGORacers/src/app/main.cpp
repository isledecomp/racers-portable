// [library:window] WinMain and the Win32 message-pump entry point are replaced by the
// SDL3 callback entry points. The game's own flow (GameMain -> LegoRacers::Run) is a
// blocking nested loop and runs unchanged on a dedicated game thread; SDL events are
// forwarded to it through the miniwin event queue.

#define SDL_MAIN_USE_CALLBACKS
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

// clang-format off
#include <windows.h>
#include <mmsystem.h>
// clang-format on

#include "app/main.h"

#include "app/legoracers.h"
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
static SDL_AtomicInt g_gameThreadDone;
static SDL_AtomicInt g_gameResult;

static int SDLCALL GameThread(void*)
{
	// Quirk preserved from the original WinMain: the seed is the timeGetTime function
	// POINTER, not its return value.
	srand((unsigned int) (uintptr_t) timeGetTime);

	int result = GameMain(g_commandLineArgs.m_argc, g_commandLineArgs.m_argv);

	SDL_SetAtomicInt(&g_gameResult, result);
	SDL_SetAtomicInt(&g_gameThreadDone, 1);
	return result;
}

static void DisplayArgumentHelp()
{
	SDL_Log("racers-portable options:");
	SDL_Log("  --path <directory>   game data directory (LEGO.JAM, *.TUN, *.AVI)");
	SDL_Log("  --language <index>   language index (seeds the emulated registry LangID)");
	SDL_Log("  --help               show this help");
	SDL_Log("Original game options (passed through):");
	SDL_Log("  -novideo -window -primary -select3d -alphatrans -horzres <n> -vertres <n>");
}

SDL_AppResult SDL_AppInit(void** appstate, int argc, char** argv)
{
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMEPAD | SDL_INIT_HAPTIC | SDL_INIT_EVENTS)) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_Init failed: %s", SDL_GetError());
		return SDL_APP_FAILURE;
	}

	// Portable arguments are consumed here; everything else is passed through to
	// LegoRacers::ParseArguments untouched.
	g_commandLine[0] = '\0';
	for (int i = 1; i < argc; i++) {
		if (SDL_strcmp(argv[i], "--help") == 0) {
			DisplayArgumentHelp();
			return SDL_APP_SUCCESS;
		}
		if (SDL_strcmp(argv[i], "--language") == 0 && i + 1 < argc) {
			MiniwinSetRegistryLangId((DWORD) SDL_atoi(argv[i + 1]));
			i++;
			continue;
		}
		if (SDL_strcmp(argv[i], "--path") == 0 && i + 1 < argc) {
#ifdef _WIN32
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

	CoInitialize(0);

	g_hInstance = reinterpret_cast<HINSTANCE>(1);
	g_hPrevInstance = NULL;
	SplitCommand();

	SDL_SetAtomicInt(&g_gameThreadDone, 0);
	SDL_SetAtomicInt(&g_gameResult, 0);

	g_gameThread = SDL_CreateThread(GameThread, "GameThread", nullptr);
	if (!g_gameThread) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Unable to start game thread: %s", SDL_GetError());
		return SDL_APP_FAILURE;
	}

	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppIterate(void* appstate)
{
	if (SDL_GetAtomicInt(&g_gameThreadDone)) {
		return SDL_APP_SUCCESS;
	}

	SDL_Delay(1);
	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event)
{
	MiniwinApp_PushEvent(*event);
	return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result)
{
	if (g_gameThread) {
		// Ask the game to shut down and wait for the thread to finish.
		SDL_Event quitEvent;
		SDL_zero(quitEvent);
		quitEvent.type = SDL_EVENT_QUIT;
		MiniwinApp_PushEvent(quitEvent);

		int status = 0;
		SDL_WaitThread(g_gameThread, &status);
		g_gameThread = nullptr;
	}

	CoUninitialize();
}
