#pragma once

// [library:window] Minimal Win32 API surface for the decompiled LEGO Racers code,
// implemented over SDL3. Modeled on isle-portable's miniwin.

#include <float.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// --- Calling conventions / storage macros ---
#define WINAPI
#define CALLBACK
#define WINUSERAPI
#define APIENTRY
#define FAR
#define NEAR
#define PASCAL
#define STDMETHODCALLTYPE
#define VOID void
#define CONST const

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

// --- Scalar types ---
typedef uint8_t BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef int32_t LONG;
typedef uint32_t ULONG;
typedef int BOOL;
typedef int INT;
typedef unsigned int UINT;
typedef char CHAR;
typedef unsigned char UCHAR;
typedef short SHORT;
typedef unsigned short USHORT;
typedef float FLOAT;
typedef double DOUBLE;
typedef int32_t HRESULT;
typedef uint16_t ATOM;

typedef intptr_t INT_PTR;
typedef intptr_t LONG_PTR;
typedef uintptr_t UINT_PTR;
typedef uintptr_t ULONG_PTR;
typedef uintptr_t DWORD_PTR;
typedef UINT_PTR WPARAM;
typedef LONG_PTR LPARAM;
typedef LONG_PTR LRESULT;
typedef LONG LSTATUS;
typedef DWORD REGSAM;
typedef DWORD MMRESULT;

typedef void* LPVOID;
typedef const void* LPCVOID;
typedef char* LPSTR;
typedef char* PSTR;
typedef const char* LPCSTR;
typedef const char* PCSTR;
typedef BYTE* LPBYTE;
typedef WORD* LPWORD;
typedef DWORD* LPDWORD;
typedef LONG* LPLONG;
typedef BOOL* LPBOOL;
typedef INT* LPINT;
typedef UINT* LPUINT;

// --- Handles ---
typedef void* HANDLE;
typedef HANDLE* LPHANDLE;
typedef void* HGDIOBJ;

#define DECLARE_HANDLE(name)                                                                                           \
	struct name##__ {                                                                                                  \
		int unused;                                                                                                    \
	};                                                                                                                 \
	typedef struct name##__* name

DECLARE_HANDLE(HWND);
DECLARE_HANDLE(HINSTANCE);
DECLARE_HANDLE(HICON);
DECLARE_HANDLE(HDC);
DECLARE_HANDLE(HMENU);
DECLARE_HANDLE(HBRUSH);
DECLARE_HANDLE(HPALETTE);
DECLARE_HANDLE(HKEY);
DECLARE_HANDLE(HMONITOR);

typedef HINSTANCE HMODULE;
typedef HICON HCURSOR;
typedef HKEY* PHKEY;

#define INVALID_HANDLE_VALUE ((HANDLE) (intptr_t) -1)

// --- GUIDs / COM basics ---
struct GUID {
	DWORD Data1;
	WORD Data2;
	WORD Data3;
	BYTE Data4[8];
};
typedef GUID IID;
typedef GUID CLSID;
typedef GUID* LPGUID;
typedef const GUID* LPCGUID;
typedef const GUID& REFGUID;
typedef const GUID& REFIID;
typedef const GUID& REFCLSID;

#define DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8)                                                   \
	extern const GUID name
#define MINIWIN_DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8)                                           \
	const GUID name = {l, w1, w2, {b1, b2, b3, b4, b5, b6, b7, b8}}

inline bool IsEqualGUID(REFGUID a, REFGUID b)
{
	return memcmp(&a, &b, sizeof(GUID)) == 0;
}
inline bool operator==(REFGUID a, REFGUID b)
{
	return IsEqualGUID(a, b);
}
inline bool operator!=(REFGUID a, REFGUID b)
{
	return !IsEqualGUID(a, b);
}

struct IUnknown {
	IUnknown() : m_refCount(1) {}
	virtual ULONG AddRef();
	virtual ULONG Release();
	virtual HRESULT QueryInterface(REFIID riid, void** ppvObject);
	virtual ~IUnknown() = default;

protected:
	int m_refCount;
};
typedef IUnknown* LPUNKNOWN;

// --- HRESULT / error codes ---
#define S_OK ((HRESULT) 0)
#define S_FALSE ((HRESULT) 1)
#define E_FAIL ((HRESULT) 0x80004005)
#define E_NOINTERFACE ((HRESULT) 0x80004002)
#define E_NOTIMPL ((HRESULT) 0x80004001)
#define E_OUTOFMEMORY ((HRESULT) 0x8007000E)
#define SUCCEEDED(hr) (((HRESULT) (hr)) >= 0)
#define FAILED(hr) (((HRESULT) (hr)) < 0)

