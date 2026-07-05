// [library:window] Rewritten on SDL3: window management, the message pump and
// display-mode switching are replaced (the Win32 originals live in the decompilation
// project); GolDP is linked directly instead of loaded via LoadLibrary. All
// platform-neutral logic is kept verbatim.

#include "app/win32golapp.h"

#include "app/golappeventhandler.h"
#include "core/gol.h"
#include "golerror.h"
#include "golfsutil.h"
#include "golstream.h"
#include "input/directinputmanager.h"
#include "input/mousedevice.h"
#include "render/golcommondrawstate.h"

#include <SDL3/SDL.h>
#include <miniwin/miniwinapp.h>
#include <mmsystem.h>
#include <stdio.h>
#include <string.h>

extern HINSTANCE g_hInstance;
extern HINSTANCE g_hPrevInstance;

DECOMP_SIZE_ASSERT(Win32GolApp, 0x944)

static SDL_Window* AppWindow(HWND p_hWnd)
{
	return reinterpret_cast<SDL_Window*>(p_hWnd);
}

// Alt+Enter handling swallows the Enter release that follows the toggle.
static bool g_suppressReturnUp;

// FUNCTION: LEGORACERS 0x004164d0
Win32GolApp::Win32GolApp()
{
	m_golLibrary = NULL;
	m_golExport = NULL;
	m_hWnd = 0;
	m_golBackendType = c_golBackendDP;
	m_windowMode = c_windowModeNone;
	m_hCursor = 0;
	m_fullscreenStyle = 0;
	m_windowedStyle = 0;
	m_windowStateChanging = FALSE;
	m_pollInput = 1;
}

// FUNCTION: LEGORACERS 0x00416580
Win32GolApp::~Win32GolApp()
{
	Destroy();
}

// FUNCTION: LEGORACERS 0x004165e0
void Win32GolApp::Initialize(const LegoChar* p_windowName, const LegoChar* p_fileName)
{
	if (m_flags & c_flagInitialized) {
		Destroy();
	}

	g_hashTable = &GolApp::GetHashTable();

	if (p_fileName) {
		AddFileSourcesFromList(p_fileName);
	}

	m_fullscreenStyle = WS_POPUP | WS_CLIPCHILDREN;
	m_windowedStyle = WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN;

	SDL_Window* window = nullptr;
	char title[128];
	SDL_strlcpy(title, p_windowName ? p_windowName : "LEGO Racers", sizeof(title));

	// Distinguish scripted test instances from the user's own window.
	const char* titleOverride = getenv("RACERS_WINDOW_TITLE");
	if (titleOverride && titleOverride[0]) {
		SDL_strlcpy(title, titleOverride, sizeof(title));
	}
	MiniwinApp_RunOnMainThread([&window, &title]() {
		Uint32 backendFlags = MiniwinBackend_PrepareWindowFlags();
		window = SDL_CreateWindow(title, 640, 480, SDL_WINDOW_HIDDEN | backendFlags);
		if (window) {
			SDL_StartTextInput(window);
			// The original hid the Win32 cursor over the client area (WM_SETCURSOR
			// with a NULL class cursor); the game draws its own pointer.
			SDL_HideCursor();
		}
	});

	m_hWnd = reinterpret_cast<HWND>(window);
	if (!m_hWnd) {
		GOL_FATALERROR_MESSAGE("Unable to create application window");
	}

	LoadGolLibrary();
	InitializeInput();
	m_golDrawState->SetWindowHandle(m_hWnd);
	m_lastFrameTimeMs = timeGetTime();
	m_windowMode = c_windowModeNone;
	m_flags |= c_flagInitialized;
}

// FUNCTION: LEGORACERS 0x004167b0
void Win32GolApp::ShutdownDisplay()
{
	m_unk0x808 = 0;
	m_renderer = NULL;

	if (m_golDrawState && (m_golDrawState->GetFlags() & GolDrawState::c_flagCreated)) {
		m_golDrawState->DestroyDisplay();
	}

	m_flags &= ~c_flagDisplayActive;
}

// FUNCTION: LEGORACERS 0x004167e0
void Win32GolApp::Destroy()
{
	m_golBackendType = c_golBackendDP;
	ShutdownDisplay();

	if (m_hWnd) {
		SDL_Window* window = AppWindow(m_hWnd);
		MiniwinApp_RunOnMainThread([window]() { SDL_DestroyWindow(window); });
		m_hWnd = 0;
	}

	for (LegoU32 i = 0; i < sizeOfArray(m_fileSources); i++) {
		if (m_fileSources[i].GetStream()) {
			m_fileSources[i].Reset();
			m_files[i].Dispose();
		}
	}

	m_inputManager.Shutdown();
	UnloadGolLibrary();

	m_windowMode = c_windowModeNone;
	m_flags = 0;
}

