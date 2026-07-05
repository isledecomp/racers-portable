// [library:video] The DirectShow-based player is replaced by an in-tree pipeline:
// AviReader demuxes the RIFF container and the vendored FFmpeg Indeo 5 decoder
// produces YUV410P pictures (validated byte-exact against ffmpeg). Presentation uses
// a temporary OpenGL 3.3 context of its own on the game window — the movies run
// before the game initializes its display, and it must be a context with the same
// attributes the render backend later requests: macOS pins the window's pixel format
// to its first GL use (an SDL_Renderer here left the window stuck on GL 2.1, which
// broke every game shader afterwards).

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

typedef unsigned int VpGLenum;
typedef unsigned int VpGLuint;
typedef int VpGLint;
typedef int VpGLsizei;
typedef float VpGLfloat;
typedef char VpGLchar;
typedef unsigned char VpGLboolean;
typedef ptrdiff_t VpGLsizeiptr;

#define VP_GL_FUNCS(X)                                                                                                 \
	X(void, glViewport, (VpGLint, VpGLint, VpGLsizei, VpGLsizei))                                                      \
	X(void, glClearColor, (VpGLfloat, VpGLfloat, VpGLfloat, VpGLfloat))                                                \
	X(void, glClear, (unsigned int) )                                                                                  \
	X(void, glGenTextures, (VpGLsizei, VpGLuint*) )                                                                    \
	X(void, glDeleteTextures, (VpGLsizei, const VpGLuint*) )                                                           \
	X(void, glBindTexture, (VpGLenum, VpGLuint))                                                                       \
	X(void,                                                                                                            \
	  glTexImage2D,                                                                                                    \
	  (VpGLenum, VpGLint, VpGLint, VpGLsizei, VpGLsizei, VpGLint, VpGLenum, VpGLenum, const void*) )                   \
	X(void, glTexParameteri, (VpGLenum, VpGLenum, VpGLint))                                                            \
	X(VpGLuint, glCreateShader, (VpGLenum))                                                                            \
	X(void, glShaderSource, (VpGLuint, VpGLsizei, const VpGLchar* const*, const VpGLint*) )                            \
	X(void, glCompileShader, (VpGLuint))                                                                               \
	X(VpGLuint, glCreateProgram, (void) )                                                                              \
	X(void, glAttachShader, (VpGLuint, VpGLuint))                                                                      \
	X(void, glLinkProgram, (VpGLuint))                                                                                 \
	X(void, glUseProgram, (VpGLuint))                                                                                  \
	X(void, glDeleteShader, (VpGLuint))                                                                                \
	X(void, glDeleteProgram, (VpGLuint))                                                                               \
	X(void, glGenVertexArrays, (VpGLsizei, VpGLuint*) )                                                                \
	X(void, glBindVertexArray, (VpGLuint))                                                                             \
	X(void, glDeleteVertexArrays, (VpGLsizei, const VpGLuint*) )                                                       \
	X(void, glGenBuffers, (VpGLsizei, VpGLuint*) )                                                                     \
	X(void, glBindBuffer, (VpGLenum, VpGLuint))                                                                        \
	X(void, glDeleteBuffers, (VpGLsizei, const VpGLuint*) )                                                            \
	X(void, glBufferData, (VpGLenum, VpGLsizeiptr, const void*, VpGLenum))                                             \
	X(void, glVertexAttribPointer, (VpGLuint, VpGLint, VpGLenum, VpGLboolean, VpGLsizei, const void*) )                \
	X(void, glEnableVertexAttribArray, (VpGLuint))                                                                     \
	X(void, glDrawArrays, (VpGLenum, VpGLint, VpGLsizei))                                                              \
	X(VpGLint, glGetUniformLocation, (VpGLuint, const VpGLchar*) )                                                     \
	X(void, glUniform1i, (VpGLint, VpGLint))

struct VideoGl {
#define VP_DECL(ret, name, args) ret(SDLCALL* name) args = nullptr;
	VP_GL_FUNCS(VP_DECL)
#undef VP_DECL