#define ERROR_SUCCESS 0L
#define ERROR_FILE_NOT_FOUND 2L
#define NO_ERROR 0L

// --- Basic structs ---
struct POINT {
	LONG x;
	LONG y;
};
typedef POINT* LPPOINT;

struct SIZE {
	LONG cx;
	LONG cy;
};
typedef SIZE* LPSIZE;

struct RECT {
	LONG left;
	LONG top;
	LONG right;
	LONG bottom;
};
typedef RECT* LPRECT;
typedef const RECT* LPCRECT;

struct PALETTEENTRY {
	BYTE peRed;
	BYTE peGreen;
	BYTE peBlue;
	BYTE peFlags;
};
typedef PALETTEENTRY* LPPALETTEENTRY;

#define PC_RESERVED 0x01
#define PC_EXPLICIT 0x02
#define PC_NOCOLLAPSE 0x04

typedef void* LPSECURITY_ATTRIBUTES;
typedef void* LPOVERLAPPED;

struct DEVMODEA {
	BYTE dmDeviceName[32];
	WORD dmSpecVersion;
	WORD dmDriverVersion;
	WORD dmSize;
	WORD dmDriverExtra;
	DWORD dmFields;
	DWORD dmBitsPerPel;
	DWORD dmPelsWidth;
	DWORD dmPelsHeight;
	DWORD dmDisplayFlags;
	DWORD dmDisplayFrequency;
};
typedef DEVMODEA DEVMODE;
typedef DEVMODEA* LPDEVMODEA;
typedef DEVMODEA* LPDEVMODE;

#define DM_BITSPERPEL 0x00040000L
#define DM_PELSWIDTH 0x00080000L
#define DM_PELSHEIGHT 0x00100000L

// --- Window types ---
typedef LRESULT(CALLBACK* WNDPROC)(HWND, UINT, WPARAM, LPARAM);
typedef INT_PTR(CALLBACK* DLGPROC)(HWND, UINT, WPARAM, LPARAM);

#pragma pack(push, 2)
struct DLGTEMPLATE {
	DWORD style;
	DWORD dwExtendedStyle;
	WORD cdit;
	short x;
	short y;
	short cx;
	short cy;
};

struct DLGITEMTEMPLATE {
	DWORD style;
	DWORD dwExtendedStyle;
	short x;
	short y;
	short cx;
	short cy;
	WORD id;
};
#pragma pack(pop)
typedef DLGTEMPLATE* LPDLGTEMPLATE;
typedef const DLGTEMPLATE* LPCDLGTEMPLATE;
typedef DLGITEMTEMPLATE* LPDLGITEMTEMPLATE;

#define DS_3DLOOK 0x0004L
#define DS_SETFONT 0x0040L
#define DS_MODALFRAME 0x0080L
#define DS_NOIDLEMSG 0x0100L
#define DS_SETFOREGROUND 0x0200L
#define DS_CENTER 0x0800L

typedef wchar_t WCHAR;
typedef WCHAR* LPWSTR;
typedef const WCHAR* LPCWSTR;
#define CP_ACP 0

int MultiByteToWideChar(UINT CodePage, DWORD dwFlags, LPCSTR lpMultiByteStr, int cbMultiByte, LPWSTR lpWideCharStr, int cchWideChar);

struct WNDCLASSA {
	UINT style;
	WNDPROC lpfnWndProc;
	int cbClsExtra;
	int cbWndExtra;
	HINSTANCE hInstance;
	HICON hIcon;
	HCURSOR hCursor;
	HBRUSH hbrBackground;
	LPCSTR lpszMenuName;
	LPCSTR lpszClassName;
};
typedef WNDCLASSA WNDCLASS;
typedef WNDCLASSA* LPWNDCLASSA;

struct MSG {
	HWND hwnd;
	UINT message;
	WPARAM wParam;
	LPARAM lParam;
	DWORD time;
	POINT pt;
};
typedef MSG* LPMSG;

// --- Macros ---
#define CONTAINING_RECORD(address, type, field) ((type*) ((char*) (address) - offsetof(type, field)))