// FUNCTION: LEGORACERS 0x00416860
void Win32GolApp::AddFileSourcesFromList(const LegoChar* p_fileList)
{
	if (!p_fileList) {
		return;
	}

	LegoChar* buffer = new LegoChar[strlen(p_fileList) + 1];
	LegoChar* saved = buffer;

	if (buffer) {
		strcpy(buffer, p_fileList);

		while (buffer) {
			if (m_fileSourceCount >= sizeOfArray(m_fileSources)) {
				break;
			}

			LegoChar* newline = strchr(buffer, '\n');
			if (newline) {
				*newline = '\0';
			}

			if (!m_files[m_fileSourceCount].BufferedOpen(buffer, GolStream::c_modeRead, 0x8000)) {
				m_fileSources[m_fileSourceCount].AttachStream(&m_files[m_fileSourceCount]);
				m_fileSourceCount++;
			}

			if (!newline) {
				break;
			}
			buffer = newline + 1;
		}

		if (m_fileSourceCount > 0) {
			g_fileSourceCount = m_fileSourceCount;
			g_fileSources = m_fileSources;
		}

		delete[] saved;
	}
}

// FUNCTION: LEGORACERS 0x00416960
void Win32GolApp::LoadGolLibrary()
{
	GolImport golImport;

	// GolDP is linked into the application; the original selected between
	// GolGlide/GolSoft/GolD3D/GolDP DLLs at runtime here.
	m_golLibrary = reinterpret_cast<HMODULE>(1);

	CreateGolImport(&golImport);
	golImport.m_fatalErrorMessage = GolFatalErrorMessage;
	m_golExport = GolEntry(&golImport);
	m_golDrawState = m_golExport->GetDrawState();
}

// FUNCTION: LEGORACERS 0x00416a30
void Win32GolApp::UnloadGolLibrary()
{
	m_golDrawState = NULL;

	if (m_golLibrary) {
		GolExit();
		m_golLibrary = NULL;
	}
}

// FUNCTION: LEGORACERS 0x00416a90
void Win32GolApp::InitializeInput()
{
	m_inputManager.Initialize(g_hInstance, m_hWnd);
}

// FUNCTION: LEGORACERS 0x00416ab0
void Win32GolApp::ShutdownInput()
{
	m_inputManager.Shutdown();
}

// FUNCTION: LEGORACERS 0x00416ac0
void Win32GolApp::InitializeDisplayWithDevice(
	LegoU32 p_width,
	LegoU32 p_height,
	LegoU32 p_bpp,
	LegoU32 p_flags,
	const LegoChar* p_driverName,
	const LegoChar* p_deviceName
)
{
	m_golDrawState->SelectDevice(p_driverName, p_deviceName);
	InitializeDisplay(p_width, p_height, p_bpp, p_flags | c_flagDeviceSelected);
}

// Applies the current windowed/fullscreen state to the SDL window.
static void ApplyWindowMode(HWND p_hWnd, LegoBool32 p_fullscreen, LegoU32 p_width, LegoU32 p_height)
{
	SDL_Window* window = AppWindow(p_hWnd);
	if (!window) {
		return;
	}

	MiniwinApp_RunOnMainThread([window, p_fullscreen, p_width, p_height]() {
		if (p_fullscreen) {
			SDL_SetWindowSize(window, (int) p_width, (int) p_height);
			SDL_SetWindowFullscreen(window, true);
		}
		else {
			SDL_SetWindowFullscreen(window, false);
			SDL_SetWindowSize(window, (int) p_width, (int) p_height);
			SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
		}
		SDL_ShowWindow(window);
		SDL_RaiseWindow(window);
	});
}

