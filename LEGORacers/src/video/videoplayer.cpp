// [library:video] The DirectShow-based player is replaced by an in-tree pipeline:
// AviReader demuxes the RIFF container and the vendored FFmpeg Indeo 5 decoder produces
// YUV410P pictures (validated byte-exact against ffmpeg). Each frame is converted to
// RGBA and presented through the active miniwin render backend (SDL_GPU / OpenGL 3.3),
// so the intro/cutscene movies use the same renderer as the rest of the game.

#include "video/videoplayer.h"

#include "app/main.h"
#include "compat.h"
#include "decomp.h"
#include "video/avireader.h"

#include <SDL3/SDL.h>
#include <indeo5dec.h>
#include <miniwin/miniwinapp.h>
#include <stdlib.h>
#include <string.h>

namespace
{

struct VideoContext {
	SDL_Window* m_window = nullptr;
	uint8_t* m_rgba = nullptr;
	int m_width = 0;
	int m_height = 0;
};

VideoContext* g_video;

// BT.601 limited-range YUV410P to RGBA with nearest-neighbour chroma.
void ConvertFrameToRgba(const Indeo5Frame& p_frame, uint8_t* p_rgba)
{
	for (int y = 0; y < p_frame.height; y++) {
		const uint8_t* lumaRow = p_frame.data[0] + (size_t) y * p_frame.linesize[0];
		const uint8_t* chromaURow = p_frame.data[1] + (size_t) (y / 4) * p_frame.linesize[1];
		const uint8_t* chromaVRow = p_frame.data[2] + (size_t) (y / 4) * p_frame.linesize[2];
		uint8_t* out = p_rgba + (size_t) y * p_frame.width * 4;

		for (int x = 0; x < p_frame.width; x++) {
			int luma = (lumaRow[x] - 16) * 298;
			int chromaU = chromaURow[x / 4] - 128;
			int chromaV = chromaVRow[x / 4] - 128;

			int red = (luma + 409 * chromaV + 128) >> 8;
			int green = (luma - 100 * chromaU - 208 * chromaV + 128) >> 8;
			int blue = (luma + 516 * chromaU + 128) >> 8;

			out[0] = (uint8_t) (red < 0 ? 0 : (red > 255 ? 255 : red));
			out[1] = (uint8_t) (green < 0 ? 0 : (green > 255 ? 255 : green));
			out[2] = (uint8_t) (blue < 0 ? 0 : (blue > 255 ? 255 : blue));
			out[3] = 0xFF;
			out += 4;
		}
	}
}

uint32_t Fourcc(const char* p_tag)
{
	return (uint32_t) ((uint8_t) p_tag[0] | ((uint8_t) p_tag[1] << 8) | ((uint8_t) p_tag[2] << 16) |
					   ((uint8_t) p_tag[3] << 24));
}

bool OpenIndeo5Avi(AviReader& p_reader, const char* p_filename)
{
	return p_reader.Open(p_filename) && p_reader.GetVideoCodec() == Fourcc("IV50");
}

// Modifier keys (Alt/Ctrl/Shift/GUI) should not skip a movie on their own — Alt is the
// first half of the Alt+Enter fullscreen chord.
bool IsModifierKey(SDL_Keycode p_key)
{
	switch (p_key) {
	case SDLK_LALT:
	case SDLK_RALT:
	case SDLK_LCTRL:
	case SDLK_RCTRL:
	case SDLK_LSHIFT:
	case SDLK_RSHIFT:
	case SDLK_LGUI:
	case SDLK_RGUI:
		return true;
	default:
		return false;
	}
}

} // namespace

extern CommandLineArgs g_commandLineArgs;

// The movies run before the game decides its display mode; mirror the intent the game
// will apply afterwards (fullscreen unless -window was given) so fullscreen launches do
// not play the intros in a window first.
static bool WantsFullscreen()
{
	for (LegoS32 i = 0; i < g_commandLineArgs.m_argc; i++) {
		if (g_commandLineArgs.m_argv[i] && strcmp(g_commandLineArgs.m_argv[i], "-window") == 0) {
			return false;
		}
	}
	return true;
}

int VideoPlayer::Begin(Win32GolApp* p_golApp, DWORD p_width, DWORD p_height)
{
	SDL_Window* window = reinterpret_cast<SDL_Window*>(p_golApp->GetHwnd());
	if (!window || g_video) {
		return 0;
	}

	bool fullscreen = WantsFullscreen();
	MiniwinApp_RunOnMainThread([window, p_width, p_height, fullscreen]() {
		if (fullscreen) {
			SDL_SetWindowFullscreen(window, true);
		}
		else {
			SDL_SetWindowSize(window, (int) p_width, (int) p_height);
			SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
		}
		SDL_ShowWindow(window);
	});

	VideoContext* video = new VideoContext;
	video->m_window = window;
	g_video = video;
	return 1;
}

