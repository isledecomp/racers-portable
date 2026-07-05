#pragma once

// [library:audio] WinMM subset (timers, wave format) over SDL3.

#include "windows.h"

#define TIMERR_NOERROR 0
#define TIME_ONESHOT 0x0000
#define TIME_PERIODIC 0x0001

typedef void(CALLBACK* LPTIMECALLBACK)(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1, DWORD_PTR dw2);

DWORD timeGetTime(void);
MMRESULT timeSetEvent(UINT uDelay, UINT uResolution, LPTIMECALLBACK fptc, DWORD_PTR dwUser, UINT fuEvent);
MMRESULT timeKillEvent(UINT uTimerID);

#ifndef WAVE_FORMAT_PCM
#define WAVE_FORMAT_PCM 1

#pragma pack(push, 1)
struct WAVEFORMATEX {
	WORD wFormatTag;
	WORD nChannels;
	DWORD nSamplesPerSec;
	DWORD nAvgBytesPerSec;
	WORD nBlockAlign;
	WORD wBitsPerSample;
	WORD cbSize;
};
#pragma pack(pop)
typedef WAVEFORMATEX* LPWAVEFORMATEX;
typedef const WAVEFORMATEX* LPCWAVEFORMATEX;

#endif // WAVE_FORMAT_PCM