#define MAKEINTRESOURCE(i) ((LPSTR) (ULONG_PTR) ((WORD) (i)))
#define LOWORD(l) ((WORD) (((DWORD_PTR) (l)) & 0xffff))
#define HIWORD(l) ((WORD) ((((DWORD_PTR) (l)) >> 16) & 0xffff))
#define LOBYTE(w) ((BYTE) (((DWORD_PTR) (w)) & 0xff))
#define HIBYTE(w) ((BYTE) ((((DWORD_PTR) (w)) >> 8) & 0xff))
#define MAKEWORD(a, b) ((WORD) (((BYTE) (a)) | ((WORD) ((BYTE) (b))) << 8))
#define MAKELONG(a, b) ((LONG) (((WORD) (a)) | ((DWORD) ((WORD) (b))) << 16))
#define RGB(r, g, b) ((DWORD) (((BYTE) (r) | ((WORD) ((BYTE) (g)) << 8)) | (((DWORD) (BYTE) (b)) << 16)))

// --- Window messages ---
#define WM_NULL 0x0000
#define WM_CREATE 0x0001
#define WM_DESTROY 0x0002
#define WM_MOVE 0x0003
#define WM_SIZE 0x0005
#define WM_ACTIVATE 0x0006
#define WM_SETFOCUS 0x0007
#define WM_KILLFOCUS 0x0008
#define WM_PAINT 0x000F
#define WM_CLOSE 0x0010
#define WM_QUIT 0x0012
#define WM_ACTIVATEAPP 0x001C
#define WM_SETCURSOR 0x0020
#define WM_KEYDOWN 0x0100
#define WM_KEYUP 0x0101
#define WM_CHAR 0x0102
#define WM_SYSKEYDOWN 0x0104
#define WM_SYSKEYUP 0x0105
#define WM_INITDIALOG 0x0110
#define WM_COMMAND 0x0111
#define WM_SYSCOMMAND 0x0112
#define WM_TIMER 0x0113
#define WM_SIZING 0x0214
#define WM_MOVING 0x0216
#define WM_QUERYNEWPALETTE 0x030F
#define WM_PALETTECHANGED 0x0311
#define WM_USER 0x0400

#define WA_INACTIVE 0
#define WA_ACTIVE 1
#define WA_CLICKACTIVE 2

#define SIZE_RESTORED 0
#define SIZE_MINIMIZED 1
#define SIZE_MAXIMIZED 2

#define SC_SCREENSAVE 0xF140

#define PM_NOREMOVE 0x0000
#define PM_REMOVE 0x0001

// --- Window styles ---
#define WS_OVERLAPPED 0x00000000L
#define WS_POPUP 0x80000000L
#define WS_VISIBLE 0x10000000L
#define WS_CAPTION 0x00C00000L
#define WS_BORDER 0x00800000L
#define WS_DLGFRAME 0x00400000L
#define WS_SYSMENU 0x00080000L
#define WS_THICKFRAME 0x00040000L
#define WS_MINIMIZEBOX 0x00020000L
#define WS_MAXIMIZEBOX 0x00010000L
#define WS_CLIPCHILDREN 0x02000000L
#define WS_VSCROLL 0x00200000L
#define WS_CHILD 0x40000000L
#define WS_TABSTOP 0x00010000L
#define WS_OVERLAPPEDWINDOW                                                                                            \
	(WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)

#define CBS_DROPDOWNLIST 0x0003L
#define CB_ADDSTRING 0x0143
#define CB_RESETCONTENT 0x014B
#define CB_SETCURSEL 0x014E
#define CB_GETCURSEL 0x0147
#define CB_SETITEMDATA 0x0151
#define CB_GETITEMDATA 0x0150
#define CBN_SELENDOK 9
#define CB_ERR (-1)
#define SS_LEFT 0x00000000L

#define CS_VREDRAW 0x0001
#define CS_HREDRAW 0x0002

#define SW_HIDE 0
#define SW_SHOWNORMAL 1
#define SW_MAXIMIZE 3
#define SW_SHOW 5
#define SW_MINIMIZE 6
#define SW_SHOWMINNOACTIVE 7
#define SW_RESTORE 9

#define GWL_STYLE (-16)
#define GWL_EXSTYLE (-20)
#define GWLP_HINSTANCE (-6)

#define SWP_NOSIZE 0x0001
#define SWP_NOMOVE 0x0002
#define SWP_NOZORDER 0x0004
#define SWP_NOACTIVATE 0x0010
#define SWP_SHOWWINDOW 0x0040
#define HWND_TOP ((HWND) 0)
#define HWND_TOPMOST ((HWND) (intptr_t) -1)
#define HWND_NOTOPMOST ((HWND) (intptr_t) -2)

