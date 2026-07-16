// [library:audio] DirectSound emulation over a miniaudio no-device engine pulled by an
// SDL2 audio callback. Secondary buffers wrap their byte storage in a ma_audio_buffer, so
// the game's Lock/write/Unlock cycles are naturally visible to the mixer — including the
// music streamer's 2 s ring buffer, whose play cursor GetCurrentPosition reports from the
// mixer's read position.

#include "miniwin.h"

#include <math.h>
#include <miniaudio.h>
#include <miniwin/dsound.h>
#include <stdlib.h>
#include <string.h>

static const DWORD c_engineSampleRate = 22050;
static const DWORD c_engineChannels = 2;

static ma_engine g_engine;
static SDL_AudioDeviceID g_deviceId = 0;

// The original's DirectSound buffers lacked DSBCAPS_GLOBALFOCUS, so audio muted
// whenever the app lost focus; the window layer mirrors that through this hook.
void MiniwinSound_SetSuspended(bool p_suspended)
{
	if (!g_deviceId) {
		return;
	}
	SDL_PauseAudioDevice(g_deviceId, p_suspended);
}
static bool g_engineReady;

static void SDLCALL PullAudio(void* p_userdata, Uint8* p_stream, int p_len)
{
	float* buffer = (float*) p_stream;
	const int frameSize = (int) (sizeof(float) * c_engineChannels);
	int framesWanted = p_len / frameSize;

	ma_uint64 framesRead = 0;
	ma_engine_read_pcm_frames(&g_engine, buffer, framesWanted, &framesRead);

	int filledBytes = (int) (framesRead * frameSize);
	if (filledBytes < p_len) {
		memset(p_stream + filledBytes, 0, p_len - filledBytes);
	}

	static const char* statsEnv = getenv("RACERS_AUDIO_STATS");
	if (statsEnv) {
		static ma_uint64 totalFrames = 0;
		static float peak = 0.0f;
		int samples = framesWanted * (int) c_engineChannels;
		for (int i = 0; i < samples; i++) {
			float v = buffer[i] < 0.0f ? -buffer[i] : buffer[i];
			if (v > peak) {
				peak = v;
			}
		}
		totalFrames += framesWanted;
		static ma_uint64 lastReport = 0;
		if (totalFrames - lastReport >= c_engineSampleRate * 2) {
			lastReport = totalFrames;
			SDL_LogInfo(
				LOG_CATEGORY_MINIWIN,
				"audio: %llu frames pulled, peak %.3f",
				(unsigned long long) totalFrames,
				peak
			);
			peak = 0.0f;
		}
	}
}

