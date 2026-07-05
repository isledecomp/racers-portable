#include "miniwin.h"

#include <SDL3/SDL.h>
#include <miniwin/miniwinapp.h>
#include <miniwin/objbase.h>
#include <miniwin/windows.h>

// --- IUnknown ---

ULONG IUnknown::AddRef()
{
	m_refCount += 1;
	return m_refCount;
}

ULONG IUnknown::Release()
{
	m_refCount -= 1;
	int refCount = m_refCount;
	if (refCount == 0) {
		delete this;
	}
	return refCount;
}

HRESULT IUnknown::QueryInterface(REFIID riid, void** ppvObject)
{
	MINIWIN_NOT_IMPLEMENTED();
	if (ppvObject) {
		*ppvObject = nullptr;
	}
	return E_NOINTERFACE;
}

// --- Window management ---
//
// HWND is an SDL_Window* in disguise; the rewritten application shell creates the
// window through SDL directly, so most of these entry points only exist for the few
// remaining decomp callers.

static int g_showCursorCount = 0;

ATOM RegisterClass(const WNDCLASSA* lpWndClass)
{
	return 1;
}

HWND CreateWindowEx(
	DWORD dwExStyle,
	LPCSTR lpClassName,
	LPCSTR lpWindowName,
	DWORD dwStyle,
	int X,
	int Y,
	int nWidth,
	int nHeight,
	HWND hWndParent,
	HMENU hMenu,
	HINSTANCE hInstance,
	LPVOID lpParam
)
{
	MINIWIN_NOT_IMPLEMENTED();
	return nullptr;
}

BOOL DestroyWindow(HWND hWnd)
{
	return TRUE;
}

BOOL ShowWindow(HWND hWnd, int nCmdShow)
{
	return TRUE;
}

BOOL UpdateWindow(HWND hWnd)
{
	return TRUE;
}

HWND SetFocus(HWND hWnd)
{
	return hWnd;
}

BOOL SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags)
{
	return TRUE;
}

LONG GetWindowLong(HWND hWnd, int nIndex)
{
	MINIWIN_NOT_IMPLEMENTED();
	return 0;
}

LONG SetWindowLong(HWND hWnd, int nIndex, LONG dwNewLong)
{
	MINIWIN_NOT_IMPLEMENTED();
	return 0;
}

LONG_PTR GetWindowLongPtrA(HWND hWnd, int nIndex)
{
	return 0;
}

LONG_PTR SetWindowLongPtrA(HWND hWnd, int nIndex, LONG_PTR dwNewLong)
{
	return 0;
}

BOOL AdjustWindowRect(LPRECT lpRect, DWORD dwStyle, BOOL bMenu)
{
	return TRUE;
}

BOOL GetClientRect(HWND hWnd, LPRECT lpRect)
{
	if (!lpRect) {
		return FALSE;
	}

	int w = 0;
	int h = 0;
	if (hWnd) {
		SDL_GetWindowSize(reinterpret_cast<SDL_Window*>(hWnd), &w, &h);
	}

	lpRect->left = 0;
	lpRect->top = 0;
	lpRect->right = w;
	lpRect->bottom = h;
	return TRUE;
}

BOOL GetWindowRect(HWND hWnd, LPRECT lpRect)
{
	if (!lpRect) {
		return FALSE;
	}

	int x = 0;
	int y = 0;
	int w = 0;
	int h = 0;
	if (hWnd) {
		SDL_Window* window = reinterpret_cast<SDL_Window*>(hWnd);
		SDL_GetWindowPosition(window, &x, &y);
		SDL_GetWindowSize(window, &w, &h);
	}

	lpRect->left = x;
	lpRect->top = y;
	lpRect->right = x + w;
	lpRect->bottom = y + h;
	return TRUE;
}

LRESULT DefWindowProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	return 0;
}

void PostQuitMessage(int nExitCode)
{
	SDL_Event event;
	SDL_zero(event);
	event.type = SDL_EVENT_QUIT;
	SDL_PushEvent(&event);
}

BOOL PeekMessage(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg)
{
	MINIWIN_NOT_IMPLEMENTED();
	return FALSE;
}

BOOL TranslateMessage(const MSG* lpMsg)
{
	return FALSE;
}

LRESULT DispatchMessage(const MSG* lpMsg)
{
	return 0;
}

