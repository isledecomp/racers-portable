// [library:3d] Render backend selection and the process-shared backend instance.
// Dispatches the MiniwinBackend_* seam to the compiled backend chosen by
// MiniwinSetBackend (--renderer / the in-game menu), with fallback when the requested
// backend is unavailable. Also presents video frames through that same backend so
// intro/cutscene playback uses the selected renderer.

#include "backends.h"

#include "miniwin.h"
#include "renderbackend.h"

#include <errno.h>
#include <miniwin/miniwinapp.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

// --- Selection config ---

#if defined(USE_SDL_GPU)
static MiniwinBackendId g_activeBackend = MINIWIN_BACKEND_SDLGPU;
#elif defined(USE_OPENGL3)
static MiniwinBackendId g_activeBackend = MINIWIN_BACKEND_OPENGL3;
#else
static MiniwinBackendId g_activeBackend = MINIWIN_BACKEND_OPENGLES3;
#endif
static bool g_resolved;
static MiniwinBackendId g_resolvedBackend;

// Set per backend when its window/context could not be created this session, so the
// fallback chain treats it as unavailable from then on.
static bool g_backendForcedUnusable[3];

void MiniwinSetBackend(MiniwinBackendId p_backend)
{
	g_activeBackend = p_backend;
	g_resolved = false;
}

MiniwinBackendId MiniwinGetBackend()
{
	return g_activeBackend;
}

bool MiniwinBackendFromName(const char* p_name, MiniwinBackendId* p_id)
{
	if (!p_name || !p_id) {
		return false;
	}
	if (SDL_strcasecmp(p_name, "sdlgpu") == 0 || SDL_strcasecmp(p_name, "gpu") == 0) {
		*p_id = MINIWIN_BACKEND_SDLGPU;
		return true;
	}
	if (SDL_strcasecmp(p_name, "opengl3") == 0 || SDL_strcasecmp(p_name, "opengl") == 0 ||
		SDL_strcasecmp(p_name, "gl") == 0) {
		*p_id = MINIWIN_BACKEND_OPENGL3;
		return true;
	}
	if (SDL_strcasecmp(p_name, "opengles3") == 0 || SDL_strcasecmp(p_name, "gles3") == 0 ||
		SDL_strcasecmp(p_name, "gles") == 0 || SDL_strcasecmp(p_name, "webgl2") == 0) {
		*p_id = MINIWIN_BACKEND_OPENGLES3;
		return true;
	}
	return false;
}

const char* MiniwinBackendName(MiniwinBackendId p_backend)
{
	switch (p_backend) {
	case MINIWIN_BACKEND_SDLGPU:
		return "sdlgpu";
	case MINIWIN_BACKEND_OPENGL3:
		return "opengl3";
	case MINIWIN_BACKEND_OPENGLES3:
		return "opengles3";
	}
	return "unknown";
}

// --- Resolution: which compiled, available backend to actually use ---

static bool BackendCompiled(MiniwinBackendId p_backend)
{
	switch (p_backend) {
	case MINIWIN_BACKEND_SDLGPU:
#ifdef USE_SDL_GPU
		return true;
#else
		return false;
#endif
	case MINIWIN_BACKEND_OPENGL3:
#ifdef USE_OPENGL3
		return true;
#else
		return false;
#endif
	case MINIWIN_BACKEND_OPENGLES3:
#ifdef USE_OPENGLES3
		return true;
#else
		return false;
#endif
	}
	return false;
}

static bool BackendProbe(MiniwinBackendId p_backend)
{
	switch (p_backend) {
	case MINIWIN_BACKEND_SDLGPU:
#ifdef USE_SDL_GPU
		return MiniwinSdlGpu_Available();
#else
		return false;
#endif
	case MINIWIN_BACKEND_OPENGL3:
#ifdef USE_OPENGL3
		return MiniwinGl3_Available();
#else
		return false;
#endif
	case MINIWIN_BACKEND_OPENGLES3:
#ifdef USE_OPENGLES3
		return MiniwinGles3_Available();
#else
		return false;
#endif
	}
	return false;
}

// Cached: probing SDL_GPU creates and destroys a device, so do it at most once per
// backend (enumeration and resolution both consult this).
static bool BackendUsable(MiniwinBackendId p_backend)
{
	static int s_cache[3] = {-1, -1, -1};
	int index = (int) p_backend;
	if (index < 0 || index >= 3) {
		return false;
	}
	if (g_backendForcedUnusable[index]) {
		return false;
	}
	if (s_cache[index] < 0) {
		s_cache[index] = BackendProbe(p_backend) ? 1 : 0;
	}
	return s_cache[index] != 0;
}