static bool EnsureEngine()
{
	if (g_engineReady) {
		return true;
	}

	ma_engine_config config = ma_engine_config_init();
	config.noDevice = MA_TRUE;
	config.channels = c_engineChannels;
	config.sampleRate = c_engineSampleRate;
	if (ma_engine_init(&config, &g_engine) != MA_SUCCESS) {
		SDL_LogError(LOG_CATEGORY_MINIWIN, "ma_engine_init failed");
		return false;
	}

	SDL_AudioSpec spec;
	SDL_zero(spec);
	spec.format = AUDIO_F32;
	spec.channels = (Uint8) c_engineChannels;
	spec.freq = (int) c_engineSampleRate;
	spec.samples = 4096;
	spec.callback = PullAudio;
	g_deviceId = SDL_OpenAudioDevice(NULL, 0, &spec, NULL, 0);
	if (!g_deviceId) {
		SDL_LogError(LOG_CATEGORY_MINIWIN, "SDL_OpenAudioDevice failed: %s", SDL_GetError());
		ma_engine_uninit(&g_engine);
		return false;
	}

	SDL_PauseAudioDevice(g_deviceId, 0);
	g_engineReady = true;
	return true;
}

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

		if (!m_data || (m_flags & DSBCAPS_PRIMARYBUFFER) || !EnsureEngine()) {
			return;
		}

		ma_format format;
		switch (m_format.wBitsPerSample) {
		case 8:
			format = ma_format_u8;
			break;
		case 16:
			format = ma_format_s16;
			break;
		default:
			return;
		}
		if (!m_format.nChannels || !m_format.nBlockAlign) {
			return;
		}

		ma_audio_buffer_config bufferConfig =
			ma_audio_buffer_config_init(format, m_format.nChannels, m_size / m_format.nBlockAlign, m_data, nullptr);
		if (ma_audio_buffer_init(&bufferConfig, &m_buffer) != MA_SUCCESS) {
			return;
		}

		if (ma_sound_init_from_data_source(&g_engine, &m_buffer, MA_SOUND_FLAG_NO_SPATIALIZATION, nullptr, &m_sound) !=
			MA_SUCCESS) {
			ma_audio_buffer_uninit(&m_buffer);
			return;
		}

		m_soundReady = true;
		ApplyFrequency(m_format.nSamplesPerSec);
	}

	~MiniwinSoundBuffer() override
	{
		if (m_soundReady) {
			ma_sound_uninit(&m_sound);
			ma_audio_buffer_uninit(&m_buffer);
		}
		free(m_data);
	}

	void ApplyFrequency(DWORD p_frequency)
	{
		if (!p_frequency) {
			p_frequency = m_format.nSamplesPerSec;
		}
		if (m_soundReady && p_frequency) {
			ma_sound_set_pitch(&m_sound, (float) p_frequency / (float) c_engineSampleRate);
		}
	}

	DWORD m_flags = 0;
	DWORD m_size = 0;
	WAVEFORMATEX m_format = {};
	unsigned char* m_data = nullptr;
	bool m_playing = false;
	bool m_looping = false;
	LONG m_volume = 0;
	LONG m_pan = 0;
	DWORD m_frequency = 0;

	ma_audio_buffer m_buffer = {};
	ma_sound m_sound = {};
	bool m_soundReady = false;
};

struct MiniwinDirectSound : public IDirectSound {
	HRESULT CreateSoundBuffer(
		LPCDSBUFFERDESC pcDSBufferDesc,
		LPDIRECTSOUNDBUFFER* ppDSBuffer,
		IUnknown* pUnkOuter
	) override
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

	HRESULT SetCooperativeLevel(HWND hwnd, DWORD dwLevel) override { return DS_OK; }
};

HRESULT DirectSoundCreate(LPCGUID pcGuidDevice, LPDIRECTSOUND* ppDS, IUnknown* pUnkOuter)
{
	if (!ppDS) {
		return DSERR_INVALIDPARAM;
	}
	if (!EnsureEngine()) {
		return DSERR_GENERIC;
	}

	*ppDS = new MiniwinDirectSound();
	return DS_OK;
}

// --- IDirectSound base bodies ---