// FUNCTION: LEGORACERS 0x00416b00
LegoS32 Win32GolApp::InitializeDisplay(LegoU32 p_width, LegoU32 p_height, LegoU32 p_bpp, LegoU32 p_flags)
{
	if (m_flags & c_flagDisplayActive) {
		ShutdownDisplay();
	}

	m_width = p_width;
	m_height = p_height;
	m_bpp = p_bpp;
	m_flags = p_flags;
	m_windowStateChanging = TRUE;

	LegoU32 drawFlags = BuildDrawStateFlags(p_flags);
	LegoS32 result = m_golDrawState->RecreateDisplay(p_width, p_height, p_bpp, drawFlags);
	if (result) {
		return result;
	}

	GolCommonDrawState* commonState = static_cast<GolCommonDrawState*>(m_golDrawState);
	Win32GolApp::m_renderer = commonState->m_currentRenderer;
	m_unk0x808 = m_golDrawState->m_displaySurface ? 1 : 0;

	if (m_golDrawState->m_flags & GolDrawState::c_flagHardwareDevice) {
		m_flags |= c_flagFullscreen;
	}

	if (!(m_flags & c_flagFullscreen)) {
		m_windowMode = c_windowModeWindowed;
		ApplyWindowMode(m_hWnd, FALSE, p_width, p_height);
	}
	else {
		m_windowMode = c_windowModeFullscreen;
		GetInputManager()->GetMouse()->SetNonExclusiveMode();
		ApplyWindowMode(m_hWnd, TRUE, p_width, p_height);
	}

	m_windowStateChanging = FALSE;
	m_flags |= c_flagDisplayActive;
	return 0;
}

// FUNCTION: LEGORACERS 0x00416cd0
void Win32GolApp::ToggleFullscreen()
{
	if (m_golDrawState->GetFlags() & GolDrawState::c_flagCreated) {
		OutputDebugString("Toggling full-screen mode\n");

		if (m_flags & c_flagFullscreen) {
			OutputDebugString("--to windowed\n");
			ChangeWindowState(c_windowModeWindowed);
		}
		else {
			OutputDebugString("--to full screen\n");
			ChangeWindowState(c_windowModeFullscreen);
		}
	}
}

// FUNCTION: LEGORACERS 0x00416d20
LegoBool32 Win32GolApp::IsCursorInClientArea(HWND p_hWnd)
{
	RECT rect;
	POINT topLeft;
	POINT bottomRight;
	POINT cursor;

	GetClientRect(p_hWnd, &rect);
	topLeft.x = rect.left;
	topLeft.y = rect.top;
	bottomRight.x = rect.right;
	bottomRight.y = rect.bottom;
	ClientToScreen(p_hWnd, &topLeft);
	ClientToScreen(p_hWnd, &bottomRight);
	GetCursorPos(&cursor);
	return cursor.x >= topLeft.x && cursor.x < bottomRight.x && cursor.y >= topLeft.y && cursor.y < bottomRight.y;
}

// FUNCTION: LEGORACERS 0x00416db0
void Win32GolApp::UpdateMousePosition()
{
	m_eventHandler->OnCursorInside();

	if (m_windowMode == c_windowModeWindowed) {
		RECT rect;
		POINT topLeft;
		POINT bottomRight;
		POINT cursorPos;

		GetClientRect(m_hWnd, &rect);
		topLeft.x = rect.left;
		topLeft.y = rect.top;
		bottomRight.x = rect.right;
		bottomRight.y = rect.bottom;
		ClientToScreen(m_hWnd, &topLeft);
		ClientToScreen(m_hWnd, &bottomRight);
		GetCursorPos(&cursorPos);

		if (cursorPos.x < topLeft.x || cursorPos.x >= bottomRight.x || cursorPos.y < topLeft.y ||
			cursorPos.y >= bottomRight.y) {
			m_eventHandler->OnCursorOutside();
		}
		else {
			LegoFloat width = (LegoFloat) m_golDrawState->m_width;
			LegoFloat xScale = (LegoFloat) (bottomRight.x - topLeft.x) / width;
			LegoFloat height = (LegoFloat) m_golDrawState->m_height;
			LegoFloat yScale = (LegoFloat) (bottomRight.y - topLeft.y) / height;
			cursorPos.x -= topLeft.x;
			cursorPos.y -= topLeft.y;
			cursorPos.x = static_cast<LONG>(static_cast<LegoFloat>(cursorPos.x) / xScale);
			cursorPos.y = static_cast<LONG>(static_cast<LegoFloat>(cursorPos.y) / yScale);
			m_eventHandler->OnCursorMoved(cursorPos.x, cursorPos.y);
		}
	}
}