int VideoPlayer::End(Win32GolApp*)
{
	if (!g_video) {
		return 1;
	}

	free(g_video->m_rgba);
	delete g_video;
	g_video = nullptr;
	return 1;
}

int VideoPlayer::Play(Win32GolApp* p_golApp, LPCSTR p_filename, int p_abortableOnKey, int p_autoRewind)
{
	(void) p_autoRewind; // the game never passes TRUE

	if (!g_video || !p_filename) {
		return 1;
	}

	AviReader reader;
	if (!OpenIndeo5Avi(reader, p_filename)) {
		SDL_Log("VideoPlayer: no Indeo 5 stream for '%s', skipping", p_filename);
		return 1;
	}

	int width = (int) reader.GetWidth();
	int height = (int) reader.GetHeight();
	Indeo5Decoder* decoder = Indeo5_Open(width, height);
	if (!decoder) {
		return 1;
	}

	if (g_video->m_width != width || g_video->m_height != height) {
		free(g_video->m_rgba);
		g_video->m_rgba = (uint8_t*) malloc((size_t) width * height * 4);
		g_video->m_width = width;
		g_video->m_height = height;
	}

	SDL_AudioStream* audio = NULL;
	if (reader.HasAudio() && reader.GetAudioBitsPerSample() == 16) {
		SDL_AudioSpec spec;
		spec.format = SDL_AUDIO_S16LE;
		spec.channels = (int) reader.GetAudioChannels();
		spec.freq = (int) reader.GetAudioSampleRate();
		audio = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
		if (audio) {
			SDL_ResumeAudioStreamDevice(audio);
		}
	}

	Uint64 startNs = 0;
	Uint64 frameIndex = 0;
	Uint64 frameDurationNs = reader.GetFrameDurationUs() * 1000ull;
	bool aborted = false;

	const uint8_t* data;
	size_t size;
	AviReader::ChunkType type;
	while (!aborted && g_video->m_rgba && (type = reader.ReadChunk(&data, &size)) != AviReader::e_endOfFile) {
		if (type == AviReader::e_audio) {
			if (audio && size) {
				SDL_PutAudioStreamData(audio, data, (int) size);
			}
			continue;
		}

		if (size == 0) {
			frameIndex++; // dropped-frame placeholder still advances time
			continue;
		}

		Indeo5Frame frame;
		if (Indeo5_Decode(decoder, data, size, &frame) != 1) {
			frameIndex++;
			continue;
		}

		ConvertFrameToRgba(frame, g_video->m_rgba);

		// Pace against the wall clock from the first presented frame.
		if (!startNs) {
			startNs = SDL_GetTicksNS();
		}
		Uint64 targetNs = startNs + frameIndex * frameDurationNs;
		Uint64 nowNs = SDL_GetTicksNS();
		if (targetNs > nowNs) {
			SDL_DelayPrecise(targetNs - nowNs);
		}
		frameIndex++;

		MiniwinBackend_PresentVideoFrame(g_video->m_window, g_video->m_rgba, width, height);

		// Drain forwarded events: abort on a key or click when allowed. A quit ends
		// playback and is re-queued for the game loop AFTER the drain — re-pushing
		// mid-drain would make this loop consume its own event forever.
		SDL_Event event;
		SDL_Event repostEvent;
		bool repost = false;
		while (MiniwinApp_PollEvent(event)) {
			if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
				repostEvent = event;
				repost = true;
				aborted = true;
			}
			else if (
				event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_RETURN && (event.key.mod & SDL_KMOD_ALT)
			) {
				// Alt+Enter toggles fullscreen, mirroring the game's message pump. The
				// game's own toggle is unavailable here (its display does not exist until
				// the movies finish), so drive the window directly; the backend reads the
				// drawable size every present, so the letterbox follows.
				SDL_Window* window = g_video->m_window;
				MiniwinApp_RunOnMainThread([window]() {
					bool fullscreen = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
					SDL_SetWindowFullscreen(window, !fullscreen);
				});
			}
			else if (
				p_abortableOnKey && ((event.type == SDL_EVENT_KEY_DOWN && !IsModifierKey(event.key.key)) ||
									 event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
			) {
				aborted = true;
			}
		}
		if (repost) {
			MiniwinApp_PushEvent(repostEvent);
		}
	}

	if (audio) {
		SDL_DestroyAudioStream(audio);
	}
	Indeo5_Close(decoder);
	return 1;
}