bool MiniwinBackendUsable(MiniwinBackendId p_backend)
{
	return BackendUsable(p_backend);
}

// Preference order for fallback when the requested backend is unavailable.
static const MiniwinBackendId c_fallbackOrder[] = {
	MINIWIN_BACKEND_SDLGPU,
	MINIWIN_BACKEND_OPENGL3,
	MINIWIN_BACKEND_OPENGLES3,
};

static MiniwinBackendId ResolveBackend()
{
	if (g_resolved) {
		return g_resolvedBackend;
	}

	MiniwinBackendId resolved = g_activeBackend;
	if (!BackendUsable(resolved)) {
		for (MiniwinBackendId candidate : c_fallbackOrder) {
			if (BackendUsable(candidate)) {
				SDL_LogWarn(
					LOG_CATEGORY_MINIWIN,
					"render backend '%s' unavailable, falling back to '%s'",
					MiniwinBackendName(g_activeBackend),
					MiniwinBackendName(candidate)
				);
				resolved = candidate;
				break;
			}
		}
	}

	g_resolvedBackend = resolved;
	g_resolved = true;
	SDL_LogInfo(LOG_CATEGORY_MINIWIN, "render backend: %s", MiniwinBackendName(resolved));
	return resolved;
}

bool MiniwinBackend_DemoteActiveBackend()
{
	MiniwinBackendId failed = ResolveBackend();
	g_backendForcedUnusable[(int) failed] = true;
	g_resolved = false;

	MiniwinBackendId next = ResolveBackend();
	if (next == failed) {
		SDL_LogError(
			LOG_CATEGORY_MINIWIN,
			"render backend '%s' could not create a window and no fallback is available",
			MiniwinBackendName(failed)
		);
		return false;
	}

	SDL_LogWarn(
		LOG_CATEGORY_MINIWIN,
		"render backend '%s' could not create a window; falling back to '%s'",
		MiniwinBackendName(failed),
		MiniwinBackendName(next)
	);
	return true;
}

// --- Dispatch ---

Uint32 MiniwinBackend_PrepareWindowFlags()
{
	switch (ResolveBackend()) {
#ifdef USE_SDL_GPU
	case MINIWIN_BACKEND_SDLGPU:
		return MiniwinSdlGpu_PrepareWindowFlags();
#endif
#ifdef USE_OPENGL3
	case MINIWIN_BACKEND_OPENGL3:
		return MiniwinGl3_PrepareWindowFlags();
#endif
#ifdef USE_OPENGLES3
	case MINIWIN_BACKEND_OPENGLES3:
		return MiniwinGles3_PrepareWindowFlags();
#endif
	default:
		return 0;
	}
}

static MiniwinRenderBackend* g_backend;
// The window the backend is bound to (for life). Kept so the renderer-switch relaunch can
// read the live fullscreen/windowed state.
static SDL_Window* g_backendWindow;

MiniwinRenderBackend* MiniwinBackend_Acquire(SDL_Window* p_window, int p_width, int p_height)
{
	if (g_backend || !p_window) {
		return g_backend;
	}

	g_backendWindow = p_window;

	switch (ResolveBackend()) {
#ifdef USE_SDL_GPU
	case MINIWIN_BACKEND_SDLGPU:
		g_backend = MiniwinSdlGpu_Create(p_window, p_width, p_height);
		break;
#endif
#ifdef USE_OPENGL3
	case MINIWIN_BACKEND_OPENGL3:
		g_backend = MiniwinGl3_Create(p_window, p_width, p_height);
		break;
#endif
#ifdef USE_OPENGLES3
	case MINIWIN_BACKEND_OPENGLES3:
		g_backend = MiniwinGles3_Create(p_window, p_width, p_height);
		break;
#endif
	default:
		break;
	}

	if (!g_backend) {
		SDL_LogError(LOG_CATEGORY_MINIWIN, "render backend failed to initialize");
	}
	return g_backend;
}

void MiniwinBackend_Shutdown()
{
	delete g_backend;
	g_backend = nullptr;
}

// --- Backend registry: one selectable "display driver" per compiled backend ---