HRESULT IDirectSound::CreateSoundBuffer(LPCDSBUFFERDESC, LPDIRECTSOUNDBUFFER*, IUnknown*)
{
	MiniwinPhaseScope phase(MINIWIN_PHASE_AUDIO);
	MiniwinSlowOpLog slowLog("dsound", "CreateSoundBuffer");

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

// --- IDirectSoundBuffer ---

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
	MiniwinSoundBuffer* self = static_cast<MiniwinSoundBuffer*>(this);

	DWORD play = 0;
	if (self->m_soundReady && self->m_size) {
		ma_uint64 frames = 0;
		ma_sound_get_cursor_in_pcm_frames(&self->m_sound, &frames);

		ma_uint64 totalFrames = self->m_size / self->m_format.nBlockAlign;
		if (totalFrames) {
			frames %= totalFrames;
		}
		play = (DWORD) (frames * self->m_format.nBlockAlign);
	}

	if (pdwCurrentPlayCursor) {
		*pdwCurrentPlayCursor = play;
	}
	if (pdwCurrentWriteCursor) {
		// DirectSound's write cursor leads the play cursor by a mixing latency;
		// ~15 ms keeps the music streamer's ring arithmetic happy.
		DWORD lead = 0;
		if (self->m_playing && self->m_format.nAvgBytesPerSec) {
			lead = self->m_format.nAvgBytesPerSec * 15 / 1000;
			if (self->m_format.nBlockAlign) {
				lead -= lead % self->m_format.nBlockAlign;
			}
		}
		*pdwCurrentWriteCursor = self->m_size ? (play + lead) % self->m_size : 0;
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
		*pdwStatus = 0;
		if (self->m_soundReady) {
			// A finished one-shot stops on its own in the mixer.
			self->m_playing = ma_sound_is_playing(&self->m_sound) != MA_FALSE;
		}
		if (self->m_playing) {
			*pdwStatus = DSBSTATUS_PLAYING;
			if (self->m_looping) {
				*pdwStatus |= DSBSTATUS_LOOPING;
			}
		}
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
	MiniwinPhaseScope phase(MINIWIN_PHASE_AUDIO);
	MiniwinSlowOpLog slowLog("dsound", "Play");

	MiniwinSoundBuffer* self = static_cast<MiniwinSoundBuffer*>(this);
	self->m_looping = (dwFlags & DSBPLAY_LOOPING) != 0;

	if (self->m_soundReady) {
		ma_sound_set_looping(&self->m_sound, self->m_looping ? MA_TRUE : MA_FALSE);

		// Replaying a one-shot that ran to completion restarts it from the top.
		if (!self->m_playing && ma_sound_at_end(&self->m_sound)) {
			ma_sound_seek_to_pcm_frame(&self->m_sound, 0);
		}
		ma_sound_start(&self->m_sound);
	}

	self->m_playing = true;
	return DS_OK;
}

HRESULT IDirectSoundBuffer::SetCurrentPosition(DWORD dwNewPosition)
{
	MiniwinSoundBuffer* self = static_cast<MiniwinSoundBuffer*>(this);
	if (self->m_soundReady && self->m_format.nBlockAlign) {
		ma_sound_seek_to_pcm_frame(&self->m_sound, dwNewPosition / self->m_format.nBlockAlign);
	}
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
	MiniwinSoundBuffer* self = static_cast<MiniwinSoundBuffer*>(this);
	if (lVolume < DSBVOLUME_MIN) {
		lVolume = DSBVOLUME_MIN;
	}
	if (lVolume > DSBVOLUME_MAX) {
		lVolume = DSBVOLUME_MAX;
	}
	self->m_volume = lVolume;

	if (self->m_soundReady) {
		// DirectSound volume is in hundredths of a decibel of attenuation.
		float linear = lVolume <= DSBVOLUME_MIN ? 0.0f : ma_volume_db_to_linear((float) lVolume / 100.0f);
		ma_sound_set_volume(&self->m_sound, linear);
	}
	return DS_OK;
}

HRESULT IDirectSoundBuffer::SetPan(LONG lPan)
{
	MiniwinSoundBuffer* self = static_cast<MiniwinSoundBuffer*>(this);
	if (lPan < DSBPAN_LEFT) {
		lPan = DSBPAN_LEFT;
	}
	if (lPan > DSBPAN_RIGHT) {
		lPan = DSBPAN_RIGHT;
	}
	self->m_pan = lPan;

	if (self->m_soundReady) {
		ma_sound_set_pan(&self->m_sound, (float) lPan / 10000.0f);
	}
	return DS_OK;
}

HRESULT IDirectSoundBuffer::SetFrequency(DWORD dwFrequency)
{
	MiniwinSoundBuffer* self = static_cast<MiniwinSoundBuffer*>(this);
	self->m_frequency = dwFrequency;
	self->ApplyFrequency(dwFrequency);
	return DS_OK;
}

HRESULT IDirectSoundBuffer::Stop()
{
	MiniwinPhaseScope phase(MINIWIN_PHASE_AUDIO);
	MiniwinSlowOpLog slowLog("dsound", "Stop");

	MiniwinSoundBuffer* self = static_cast<MiniwinSoundBuffer*>(this);
	if (self->m_soundReady) {
		ma_sound_stop(&self->m_sound);
	}
	self->m_playing = false;
	return DS_OK;
}

HRESULT IDirectSoundBuffer::Unlock(LPVOID pvAudioPtr1, DWORD dwAudioBytes1, LPVOID pvAudioPtr2, DWORD dwAudioBytes2)
{
	// The mixer reads the ring memory directly; written regions are audible as-is.
	return DS_OK;
}

HRESULT IDirectSoundBuffer::Restore()
{
	return DS_OK;
}
