#pragma once

// [library:audio] DirectSound (DirectX 6) subset used by the game's audio system,
// implemented over a miniaudio no-device engine mixed into an SDL3 audio stream.
// The game uses no 3D buffers and no notifications; positional audio is computed by
// game code via pan/volume/frequency.

#include "mmsystem.h"
#include "windows.h"

#define DS_OK ((HRESULT) 0)
#define MAKE_DSHRESULT(code) ((HRESULT) (0x88780000 | (code)))
#define DSERR_INVALIDPARAM E_FAIL
#define DSERR_BUFFERLOST MAKE_DSHRESULT(150)
#define DSERR_GENERIC E_FAIL

// Cooperative levels
#define DSSCL_NORMAL 1
#define DSSCL_PRIORITY 2
#define DSSCL_EXCLUSIVE 3
#define DSSCL_WRITEPRIMARY 4

// Buffer caps
#define DSBCAPS_PRIMARYBUFFER 0x00000001
#define DSBCAPS_STATIC 0x00000002
#define DSBCAPS_CTRL3D 0x00000010
#define DSBCAPS_CTRLFREQUENCY 0x00000020
#define DSBCAPS_CTRLPAN 0x00000040
#define DSBCAPS_CTRLVOLUME 0x00000080
#define DSBCAPS_GETCURRENTPOSITION2 0x00010000

// Play / status / lock
#define DSBPLAY_LOOPING 0x00000001
#define DSBSTATUS_PLAYING 0x00000001
#define DSBSTATUS_LOOPING 0x00000004
#define DSBLOCK_FROMWRITECURSOR 0x00000001
#define DSBLOCK_ENTIREBUFFER 0x00000002

// Volume / pan / frequency ranges
#define DSBVOLUME_MIN (-10000)
#define DSBVOLUME_MAX 0
#define DSBPAN_LEFT (-10000)
#define DSBPAN_CENTER 0
#define DSBPAN_RIGHT 10000

struct IDirectSound;
struct IDirectSoundBuffer;
typedef IDirectSound* LPDIRECTSOUND;
typedef IDirectSoundBuffer* LPDIRECTSOUNDBUFFER;

struct DSCAPS {
	DWORD dwSize;
	DWORD dwFlags;
	DWORD dwMinSecondarySampleRate;
	DWORD dwMaxSecondarySampleRate;
	DWORD dwPrimaryBuffers;
	DWORD dwMaxHwMixingAllBuffers;
	DWORD dwMaxHwMixingStaticBuffers;
	DWORD dwMaxHwMixingStreamingBuffers;
	DWORD dwFreeHwMixingAllBuffers;
	DWORD dwFreeHwMixingStaticBuffers;
	DWORD dwFreeHwMixingStreamingBuffers;
	DWORD dwReserved[40];
};
typedef DSCAPS* LPDSCAPS;

struct DSBCAPS {
	DWORD dwSize;
	DWORD dwFlags;
	DWORD dwBufferBytes;
	DWORD dwUnlockTransferRate;
	DWORD dwPlayCpuOverhead;
};
typedef DSBCAPS* LPDSBCAPS;

struct DSBUFFERDESC {
	DWORD dwSize;
	DWORD dwFlags;
	DWORD dwBufferBytes;
	DWORD dwReserved;
	LPWAVEFORMATEX lpwfxFormat;
};
typedef DSBUFFERDESC* LPDSBUFFERDESC;
typedef const DSBUFFERDESC* LPCDSBUFFERDESC;

struct IDirectSoundBuffer : virtual public IUnknown {
	virtual HRESULT GetCaps(LPDSBCAPS pDSBufferCaps);
	virtual HRESULT GetCurrentPosition(LPDWORD pdwCurrentPlayCursor, LPDWORD pdwCurrentWriteCursor);
	virtual HRESULT GetFormat(LPWAVEFORMATEX pwfxFormat, DWORD dwSizeAllocated, LPDWORD pdwSizeWritten);
	virtual HRESULT GetStatus(LPDWORD pdwStatus);
	virtual HRESULT GetFrequency(LPDWORD pdwFrequency);
	virtual HRESULT Lock(
		DWORD dwOffset,
		DWORD dwBytes,
		LPVOID* ppvAudioPtr1,
		LPDWORD pdwAudioBytes1,
		LPVOID* ppvAudioPtr2,
		LPDWORD pdwAudioBytes2,
		DWORD dwFlags
	);
	virtual HRESULT Play(DWORD dwReserved1, DWORD dwPriority, DWORD dwFlags);
	virtual HRESULT SetCurrentPosition(DWORD dwNewPosition);
	virtual HRESULT SetFormat(LPCWAVEFORMATEX pcfxFormat);
	virtual HRESULT SetVolume(LONG lVolume);
	virtual HRESULT SetPan(LONG lPan);
	virtual HRESULT SetFrequency(DWORD dwFrequency);
	virtual HRESULT Stop();
	virtual HRESULT Unlock(LPVOID pvAudioPtr1, DWORD dwAudioBytes1, LPVOID pvAudioPtr2, DWORD dwAudioBytes2);
	virtual HRESULT Restore();
};

struct IDirectSound : virtual public IUnknown {
	virtual HRESULT CreateSoundBuffer(LPCDSBUFFERDESC pcDSBufferDesc, LPDIRECTSOUNDBUFFER* ppDSBuffer, IUnknown* pUnkOuter);
	virtual HRESULT GetCaps(LPDSCAPS pDSCaps);
	virtual HRESULT SetCooperativeLevel(HWND hwnd, DWORD dwLevel);
};

HRESULT DirectSoundCreate(LPCGUID pcGuidDevice, LPDIRECTSOUND* ppDS, IUnknown* pUnkOuter);