// Stable per-backend driver GUIDs. The in-game Options -> Video carousel lists these as
// display drivers; selecting one maps back to a backend and (via the CreateDevice
// interception) re-launches into it. Derived from the device GUID with a per-backend tag.
MINIWIN_DEFINE_GUID(
	MINIWIN_GUID_BackendSdlGpu,
	0x682656f3,
	0x121a,
	0x4d97,
	0x9a,
	0xb2,
	0xc5,
	0x88,
	0x14,
	0x91,
	0x01,
	0x00
);
MINIWIN_DEFINE_GUID(
	MINIWIN_GUID_BackendOpenGl3,
	0x682656f3,
	0x121a,
	0x4d97,
	0x9a,
	0xb2,
	0xc5,
	0x88,
	0x14,
	0x91,
	0x01,
	0x01
);
MINIWIN_DEFINE_GUID(
	MINIWIN_GUID_BackendOpenGles3,
	0x682656f3,
	0x121a,
	0x4d97,
	0x9a,
	0xb2,
	0xc5,
	0x88,
	0x14,
	0x91,
	0x01,
	0x02
);

const GUID* MiniwinBackendGuid(MiniwinBackendId p_backend)
{
	switch (p_backend) {
	case MINIWIN_BACKEND_OPENGL3:
		return &MINIWIN_GUID_BackendOpenGl3;
	case MINIWIN_BACKEND_OPENGLES3:
		return &MINIWIN_GUID_BackendOpenGles3;
	case MINIWIN_BACKEND_SDLGPU:
	default:
		return &MINIWIN_GUID_BackendSdlGpu;
	}
}

const char* MiniwinBackendDisplayName(MiniwinBackendId p_backend)
{
	switch (p_backend) {
	case MINIWIN_BACKEND_SDLGPU:
		return "SDL3 GPU";
	case MINIWIN_BACKEND_OPENGL3:
		return "OpenGL 3.3";
	case MINIWIN_BACKEND_OPENGLES3:
		return "OpenGL ES 3";
	}
	return "Renderer";
}

bool MiniwinBackendFromGuid(const GUID* p_guid, MiniwinBackendId* p_id)
{
	if (!p_guid || !p_id) {
		return false;
	}
	if (*p_guid == MINIWIN_GUID_BackendSdlGpu) {
		*p_id = MINIWIN_BACKEND_SDLGPU;
		return true;
	}
	if (*p_guid == MINIWIN_GUID_BackendOpenGl3) {
		*p_id = MINIWIN_BACKEND_OPENGL3;
		return true;
	}
	if (*p_guid == MINIWIN_GUID_BackendOpenGles3) {
		*p_id = MINIWIN_BACKEND_OPENGLES3;
		return true;
	}
	return false;
}

MiniwinBackendId MiniwinBackendActive()
{
	return ResolveBackend();
}

int MiniwinBackend_EnumDrivers(MiniwinBackendId* p_out, int p_max)
{
	// Active backend first so the game's default device matching selects the running
	// one; only usable backends are listed so a menu pick can never re-launch into a
	// backend that would just fall back (which would loop).
	MiniwinBackendId ordered[] = {
		ResolveBackend(),
		MINIWIN_BACKEND_SDLGPU,
		MINIWIN_BACKEND_OPENGL3,
		MINIWIN_BACKEND_OPENGLES3,
	};
	int count = 0;
	for (MiniwinBackendId candidate : ordered) {
		if (count >= p_max || !BackendCompiled(candidate) || !BackendUsable(candidate)) {
			continue;
		}
		bool duplicate = false;
		for (int i = 0; i < count; i++) {
			if (p_out[i] == candidate) {
				duplicate = true;
				break;
			}
		}
		if (!duplicate) {
			p_out[count++] = candidate;
		}
	}
	return count;
}

// --- Persistence: remember the in-game renderer choice across launches ---

static bool BackendPrefPath(char* p_path, size_t p_size)
{
	char base[1024];
	MiniwinGetUserDataPath(base, sizeof(base));
	if (!base[0]) {
		return false;
	}
	SDL_snprintf(p_path, p_size, "%srenderer", base);
	return true;
}

void MiniwinBackendSavePref(MiniwinBackendId p_backend)
{
	char path[1024];
	if (!BackendPrefPath(path, sizeof(path))) {
		return;
	}
	const char* name = MiniwinBackendName(p_backend);
	SDL_SaveFile(path, name, SDL_strlen(name));
}

bool MiniwinBackendLoadPref(MiniwinBackendId* p_id)
{
	char path[1024];
	if (!p_id || !BackendPrefPath(path, sizeof(path))) {
		return false;
	}
	size_t size = 0;
	void* data = SDL_LoadFile(path, &size);
	if (!data) {
		return false;
	}
	char name[32];
	SDL_strlcpy(name, (const char*) data, SDL_min(size + 1, sizeof(name)));
	SDL_free(data);
	return MiniwinBackendFromName(name, p_id);
}

// --- Re-exec: switching backend needs a fresh process (the window's graphics binding
// is fixed at creation). The saved command line is replayed with --renderer swapped. ---