int GetSystemMetrics(int nIndex)
{
	const SDL_DisplayMode* mode = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay());
	if (!mode) {
		return 0;
	}

	switch (nIndex) {
	case SM_CXSCREEN:
		return mode->w;
	case SM_CYSCREEN:
		return mode->h;
	default:
		return 0;
	}
}

// --- Dialogs (device picker) ---

INT_PTR DialogBoxIndirectParam(
	HINSTANCE hInstance,
	LPCDLGTEMPLATE lpTemplate,
	HWND hWndParent,
	DLGPROC lpDialogFunc,
	LPARAM dwInitParam
)
{
	// The -select3d device-picker dialog. miniwin exposes a single device, so the
	// picker would be a no-op; report "cancel" and let the game use the default.
	MINIWIN_NOT_IMPLEMENTED();
	return IDCANCEL;
}

BOOL EndDialog(HWND hDlg, INT_PTR nResult)
{
	return TRUE;
}

LRESULT SendDlgItemMessage(HWND hDlg, int nIDDlgItem, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	return 0;
}

LRESULT SendMessage(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
	return 0;
}

HWND GetDlgItem(HWND hDlg, int nIDDlgItem)
{
	return nullptr;
}

// --- Cursor / icon / GDI ---

BOOL GetCursorPos(LPPOINT lpPoint)
{
	if (!lpPoint) {
		return FALSE;
	}

	float x = 0.f;
	float y = 0.f;
	SDL_GetGlobalMouseState(&x, &y);
	lpPoint->x = (LONG) x;
	lpPoint->y = (LONG) y;
	return TRUE;
}

BOOL ClientToScreen(HWND hWnd, LPPOINT lpPoint)
{
	if (!lpPoint) {
		return FALSE;
	}

	int x = 0;
	int y = 0;
	if (hWnd) {
		SDL_GetWindowPosition(reinterpret_cast<SDL_Window*>(hWnd), &x, &y);
	}

	lpPoint->x += x;
	lpPoint->y += y;
	return TRUE;
}

BOOL ScreenToClient(HWND hWnd, LPPOINT lpPoint)
{
	if (!lpPoint) {
		return FALSE;
	}

	int x = 0;
	int y = 0;
	if (hWnd) {
		SDL_GetWindowPosition(reinterpret_cast<SDL_Window*>(hWnd), &x, &y);
	}

	lpPoint->x -= x;
	lpPoint->y -= y;
	return TRUE;
}

HCURSOR SetCursor(HCURSOR hCursor)
{
	return hCursor;
}

HCURSOR LoadCursor(HINSTANCE hInstance, LPCSTR lpCursorName)
{
	return reinterpret_cast<HCURSOR>(const_cast<char*>(lpCursorName));
}

int ShowCursor(BOOL bShow)
{
	g_showCursorCount += bShow ? 1 : -1;
	if (g_showCursorCount >= 0) {
		SDL_ShowCursor();
	}
	else {
		SDL_HideCursor();
	}
	return g_showCursorCount;
}

HICON LoadIcon(HINSTANCE hInstance, LPCSTR lpIconName)
{
	return reinterpret_cast<HICON>(const_cast<char*>(lpIconName));
}

HGDIOBJ GetStockObject(int i)
{
	return nullptr;
}

HDC GetDC(HWND hWnd)
{
	return nullptr;
}

int ReleaseDC(HWND hWnd, HDC hDC)
{
	return 1;
}

int GetDeviceCaps(HDC hdc, int index)
{
	if (index == BITSPIXEL) {
		return 32;
	}
	return 0;
}

// --- Misc ---

int MessageBox(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType)
{
	SDL_MessageBoxFlags flags = SDL_MESSAGEBOX_INFORMATION;
	if (uType & MB_ICONERROR) {
		flags = SDL_MESSAGEBOX_ERROR;
	}
	else if (uType & MB_ICONWARNING) {
		flags = SDL_MESSAGEBOX_WARNING;
	}

	SDL_LogError(LOG_CATEGORY_MINIWIN, "%s: %s", lpCaption ? lpCaption : "", lpText ? lpText : "");

	// Message boxes must run on the main thread (macOS); the game calls this from
	// the game thread (fatal errors, usage).
	MiniwinApp_RunOnMainThread([&]() {
		SDL_ShowSimpleMessageBox(flags, lpCaption, lpText, hWnd ? reinterpret_cast<SDL_Window*>(hWnd) : nullptr);
	});
	return IDOK;
}

