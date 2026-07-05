// [library:audio] DirectSound emulation. M1: functional-silent stubs (buffers hold
// memory, cursors stand still); the miniaudio mixing engine attaches in M3.

#include "miniwin.h"

#include <miniwin/dsound.h>
#include <stdlib.h>
#include <string.h>

struct MiniwinSoundBuffer : public IDirectSoundBuffer {
	MiniwinSoundBuffer(const DSBUFFERDESC& p_desc)
	{
		m_flags = p_desc.dwFlags;
		m_size = p_desc.dwBufferBytes;
		if (p_desc.lpwfxFormat) {
			m_format = *p_desc.lpwfxFormat;
		}
		if (m_size) {
			m_data = (unsigned char*) calloc(1, m_size);
		}
	}

	~MiniwinSoundBuffer() override
	{
		free(m_data);
	}

	DWORD m_flags = 0;
	DWORD m_size = 0;
	WAVEFORMATEX m_format = {};
	unsigned char* m_data = nullptr;
	bool m_playing = false;
	LONG m_volume = 0;
	LONG m_pan = 0;
	DWORD m_frequency = 0;
};

struct MiniwinDirectSound : public IDirectSound {
	HRESULT CreateSoundBuffer(LPCDSBUFFERDESC pcDSBufferDesc, LPDIRECTSOUNDBUFFER* ppDSBuffer, IUnknown* pUnkOuter) override
	{
		if (!pcDSBufferDesc || !ppDSBuffer) {
			return DSERR_INVALIDPARAM;
		}

		*ppDSBuffer = new MiniwinSoundBuffer(*pcDSBufferDesc);
		return DS_OK;
	}

	HRESULT GetCaps(LPDSCAPS pDSCaps) override
	{
		if (pDSCaps) {
			DWORD size = pDSCaps->dwSize;
			memset(pDSCaps, 0, sizeof(DSCAPS));
			pDSCaps->dwSize = size;
			pDSCaps->dwMinSecondarySampleRate = 8000;
			pDSCaps->dwMaxSecondarySampleRate = 48000;
			pDSCaps->dwPrimaryBuffers = 1;
		}
		return DS_OK;
	}

	HRESULT SetCooperativeLevel(HWND hwnd, DWORD dwLevel) override
	{
		return DS_OK;
	}
};

HRESULT DirectSoundCreate(LPCGUID pcGuidDevice, LPDIRECTSOUND* ppDS, IUnknown* pUnkOuter)
{
	if (!ppDS) {
		return DSERR_INVALIDPARAM;
	}

	*ppDS = new MiniwinDirectSound();
	return DS_OK;
}

// --- IDirectSound base bodies ---

HRESULT IDirectSound::CreateSoundBuffer(LPCDSBUFFERDESC, LPDIRECTSOUNDBUFFER*, IUnknown*)
{
	return DSERR_GENERIC;
}
HRESULT IDirectSound::GetCaps(LPDSCAPS)
{
	return DSERR_GENERIC;
}
HRESULT IDirectSound::SetCooperativeLevel(HWND, DWORD)
{
	return DS_OK;
}

// --- IDirectSoundBuffer bodies (M1 silent implementation) ---

HRESULT IDirectSoundBuffer::GetCaps(LPDSBCAPS pDSBufferCaps)
{
	MiniwinSoundBuffer* self = static_cast<MiniwinSoundBuffer*>(this);
	if (pDSBufferCaps) {
		pDSBufferCaps->dwFlags = self->m_flags;
		pDSBufferCaps->dwBufferBytes = self->m_size;
	}
	return DS_OK;
}

HRESULT IDirectSoundBuffer::GetCurrentPosition(LPDWORD pdwCurrentPlayCursor, LPDWORD pdwCurrentWriteCursor)
{
	if (pdwCurrentPlayCursor) {
		*pdwCurrentPlayCursor = 0;
	}
	if (pdwCurrentWriteCursor) {
		*pdwCurrentWriteCursor = 0;
	}
	return DS_OK;
}