	bool Load()
	{
		bool ok = true;
#define VP_LOAD(ret, name, args)                                                                                       \
	name = (ret(SDLCALL*) args) SDL_GL_GetProcAddress(#name);                                                          \
	ok = ok && name != nullptr;
		VP_GL_FUNCS(VP_LOAD)
#undef VP_LOAD
		return ok;
	}
};

constexpr unsigned int c_glColorBufferBit = 0x00004000;
constexpr VpGLenum c_glTexture2d = 0x0DE1;
constexpr VpGLenum c_glRgba8 = 0x8058;
constexpr VpGLenum c_glRgba = 0x1908;
constexpr VpGLenum c_glUnsignedByte = 0x1401;
constexpr VpGLint c_glLinear = 0x2601;
constexpr VpGLenum c_glTextureMinFilter = 0x2801;
constexpr VpGLenum c_glTextureMagFilter = 0x2800;
constexpr VpGLenum c_glVertexShader = 0x8B31;
constexpr VpGLenum c_glFragmentShader = 0x8B30;
constexpr VpGLenum c_glArrayBuffer = 0x8892;
constexpr VpGLenum c_glStreamDraw = 0x88E0;
constexpr VpGLenum c_glTriangleStrip = 0x0005;
constexpr VpGLenum c_glFloat = 0x1406;

const char* g_videoVertexShader = "#version 330 core\n"
								  "layout(location = 0) in vec2 aPos;\n"
								  "layout(location = 1) in vec2 aUV;\n"
								  "out vec2 vUV;\n"
								  "void main() { gl_Position = vec4(aPos, 0.0, 1.0); vUV = aUV; }\n";

const char* g_videoFragmentShader = "#version 330 core\n"
									"in vec2 vUV;\n"
									"uniform sampler2D uTexture;\n"
									"out vec4 oColor;\n"
									"void main() { oColor = texture(uTexture, vUV); }\n";

struct VideoContext {
	SDL_Window* m_window = nullptr;
	SDL_GLContext m_glContext = nullptr;
	VideoGl gl;
	VpGLuint m_program = 0;
	VpGLuint m_vao = 0;
	VpGLuint m_vbo = 0;
	VpGLuint m_texture = 0;
	int m_textureWidth = 0;
	int m_textureHeight = 0;
	uint8_t* m_rgba = nullptr;
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

} // namespace

extern CommandLineArgs g_commandLineArgs;

// The movies run before the game decides its display mode; mirror the intent the
// game will apply afterwards (fullscreen unless -window was given) so fullscreen
// launches do not play the intros in a window first.
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

	// Request the same GL attributes the render backend uses later: the first
	// context created on the window decides its pixel format on macOS.
	MiniwinBackend_PrepareWindowFlags();

	VideoContext* video = new VideoContext;
	video->m_window = window;
	video->m_glContext = SDL_GL_CreateContext(window);
	if (!video->m_glContext || !video->gl.Load()) {
		SDL_Log("VideoPlayer: no GL context for playback (%s), movies skipped", SDL_GetError());
		if (video->m_glContext) {
			SDL_GL_DestroyContext(video->m_glContext);
		}
		delete video;
		return 0;
	}
	SDL_GL_SetSwapInterval(0);

	VideoGl& gl = video->gl;
	VpGLuint vertexShader = gl.glCreateShader(c_glVertexShader);
	gl.glShaderSource(vertexShader, 1, &g_videoVertexShader, nullptr);
	gl.glCompileShader(vertexShader);
	VpGLuint fragmentShader = gl.glCreateShader(c_glFragmentShader);
	gl.glShaderSource(fragmentShader, 1, &g_videoFragmentShader, nullptr);
	gl.glCompileShader(fragmentShader);
	video->m_program = gl.glCreateProgram();
	gl.glAttachShader(video->m_program, vertexShader);
	gl.glAttachShader(video->m_program, fragmentShader);
	gl.glLinkProgram(video->m_program);
	gl.glDeleteShader(vertexShader);
	gl.glDeleteShader(fragmentShader);
	gl.glUseProgram(video->m_program);
	gl.glUniform1i(gl.glGetUniformLocation(video->m_program, "uTexture"), 0);