static int g_argc;
static char** g_argv;

void MiniwinApp_SetCommandLine(int p_argc, char** p_argv)
{
	g_argc = p_argc;
	g_argv = (char**) SDL_malloc((size_t) (p_argc + 1) * sizeof(char*));
	for (int i = 0; i < p_argc; i++) {
		g_argv[i] = SDL_strdup(p_argv[i]);
	}
	g_argv[p_argc] = nullptr;
}

static bool ExecutablePath(char* p_path, size_t p_size)
{
#ifdef __APPLE__
	uint32_t size = (uint32_t) p_size;
	return _NSGetExecutablePath(p_path, &size) == 0;
#elif defined(__linux__)
	ssize_t length = readlink("/proc/self/exe", p_path, p_size - 1);
	if (length <= 0) {
		return false;
	}
	p_path[length] = '\0';
	return true;
#else
	// Fall back to argv[0] (miniwin's windows.h shim doesn't expose GetModuleFileName).
	if (!g_argv || !g_argv[0]) {
		return false;
	}
	SDL_strlcpy(p_path, g_argv[0], p_size);
	return true;
#endif
}

void MiniwinBackend_Relaunch(MiniwinBackendId p_backend)
{
	char exe[4096];
	if (g_argc <= 0 || !ExecutablePath(exe, sizeof(exe))) {
		SDL_LogError(LOG_CATEGORY_MINIWIN, "cannot relaunch: no saved command line / executable path");
		return;
	}

	// Carry the CURRENT window state across the relaunch: an in-game Alt+Enter or
	// fullscreen toggle since launch must be preserved, not reverted to the original
	// -window argument. Query the live window (main thread) and re-derive -window below.
	bool windowed = false;
	if (g_backendWindow) {
		SDL_Window* window = g_backendWindow;
		MiniwinApp_RunOnMainThread([window, &windowed]() {
			windowed = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) == 0;
		});
	}

	// Rebuild argv: the original minus any --renderer <value> and -window. The new backend
	// is NOT forced on the command line -- it was just written as the saved preference, and
	// the relaunched process (no --renderer) reads that preference. This keeps the pref the
	// single source of truth and lets a later plain launch follow the same choice. Force
	// -novideo so the intro movies do not replay on every switch, and re-add -window only
	// if the window is currently windowed.
	std::vector<char*> args;
	args.push_back(exe);
	bool hasNoVideo = false;
	for (int i = 1; i < g_argc; i++) {
		if (SDL_strcmp(g_argv[i], "--renderer") == 0) {
			i++; // also skip its value
			continue;
		}
		if (SDL_strcmp(g_argv[i], "-window") == 0) {
			continue; // re-derived from the current window state below
		}
		if (SDL_strcmp(g_argv[i], "-novideo") == 0) {
			hasNoVideo = true;
		}
		args.push_back(g_argv[i]);
	}
	if (!hasNoVideo) {
		args.push_back((char*) "-novideo");
	}
	if (windowed) {
		args.push_back((char*) "-window");
	}
	args.push_back(nullptr);

	SDL_LogInfo(
		LOG_CATEGORY_MINIWIN,
		"relaunching for renderer '%s' (saved preference)",
		MiniwinBackendName(p_backend)
	);

	// Replace the process image on the main thread; execv tears down everything, so the
	// game's already-persisted state is what carries over.
	char** argv = args.data();
	MiniwinApp_RunOnMainThread([&exe, argv]() {
#ifdef _WIN32
		_execv(exe, argv);
#else
		execv(exe, argv);
#endif
	});
	SDL_LogError(LOG_CATEGORY_MINIWIN, "relaunch failed: %s", strerror(errno));
}

// --- Video presentation through the active backend ---