// FUNCTION: LEGORACERS 0x00416ef0
LegoS32 Win32GolApp::Tick(GolAppEventHandler* p_eventHandler)
{
	m_eventHandler = p_eventHandler;
	UpdateMousePosition();

	// Replaces the PeekMessage pump + AppWndProc: SDL events (forwarded from the main
	// thread) are dispatched to the same GolAppEventHandler notifications the Win32
	// window procedure produced.
	SDL_Event event;
	do {
		while (MiniwinApp_PollEvent(event)) {
			// Alt+Enter toggles fullscreen and must not leak into the game's input
			// layer (Enter doubles as the menu click).
			if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_RETURN && (event.key.mod & SDL_KMOD_ALT)) {
				ToggleFullscreen();
				g_suppressReturnUp = true;
				continue;
			}
			if (g_suppressReturnUp && event.type == SDL_EVENT_KEY_UP && event.key.key == SDLK_RETURN) {
				g_suppressReturnUp = false;
				continue;
			}

			MiniwinInput_HandleEvent(event);

			switch (event.type) {
			case SDL_EVENT_QUIT:
				// WM_QUIT equivalent.
				m_eventHandler = 0;
				return 0;
			case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
				NotifyCloseRequested();
				break;
			case SDL_EVENT_TEXT_INPUT:
				if (m_eventHandler && event.text.text) {
					for (const char* c = event.text.text; *c; c++) {
						m_eventHandler->OnChar((undefined4) (unsigned char) *c);
						m_eventHandler->VTable0x20((undefined4) (unsigned char) *c);
					}
				}
				break;
			case SDL_EVENT_KEY_DOWN:
				// WM_CHAR carried control characters; SDL text input does not, so
				// synthesize the ones the game's text fields react to.
				if (m_eventHandler) {
					undefined4 ch = 0;
					if (event.key.key == SDLK_RETURN) {
						ch = '\r';
					}
					else if (event.key.key == SDLK_BACKSPACE) {
						ch = '\b';
					}
					else if (event.key.key == SDLK_ESCAPE) {
						ch = 0x1b;
					}
					if (ch) {
						m_eventHandler->OnChar(ch);
						m_eventHandler->VTable0x20(ch);
					}
				}
				break;
			case SDL_EVENT_WINDOW_FOCUS_LOST:
				// WM_ACTIVATEAPP(FALSE) equivalent.
				if ((m_flags & c_flagDisplayActive) && !m_disabled) {
					OutputDebugString("Deactivate App\n");
					OnAppDeactivated();
					if (m_eventHandler) {
						m_eventHandler->OnAppDeactivated();
					}
					m_disabled = TRUE;
					m_pollInput = 0;
					m_inputManager.SuspendActiveDevices();
					MiniwinSound_SetSuspended(true);
				}
				break;
			case SDL_EVENT_WINDOW_FOCUS_GAINED:
				// WM_ACTIVATEAPP(TRUE) / WM_SIZE(SIZE_RESTORED) equivalent.
				if ((m_flags & c_flagDisplayActive) && m_disabled) {
					OutputDebugString("Activate App\n");
					OnAppActivated();
					m_disabled = FALSE;
					m_pollInput = 1;
					m_inputManager.RestoreSuspendedDevices();
					MiniwinSound_SetSuspended(false);
					if (m_eventHandler) {
						m_eventHandler->OnAppActivated();
					}
				}
				break;
			default:
				break;
			}
		}

		// The original minimized and idled while deactivated; sleep here so the whole
		// game (race clock included) pauses until focus returns.
		if (m_disabled) {
			Sleep(50);
		}
	} while (m_disabled);

	DWORD time = timeGetTime();
	m_frameDeltaMs = time - m_lastFrameTimeMs;
	if (m_frameDeltaMs > m_maxFrameDeltaMs) {
		m_frameDeltaMs = m_maxFrameDeltaMs;
	}
	else if (m_frameDeltaMs < c_minFrameDeltaMs) {
		Sleep(c_minFrameDeltaMs - m_frameDeltaMs);
		time = timeGetTime();
		m_frameDeltaMs = time - m_lastFrameTimeMs;
	}
	m_lastFrameTimeMs = time;

	if (m_pollInput) {
		m_inputManager.PollDevices(m_frameDeltaMs);
	}

	m_eventHandler = 0;
	return 1;
}