	gl.glGenVertexArrays(1, &video->m_vao);
	gl.glBindVertexArray(video->m_vao);
	gl.glGenBuffers(1, &video->m_vbo);
	gl.glBindBuffer(c_glArrayBuffer, video->m_vbo);
	gl.glVertexAttribPointer(0, 2, c_glFloat, 0, 4 * sizeof(float), (const void*) 0);
	gl.glEnableVertexAttribArray(0);
	gl.glVertexAttribPointer(1, 2, c_glFloat, 0, 4 * sizeof(float), (const void*) (2 * sizeof(float)));
	gl.glEnableVertexAttribArray(1);

	g_video = video;
	return 1;
}

int VideoPlayer::End(Win32GolApp*)
{
	if (!g_video) {
		return 1;
	}

	VideoGl& gl = g_video->gl;
	if (g_video->m_texture) {
		gl.glDeleteTextures(1, &g_video->m_texture);
	}
	gl.glDeleteBuffers(1, &g_video->m_vbo);
	gl.glDeleteVertexArrays(1, &g_video->m_vao);
	gl.glDeleteProgram(g_video->m_program);
	SDL_GL_DestroyContext(g_video->m_glContext);

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

	VideoGl& gl = g_video->gl;
	if (g_video->m_textureWidth != width || g_video->m_textureHeight != height) {
		if (g_video->m_texture) {
			gl.glDeleteTextures(1, &g_video->m_texture);
		}
		gl.glGenTextures(1, &g_video->m_texture);
		gl.glBindTexture(c_glTexture2d, g_video->m_texture);
		gl.glTexParameteri(c_glTexture2d, c_glTextureMinFilter, c_glLinear);
		gl.glTexParameteri(c_glTexture2d, c_glTextureMagFilter, c_glLinear);
		g_video->m_textureWidth = width;
		g_video->m_textureHeight = height;
		free(g_video->m_rgba);
		g_video->m_rgba = (uint8_t*) malloc((size_t) width * height * 4);
	}
	gl.glBindTexture(c_glTexture2d, g_video->m_texture);

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

		// Letterbox the frame into the drawable.
		int drawableWidth = 0;
		int drawableHeight = 0;
		SDL_GetWindowSizeInPixels(g_video->m_window, &drawableWidth, &drawableHeight);
		if (drawableWidth <= 0 || drawableHeight <= 0) {
			continue;
		}
		float scaleX = (float) drawableWidth / (float) width;
		float scaleY = (float) drawableHeight / (float) height;
		float scale = scaleX < scaleY ? scaleX : scaleY;
		float extentX = ((float) width * scale) / (float) drawableWidth;
		float extentY = ((float) height * scale) / (float) drawableHeight;

		const float quad[16] = {
			-extentX,
			-extentY,
			0.0f,
			1.0f, //
			extentX,
			-extentY,
			1.0f,
			1.0f, //
			-extentX,
			extentY,
			0.0f,
			0.0f, //
			extentX,
			extentY,
			1.0f,
			0.0f, //
		};

		gl.glTexImage2D(
			c_glTexture2d,
			0,
			(VpGLint) c_glRgba8,
			width,
			height,
			0,
			c_glRgba,
			c_glUnsignedByte,
			g_video->m_rgba
		);
		gl.glViewport(0, 0, drawableWidth, drawableHeight);
		gl.glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
		gl.glClear(c_glColorBufferBit);
		gl.glBufferData(c_glArrayBuffer, sizeof(quad), quad, c_glStreamDraw);
		gl.glDrawArrays(c_glTriangleStrip, 0, 4);
		SDL_GL_SwapWindow(g_video->m_window);

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
				p_abortableOnKey && (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_DOWN)
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