// --- MessageBox ---
#define MB_OK 0x00000000L
#define MB_OKCANCEL 0x00000001L
#define MB_ICONERROR 0x00000010L
#define MB_ICONWARNING 0x00000030L
#define MB_ICONINFORMATION 0x00000040L
#define MB_SETFOREGROUND 0x00010000L
#define MB_TOPMOST 0x00040000L

#define IDOK 1
#define IDCANCEL 2

// --- Cursors / icons / GDI ---
#define IDC_ARROW MAKEINTRESOURCE(32512)
#define IDC_WAIT MAKEINTRESOURCE(32514)
#define BLACK_BRUSH 4
#define BITSPIXEL 12

// --- System metrics ---
#define SM_CXSCREEN 0
#define SM_CYSCREEN 1

// --- Dialog / listbox messages (device-picker dialog) ---
#define LB_ADDSTRING 0x0180
#define LB_RESETCONTENT 0x0184
#define LB_SETCURSEL 0x0186
#define LB_GETCURSEL 0x0188
#define LB_ERR (-1)
#define BN_CLICKED 0

// --- Synchronization ---
#define INFINITE 0xFFFFFFFF
#define WAIT_OBJECT_0 0
#define WAIT_ABANDONED 0x00000080L
#define WAIT_TIMEOUT 0x00000102L
#define WAIT_FAILED 0xFFFFFFFF

// --- File API ---
#define GENERIC_READ 0x80000000L
#define GENERIC_WRITE 0x40000000L
#define FILE_SHARE_READ 0x00000001
#define FILE_SHARE_WRITE 0x00000002
#define CREATE_ALWAYS 2
#define OPEN_EXISTING 3
#define FILE_ATTRIBUTE_NORMAL 0x00000080
#define FILE_FLAG_SEQUENTIAL_SCAN 0x08000000
#define INVALID_FILE_ATTRIBUTES ((DWORD) -1)
#define FILE_BEGIN 0
#define FILE_CURRENT 1
#define FILE_END 2
#define INVALID_SET_FILE_POINTER ((DWORD) -1)
#define INVALID_FILE_SIZE ((DWORD) 0xFFFFFFFF)

// --- Registry ---
#define HKEY_CLASSES_ROOT ((HKEY) (ULONG_PTR) 0x80000000)
#define HKEY_CURRENT_USER ((HKEY) (ULONG_PTR) 0x80000001)
#define HKEY_LOCAL_MACHINE ((HKEY) (ULONG_PTR) 0x80000002)
#define REG_NONE 0
#define REG_SZ 1
#define REG_BINARY 3
#define REG_DWORD 4
#define KEY_READ 0x20019
#define KEY_ALL_ACCESS 0xF003F

// --- Display settings ---
#define DISP_CHANGE_SUCCESSFUL 0
#define DISP_CHANGE_FAILED (-1)
#define CDS_FULLSCREEN 0x00000004
#define CDS_RESET 0x40000000
#define ENUM_CURRENT_SETTINGS ((DWORD) -1)

// --- Priority classes ---
#define NORMAL_PRIORITY_CLASS 0x00000020
#define HIGH_PRIORITY_CLASS 0x00000080