// FUNCTION: LEGORACERS 0x00417000
void Win32GolApp::ChangeWindowState(LegoU32 p_mode)
{
	if (m_windowStateChanging) {
		return;
	}
	if (!m_golDrawState) {
		return;
	}
	if (!(m_golDrawState->GetFlags() & GolDrawState::c_flagCreated)) {
		return;
	}
	if (p_mode == m_windowMode) {
		return;
	}

	m_windowStateChanging = TRUE;

	if (m_eventHandler) {
		m_eventHandler->OnWindowModeChanging();
	}

	m_golDrawState->ReleaseDisplay();

	LegoU32 drawFlags = BuildDrawStateFlags(m_flags) & ~GolDrawState::c_flagShowDeviceDialog;

	OutputDebugString("Changing window state\n");

	if (p_mode == c_windowModeWindowed) {
		OutputDebugString("--to windowed\n");
		m_golDrawState->CreateDisplay(
			m_width,
			m_height,
			m_bpp,
			drawFlags & ~(GolDrawState::c_flagHardwareDevice | GolDrawState::c_flagBit10)
		);

		if (m_golDrawState->m_flags & GolDrawState::c_flagHardwareDevice) {
			OutputDebugString("--from full screen\n");
			m_flags |= c_flagFullscreen;
			m_windowMode = c_windowModeFullscreen;
			GetInputManager()->GetMouse()->SetNonExclusiveMode();
			ApplyWindowMode(m_hWnd, TRUE, m_width, m_height);
		}
		else {
			m_flags &= ~c_flagFullscreen;
			m_windowMode = c_windowModeWindowed;
			ApplyWindowMode(m_hWnd, FALSE, m_width, m_height);
		}

		if (m_eventHandler) {
			m_eventHandler->OnWindowModeChanged();
		}
	}
	else if (p_mode == c_windowModeFullscreen) {
		OutputDebugString("--to full screen\n");
		m_flags |= c_flagFullscreen;
		LegoU32 fullscreenFlags = drawFlags | (GolDrawState::c_flagHardwareDevice | GolDrawState::c_flagBit10);
		m_windowMode = c_windowModeFullscreen;
		GetInputManager()->GetMouse()->SetNonExclusiveMode();
		ApplyWindowMode(m_hWnd, TRUE, m_width, m_height);

		m_golDrawState->CreateDisplay(m_width, m_height, m_bpp, fullscreenFlags);

		if (m_eventHandler) {
			m_eventHandler->OnWindowModeChanged();
		}
	}
	else if (p_mode == c_windowModeMinimized) {
		OutputDebugString("--to minimized\n");
		SDL_Window* window = AppWindow(m_hWnd);
		MiniwinApp_RunOnMainThread([window]() { SDL_MinimizeWindow(window); });
		m_windowMode = c_windowModeMinimized;
	}

	m_windowStateChanging = FALSE;
}

// FUNCTION: LEGORACERS 0x00417900
LegoU32 Win32GolApp::BuildDrawStateFlags(LegoU32 p_flags)
{
	LegoU32 result = 0;
	if (p_flags & c_flagFullscreen) {
		result |= GolDrawState::c_flagHardwareDevice | GolDrawState::c_flagBit10;
	}
	if (p_flags & c_flagBit5) {
		result |= GolDrawState::c_flagBit11;
	}
	if (p_flags & c_flagBit4) {
		result |= GolDrawState::c_flagZBuffer;
	}
	if (p_flags & c_flagBit6) {
		result |= GolDrawState::c_flagAntialias;
	}
	if (p_flags & c_flagPrimaryDriver) {
		result |= GolDrawState::c_flagBit13;
	}
	if (p_flags & c_flagSelect3DDevice) {
		result |= GolDrawState::c_flagShowDeviceDialog;
	}
	if (p_flags & c_flagForceSoftware) {
		result |= GolDrawState::c_flagForceSoftware;
	}
	if (p_flags & c_flagBit17) {
		result |= GolDrawState::c_flagBit17;
	}
	if (p_flags & c_flagDeviceSelected) {
		result |= GolDrawState::c_flagDeviceSelected;
	}
	if (p_flags & c_flagBit12) {
		result |= GolDrawState::c_flagTexturePalettes;
	}
	if (p_flags & c_flagSystemMemorySurfaces) {
		result |= GolDrawState::c_flagSystemMemorySurfaces;
	}
	if (p_flags & c_flagAlphaTransparency) {
		result |= GolDrawState::c_flagPreferAlphaTest;
	}

	return result;
}

// FUNCTION: LEGORACERS 0x00417980
InputManager* Win32GolApp::GetInputManager()
{
	return &m_inputManager;
}
