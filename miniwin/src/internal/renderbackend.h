#pragma once

// [library:3d] Render backend abstraction for the Direct3D immediate-mode emulation.
// The game submits pre-transformed D3DTLVERTEX triangles; a backend is a rasterizer
// state machine (SDL3 GPU / OpenGL 3.3 / OpenGL ES 3).

#include <SDL3/SDL.h>
#include <miniwin/d3d.h>

// How a channel combines the texture sample with the vertex diffuse color
// (D3DTSS_COLOROP/ALPHAOP reduced to the three ops the game uses).
enum class MiniwinTextureOp : Uint8 {
	Modulate = 0, // texture * diffuse (D3DTOP_MODULATE)
	Texture = 1,  // texture only (D3DTOP_SELECTARG1)
	Diffuse = 2,  // diffuse only (D3DTOP_SELECTARG2 / no texture bound)
};

// Snapshot of the D3D render states a draw call depends on.
struct MiniwinRasterState {
	bool zEnable = true;
	bool zWrite = true;
	D3DCMPFUNC zFunc = D3DCMP_LESSEQUAL;
	bool alphaBlend = false;
	D3DBLEND srcBlend = D3DBLEND_SRCALPHA;
	D3DBLEND destBlend = D3DBLEND_INVSRCALPHA;
	bool alphaTest = false;
	D3DCMPFUNC alphaFunc = D3DCMP_ALWAYS;
	float alphaRef = 0.f;
	D3DCULL cullMode = D3DCULL_NONE;
	bool specular = false;
	bool textured = false;
	Uint32 textureId = 0;
	bool textureLinear = true;
	bool textureWrap = true;
	bool colorKeyTest = false;
	MiniwinTextureOp colorOp = MiniwinTextureOp::Modulate;
	MiniwinTextureOp alphaOp = MiniwinTextureOp::Modulate;
};

class MiniwinRenderBackend {
public:
	virtual ~MiniwinRenderBackend() = default;

	virtual bool Init(SDL_Window* p_window) = 0;
	virtual void Resize(int p_width, int p_height) = 0;

	// Texture management. Pixels are RGBA8, tightly packed.
	virtual Uint32 CreateTexture(int p_width, int p_height, const void* p_rgba, bool p_mipmaps) = 0;
	virtual void UpdateTexture(Uint32 p_id, int p_width, int p_height, const void* p_rgba, bool p_mipmaps) = 0;
	virtual void DestroyTexture(Uint32 p_id) = 0;

	virtual void BeginScene() = 0;
	virtual void Clear(const SDL_Rect* p_rect, float p_r, float p_g, float p_b, bool p_color, bool p_depth) = 0;
	virtual void DrawTriangles(
		const MiniwinRasterState& p_state,
		const D3DTLVERTEX* p_vertices,
		Uint32 p_vertexCount,
		const Uint16* p_indices,
		Uint32 p_indexCount
	) = 0;
	virtual void EndScene() = 0;
	virtual void Present() = 0;

	// Reads the current backbuffer into an SDL_Surface (caller owns it).
	virtual SDL_Surface* ReadBackbuffer() = 0;

	// The logical render size the game targets (e.g. 640x480).
	int m_width = 640;
	int m_height = 480;
};

// Cross-object draw statistics (RACERS_GL_STATS).
extern Uint32 g_miniwinStatSetRenderState;
extern Uint32 g_miniwinStatSetTexture;

// Returns the process-shared render backend for the window, creating it on first use
// (on the game thread; the GL context / GPU device is created and bound there). The
// same instance is shared by the DirectDraw present path and video playback, so the
// window is claimed by exactly one backend. p_width/p_height are the logical render
// size and only take effect on creation. Returns nullptr if the backend fails to
// initialize. Owned by the backend layer; released by MiniwinBackend_Shutdown().
MiniwinRenderBackend* MiniwinBackend_Acquire(SDL_Window* p_window, int p_width, int p_height);

// MiniwinBackend_PrepareWindowFlags / _PresentVideoFrame / _Shutdown and the backend
// selection config are declared in <miniwin/miniwinapp.h>.

// Backend-agnostic screenshot hook (RACERS_DUMP_EVERY / RACERS_DUMP_FRAME): reads the
// backbuffer and writes a BMP. Call once per Present with the backend's frame counter.
void MiniwinBackend_HandleFrameDump(MiniwinRenderBackend* p_backend, Uint64 p_frameCounter);