HRESULT IDirectSoundBuffer::GetFormat(LPWAVEFORMATEX pwfxFormat, DWORD dwSizeAllocated, LPDWORD pdwSizeWritten)
{
	MiniwinSoundBuffer* self = static_cast<MiniwinSoundBuffer*>(this);
	if (pwfxFormat && dwSizeAllocated >= sizeof(WAVEFORMATEX)) {
		*pwfxFormat = self->m_format;
	}
	if (pdwSizeWritten) {
		*pdwSizeWritten = sizeof(WAVEFORMATEX);
	}
	return DS_OK;
}

HRESULT IDirectSoundBuffer::GetStatus(LPDWORD pdwStatus)
{
	MiniwinSoundBuffer* self = static_cast<MiniwinSoundBuffer*>(this);
	if (pdwStatus) {
		*pdwStatus = self->m_playing ? DSBSTATUS_PLAYING : 0;
	}
	return DS_OK;
}

HRESULT IDirectSoundBuffer::GetFrequency(LPDWORD pdwFrequency)
{
	MiniwinSoundBuffer* self = static_cast<MiniwinSoundBuffer*>(this);
	if (pdwFrequency) {
		*pdwFrequency = self->m_frequency ? self->m_frequency : self->m_format.nSamplesPerSec;
	}
	return DS_OK;
}

HRESULT IDirectSoundBuffer::Lock(
	DWORD dwOffset,
	DWORD dwBytes,
	LPVOID* ppvAudioPtr1,
	LPDWORD pdwAudioBytes1,
	LPVOID* ppvAudioPtr2,
	LPDWORD pdwAudioBytes2,
	DWORD dwFlags
)
{
	MiniwinSoundBuffer* self = static_cast<MiniwinSoundBuffer*>(this);
	if (!self->m_data) {
		return DSERR_GENERIC;
	}

	if (dwFlags & DSBLOCK_ENTIREBUFFER) {
		dwOffset = 0;
		dwBytes = self->m_size;
	}

	if (dwOffset >= self->m_size) {
		dwOffset = 0;
	}

	DWORD first = dwBytes;
	DWORD second = 0;
	if (dwOffset + dwBytes > self->m_size) {
		first = self->m_size - dwOffset;
		second = dwBytes - first;
	}

	if (ppvAudioPtr1) {
		*ppvAudioPtr1 = self->m_data + dwOffset;
	}
	if (pdwAudioBytes1) {
		*pdwAudioBytes1 = first;
	}
	if (ppvAudioPtr2) {
		*ppvAudioPtr2 = second ? self->m_data : nullptr;
	}
	if (pdwAudioBytes2) {
		*pdwAudioBytes2 = second;
	}

	return DS_OK;
}

HRESULT IDirectSoundBuffer::Play(DWORD dwReserved1, DWORD dwPriority, DWORD dwFlags)
{
	static_cast<MiniwinSoundBuffer*>(this)->m_playing = true;
	return DS_OK;
}

HRESULT IDirectSoundBuffer::SetCurrentPosition(DWORD dwNewPosition)
{
	return DS_OK;
}

HRESULT IDirectSoundBuffer::SetFormat(LPCWAVEFORMATEX pcfxFormat)
{
	MiniwinSoundBuffer* self = static_cast<MiniwinSoundBuffer*>(this);
	if (pcfxFormat) {
		self->m_format = *pcfxFormat;
	}
	return DS_OK;
}

HRESULT IDirectSoundBuffer::SetVolume(LONG lVolume)
{
	static_cast<MiniwinSoundBuffer*>(this)->m_volume = lVolume;
	return DS_OK;
}

HRESULT IDirectSoundBuffer::SetPan(LONG lPan)
{
	static_cast<MiniwinSoundBuffer*>(this)->m_pan = lPan;
	return DS_OK;
}

HRESULT IDirectSoundBuffer::SetFrequency(DWORD dwFrequency)
{
	static_cast<MiniwinSoundBuffer*>(this)->m_frequency = dwFrequency;
	return DS_OK;
}

HRESULT IDirectSoundBuffer::Stop()
{
	static_cast<MiniwinSoundBuffer*>(this)->m_playing = false;
	return DS_OK;
}

HRESULT IDirectSoundBuffer::Unlock(LPVOID pvAudioPtr1, DWORD dwAudioBytes1, LPVOID pvAudioPtr2, DWORD dwAudioBytes2)
{
	return DS_OK;
}

HRESULT IDirectSoundBuffer::Restore()
{
	return DS_OK;
}