void OutputDebugString(LPCSTR lpOutputString)
{
	SDL_LogDebug(LOG_CATEGORY_MINIWIN, "%s", lpOutputString ? lpOutputString : "");
}

DWORD GetLastError(void)
{
	return 0;
}

int lstrlen(LPCSTR lpString)
{
	return lpString ? (int) SDL_strlen(lpString) : 0;
}

LPSTR lstrcpy(LPSTR lpString1, LPCSTR lpString2)
{
	if (!lpString1 || !lpString2) {
		return lpString1;
	}
	SDL_strlcpy(lpString1, lpString2, SDL_strlen(lpString2) + 1);
	return lpString1;
}

LPSTR lstrcat(LPSTR lpString1, LPCSTR lpString2)
{
	if (!lpString1 || !lpString2) {
		return lpString1;
	}
	SDL_strlcat(lpString1, lpString2, SDL_strlen(lpString1) + SDL_strlen(lpString2) + 1);
	return lpString1;
}

void Sleep(DWORD dwMilliseconds)
{
	// The game's frame limiter sleeps for the remainder of the 12 ms frame budget.
	// SDL_Delay overshoots by several milliseconds on macOS, dragging the frame rate
	// well below the original's ~83 FPS cap; the original ran with timeBeginPeriod(1)
	// precision, which SDL_DelayPrecise matches.
	SDL_DelayPrecise((Uint64) dwMilliseconds * 1000000ull);
}

int MultiByteToWideChar(
	UINT CodePage,
	DWORD dwFlags,
	LPCSTR lpMultiByteStr,
	int cbMultiByte,
	LPWSTR lpWideCharStr,
	int cchWideChar
)
{
	if (!lpMultiByteStr) {
		return 0;
	}

	int length = cbMultiByte < 0 ? (int) SDL_strlen(lpMultiByteStr) + 1 : cbMultiByte;
	if (!lpWideCharStr || cchWideChar == 0) {
		return length;
	}

	int count = length < cchWideChar ? length : cchWideChar;
	for (int i = 0; i < count; i++) {
		lpWideCharStr[i] = (WCHAR) (unsigned char) lpMultiByteStr[i];
	}

	return count;
}

LONG ChangeDisplaySettings(LPDEVMODEA lpDevMode, DWORD dwFlags)
{
	// SDL owns display modes; the game only calls this to restore the desktop mode.
	return DISP_CHANGE_SUCCESSFUL;
}

BOOL EnumDisplaySettings(LPCSTR lpszDeviceName, DWORD iModeNum, LPDEVMODEA lpDevMode)
{
	MINIWIN_NOT_IMPLEMENTED();
	return FALSE;
}

HANDLE GetCurrentProcess(void)
{
	return reinterpret_cast<HANDLE>(-2);
}

BOOL SetPriorityClass(HANDLE hProcess, DWORD dwPriorityClass)
{
	return TRUE;
}

DWORD GetCurrentProcessId(void)
{
	return 1;
}

HWND GetForegroundWindow(void)
{
	return nullptr;
}

HWND GetActiveWindow(void)
{
	return nullptr;
}

DWORD GetWindowThreadProcessId(HWND hWnd, LPDWORD lpdwProcessId)
{
	// Single-process world: every window belongs to us.
	if (lpdwProcessId) {
		*lpdwProcessId = GetCurrentProcessId();
	}
	return 1;
}

#ifndef _WIN32
// The Windows CRT provides stricmp; this shim is for POSIX systems.
int stricmp(const char* string1, const char* string2)
{
	return SDL_strcasecmp(string1, string2);
}
#endif

// --- COM stubs ---

HRESULT CoInitialize(LPVOID pvReserved)
{
	return S_OK;
}

void CoUninitialize(void)
{
}

HRESULT CoCreateInstance(REFCLSID rclsid, LPUNKNOWN pUnkOuter, DWORD dwClsContext, REFIID riid, LPVOID* ppv)
{
	MINIWIN_NOT_IMPLEMENTED();
	if (ppv) {
		*ppv = nullptr;
	}
	return E_NOINTERFACE;
}
