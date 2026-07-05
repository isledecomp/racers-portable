// [library:config] Emulated registry. The game performs exactly one read: the LangID
// value under HKLM\Software\LEGO Media\Games\LEGO Racers\0.90.000, seeded by the
// --language command-line argument.

#include "miniwin.h"

#include <miniwin/windows.h>

static bool g_langIdSet = false;
static DWORD g_langId = 0;

void MiniwinSetRegistryLangId(DWORD p_langId)
{
	g_langIdSet = true;
	g_langId = p_langId;
}

LSTATUS RegOpenKey(HKEY hKey, LPCSTR lpSubKey, PHKEY phkResult)
{
	if (phkResult) {
		*phkResult = reinterpret_cast<HKEY>(1);
	}
	return ERROR_SUCCESS;
}

LSTATUS RegOpenKeyEx(HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult)
{
	return RegOpenKey(hKey, lpSubKey, phkResult);
}

LSTATUS RegQueryValueEx(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData)
{
	if (lpValueName && SDL_strcasecmp(lpValueName, "LangID") == 0 && g_langIdSet) {
		if (lpType) {
			*lpType = REG_DWORD;
		}
		if (lpData && lpcbData && *lpcbData >= sizeof(DWORD)) {
			SDL_memcpy(lpData, &g_langId, sizeof(DWORD));
			*lpcbData = sizeof(DWORD);
		}
		return ERROR_SUCCESS;
	}

	return ERROR_FILE_NOT_FOUND;
}

LSTATUS RegCloseKey(HKEY hKey)
{
	return ERROR_SUCCESS;
}
