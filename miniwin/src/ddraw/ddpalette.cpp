// [library:2d] DirectDraw palette emulation (plain backing store).

#include "ddraw_impl.h"
#include "miniwin.h"

#include <string.h>

IDirectDrawPalette::IDirectDrawPalette(DWORD dwFlags, const PALETTEENTRY* lpEntries)
{
	m_flags = dwFlags;
	memset(m_entries, 0, sizeof(m_entries));

	DWORD count = 256;
	if (dwFlags & DDPCAPS_1BIT) {
		count = 2;
	}
	else if (dwFlags & DDPCAPS_2BIT) {
		count = 4;
	}
	else if (dwFlags & DDPCAPS_4BIT) {
		count = 16;
	}

	if (lpEntries) {
		memcpy(m_entries, lpEntries, count * sizeof(PALETTEENTRY));
	}
}

HRESULT IDirectDrawPalette::GetCaps(LPDWORD lpdwCaps)
{
	if (lpdwCaps) {
		*lpdwCaps = m_flags;
	}
	return DD_OK;
}

HRESULT IDirectDrawPalette::GetEntries(DWORD dwFlags, DWORD dwBase, DWORD dwNumEntries, LPPALETTEENTRY lpEntries)
{
	if (!lpEntries || dwBase + dwNumEntries > 256) {
		return DDERR_INVALIDPARAMS;
	}

	memcpy(lpEntries, m_entries + dwBase, dwNumEntries * sizeof(PALETTEENTRY));
	return DD_OK;
}

HRESULT IDirectDrawPalette::SetEntries(DWORD dwFlags, DWORD dwStartingEntry, DWORD dwCount, const PALETTEENTRY* lpEntries)
{
	if (!lpEntries || dwStartingEntry + dwCount > 256) {
		return DDERR_INVALIDPARAMS;
	}

	memcpy(m_entries + dwStartingEntry, lpEntries, dwCount * sizeof(PALETTEENTRY));
	m_version++;
	return DD_OK;
}