void MiniwinBackend_PresentVideoFrame(SDL_Window* p_window, const void* p_rgba, int p_width, int p_height)
{
	MiniwinRenderBackend* backend = MiniwinBackend_Acquire(p_window, p_width, p_height);
	if (!backend || !p_rgba || p_width <= 0 || p_height <= 0) {
		return;
	}

	// Reusable RGBA texture, recreated on a dimension change or backend swap.
	static Uint32 s_texture;
	static int s_texWidth;
	static int s_texHeight;
	static MiniwinRenderBackend* s_texBackend;
	if (s_texBackend != backend || s_texWidth != p_width || s_texHeight != p_height) {
		if (s_texture && s_texBackend == backend) {
			backend->DestroyTexture(s_texture);
		}
		s_texture = backend->CreateTexture(p_width, p_height, p_rgba, false);
		s_texWidth = p_width;
		s_texHeight = p_height;
		s_texBackend = backend;
	}
	else {
		backend->UpdateTexture(s_texture, p_width, p_height, p_rgba, false);
	}

	// Fit the frame into the logical render rect preserving its aspect ratio (all
	// shipped movies match the logical 640x480, so this is normally a full-rect quad).
	// The backend letterboxes the logical rect into the window at present time.
	float logicalW = (float) backend->m_width;
	float logicalH = (float) backend->m_height;
	float logicalAspect = logicalW / logicalH;
	float frameAspect = (float) p_width / (float) p_height;
	float quadW = logicalW;
	float quadH = logicalH;
	if (frameAspect > logicalAspect) {
		quadH = logicalW / frameAspect;
	}
	else {
		quadW = logicalH * frameAspect;
	}
	float x0 = (logicalW - quadW) * 0.5f;
	float y0 = (logicalH - quadH) * 0.5f;
	float x1 = x0 + quadW;
	float y1 = y0 + quadH;

	D3DTLVERTEX quad[4] = {};
	for (D3DTLVERTEX& vertex : quad) {
		vertex.sz = 0.0f;
		vertex.rhw = 1.0f;
		vertex.color = 0xFFFFFFFF;
		vertex.specular = 0;
	}
	quad[0].sx = x0;
	quad[0].sy = y0;
	quad[0].tu = 0.0f;
	quad[0].tv = 0.0f;
	quad[1].sx = x1;
	quad[1].sy = y0;
	quad[1].tu = 1.0f;
	quad[1].tv = 0.0f;
	quad[2].sx = x0;
	quad[2].sy = y1;
	quad[2].tu = 0.0f;
	quad[2].tv = 1.0f;
	quad[3].sx = x1;
	quad[3].sy = y1;
	quad[3].tu = 1.0f;
	quad[3].tv = 1.0f;

	MiniwinRasterState state;
	state.zEnable = false;
	state.zWrite = false;
	state.alphaBlend = false;
	state.textured = true;
	state.textureId = s_texture;
	state.textureLinear = true;
	state.textureWrap = false;
	state.colorOp = MiniwinTextureOp::Texture;
	state.alphaOp = MiniwinTextureOp::Texture;

	const Uint16 indices[6] = {0, 1, 2, 2, 1, 3};

	backend->Clear(nullptr, 0.0f, 0.0f, 0.0f, true, false);
	backend->DrawTriangles(state, quad, 4, indices, 6);
	backend->Present();
}

// --- Screenshot hook (shared by the backends' Present) ---

void MiniwinBackend_HandleFrameDump(MiniwinRenderBackend* p_backend, Uint64 p_frameCounter)
{
	// RACERS_DUMP_EVERY="ms:prefix" writes prefix-<seconds>.bmp every ms milliseconds.
	static const char* periodicSpec = getenv("RACERS_DUMP_EVERY");
	if (periodicSpec && periodicSpec[0]) {
		const char* colon = SDL_strchr(periodicSpec, ':');
		if (colon) {
			static Uint64 lastDumpMs;
			Uint64 intervalMs = (Uint64) SDL_atoi(periodicSpec);
			Uint64 nowMs = SDL_GetTicks();
			if (intervalMs && nowMs - lastDumpMs >= intervalMs) {
				lastDumpMs = nowMs;
				char path[512];
				SDL_snprintf(path, sizeof(path), "%s-%03llu.bmp", colon + 1, (unsigned long long) (nowMs / 1000));
				SDL_Surface* shot = p_backend->ReadBackbuffer();
				if (shot) {
					SDL_SaveBMP(shot, path);
					SDL_DestroySurface(shot);
				}
			}
		}
	}

	// RACERS_DUMP_FRAME="N:path,..." dumps frame N to path.
	static const char* dumpSpec = getenv("RACERS_DUMP_FRAME");
	if (!dumpSpec || !dumpSpec[0]) {
		return;
	}
	char spec[512];
	SDL_strlcpy(spec, dumpSpec, sizeof(spec));
	char* saveptr = nullptr;
	for (char* entry = SDL_strtok_r(spec, ",", &saveptr); entry; entry = SDL_strtok_r(nullptr, ",", &saveptr)) {
		char* colon = SDL_strchr(entry, ':');
		if (!colon) {
			continue;
		}
		*colon = '\0';
		if ((Uint64) SDL_atoi(entry) == p_frameCounter) {
			SDL_Surface* shot = p_backend->ReadBackbuffer();
			if (shot) {
				SDL_SaveBMP(shot, colon + 1);
				SDL_DestroySurface(shot);
			}
		}
	}
}