// --- Functions: window management ---
ATOM RegisterClass(const WNDCLASSA* lpWndClass);
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
);
BOOL DestroyWindow(HWND hWnd);
BOOL ShowWindow(HWND hWnd, int nCmdShow);
BOOL UpdateWindow(HWND hWnd);
HWND SetFocus(HWND hWnd);
BOOL SetWindowPos(HWND hWnd, HWND hWndInsertAfter, int X, int Y, int cx, int cy, UINT uFlags);
LONG GetWindowLong(HWND hWnd, int nIndex);
LONG SetWindowLong(HWND hWnd, int nIndex, LONG dwNewLong);
LONG_PTR GetWindowLongPtrA(HWND hWnd, int nIndex);
LONG_PTR SetWindowLongPtrA(HWND hWnd, int nIndex, LONG_PTR dwNewLong);
#define GetWindowLongPtr GetWindowLongPtrA
#define SetWindowLongPtr SetWindowLongPtrA
BOOL AdjustWindowRect(LPRECT lpRect, DWORD dwStyle, BOOL bMenu);
BOOL GetClientRect(HWND hWnd, LPRECT lpRect);
BOOL GetWindowRect(HWND hWnd, LPRECT lpRect);
LRESULT DefWindowProc(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
void PostQuitMessage(int nExitCode);
BOOL PeekMessage(LPMSG lpMsg, HWND hWnd, UINT wMsgFilterMin, UINT wMsgFilterMax, UINT wRemoveMsg);
BOOL TranslateMessage(const MSG* lpMsg);
LRESULT DispatchMessage(const MSG* lpMsg);
int GetSystemMetrics(int nIndex);

// --- Functions: dialogs ---
INT_PTR DialogBoxIndirectParam(HINSTANCE hInstance, LPCDLGTEMPLATE lpTemplate, HWND hWndParent, DLGPROC lpDialogFunc, LPARAM dwInitParam);
BOOL EndDialog(HWND hDlg, INT_PTR nResult);
LRESULT SendDlgItemMessage(HWND hDlg, int nIDDlgItem, UINT Msg, WPARAM wParam, LPARAM lParam);
LRESULT SendMessage(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
HWND GetDlgItem(HWND hDlg, int nIDDlgItem);

// --- Functions: cursor / icon / GDI ---
BOOL GetCursorPos(LPPOINT lpPoint);
BOOL ClientToScreen(HWND hWnd, LPPOINT lpPoint);
BOOL ScreenToClient(HWND hWnd, LPPOINT lpPoint);
HCURSOR SetCursor(HCURSOR hCursor);
HCURSOR LoadCursor(HINSTANCE hInstance, LPCSTR lpCursorName);
int ShowCursor(BOOL bShow);
HICON LoadIcon(HINSTANCE hInstance, LPCSTR lpIconName);
HGDIOBJ GetStockObject(int i);
HDC GetDC(HWND hWnd);
int ReleaseDC(HWND hWnd, HDC hDC);
int GetDeviceCaps(HDC hdc, int index);

// --- Functions: synchronization ---
HANDLE CreateMutex(LPSECURITY_ATTRIBUTES lpMutexAttributes, BOOL bInitialOwner, LPCSTR lpName);
DWORD WaitForSingleObject(HANDLE hHandle, DWORD dwMilliseconds);
BOOL ReleaseMutex(HANDLE hMutex);
BOOL CloseHandle(HANDLE hObject);

// --- Functions: files ---
HANDLE CreateFile(
	LPCSTR lpFileName,
	DWORD dwDesiredAccess,
	DWORD dwShareMode,
	LPSECURITY_ATTRIBUTES lpSecurityAttributes,
	DWORD dwCreationDisposition,
	DWORD dwFlagsAndAttributes,
	HANDLE hTemplateFile
);
BOOL ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped);
BOOL WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped);
DWORD SetFilePointer(HANDLE hFile, LONG lDistanceToMove, LPLONG lpDistanceToMoveHigh, DWORD dwMoveMethod);
DWORD GetFileSize(HANDLE hFile, LPDWORD lpFileSizeHigh);
DWORD GetFileAttributes(LPCSTR lpFileName);

// --- Functions: registry ---
LSTATUS RegOpenKey(HKEY hKey, LPCSTR lpSubKey, PHKEY phkResult);
LSTATUS RegOpenKeyEx(HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult);
LSTATUS RegQueryValueEx(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData);
LSTATUS RegCloseKey(HKEY hKey);

// --- Functions: misc ---
int MessageBox(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType);
void OutputDebugString(LPCSTR lpOutputString);
DWORD GetLastError(void);
int lstrlen(LPCSTR lpString);
LPSTR lstrcpy(LPSTR lpString1, LPCSTR lpString2);
LPSTR lstrcat(LPSTR lpString1, LPCSTR lpString2);
void Sleep(DWORD dwMilliseconds);
LONG ChangeDisplaySettings(LPDEVMODEA lpDevMode, DWORD dwFlags);
BOOL EnumDisplaySettings(LPCSTR lpszDeviceName, DWORD iModeNum, LPDEVMODEA lpDevMode);
HANDLE GetCurrentProcess(void);
BOOL SetPriorityClass(HANDLE hProcess, DWORD dwPriorityClass);
DWORD GetCurrentProcessId(void);
HWND GetForegroundWindow(void);
HWND GetActiveWindow(void);
DWORD GetWindowThreadProcessId(HWND hWnd, LPDWORD lpdwProcessId);
int stricmp(const char* string1, const char* string2);

#define ZeroMemory(Destination, Length) memset((Destination), 0, (Length))
#define CopyMemory(Destination, Source, Length) memcpy((Destination), (Source), (Length))
