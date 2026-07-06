// [library:2d] DirectDraw object + creation entry points.

#include "backends.h"
#include "ddraw_impl.h"
#include "miniwin.h"
#include "renderbackend.h"

#include <string.h>

MINIWIN_DEFINE_GUID(
	MINIWIN_GUID_Direct3DDevice,
	0x682656f3,
	0x121a,
	0x4d97,
	0x9a,
	0xb2,
	0xc5,
	0x88,
	0x14,
	0x91,
	0x00,
	0x01
);

// IDirect3D3 lives in d3d.cpp
IDirect3D3* MiniwinCreateDirect3D(MiniwinDirectDraw* p_ddraw);

void MiniwinFillRgbPixelFormat(DDPIXELFORMAT* p_format, DWORD p_bpp)
{
	memset(p_format, 0, sizeof(*p_format));
	p_format->dwSize = sizeof(*p_format);
	p_format->dwFlags = DDPF_RGB;
	p_format->dwRGBBitCount = p_bpp;

	if (p_bpp == 16) {
		p_format->dwRBitMask = 0xF800;
		p_format->dwGBitMask = 0x07E0;
		p_format->dwBBitMask = 0x001F;
	}
	else {
		p_format->dwRBitMask = 0x00FF0000;
		p_format->dwGBitMask = 0x0000FF00;
		p_format->dwBBitMask = 0x000000FF;
	}
}

HRESULT DirectDrawEnumerate(LPDDENUMCALLBACKA lpCallback, LPVOID lpContext)
{
	// One display driver per compiled, usable render backend, so the in-game Options ->
	// Video carousel can switch renderers (active backend enumerated first).
	MiniwinBackendId drivers[4];
	int count = MiniwinBackend_EnumDrivers(drivers, 4);
	for (int i = 0; i < count; i++) {
		GUID guid = *MiniwinBackendGuid(drivers[i]);
		char name[32];
		SDL_strlcpy(name, MiniwinBackendDisplayName(drivers[i]), sizeof(name));
		if (!lpCallback(&guid, name, name, lpContext)) {
			break;
		}
	}
	return DD_OK;
}

HRESULT DirectDrawCreate(GUID* lpGUID, LPDIRECTDRAW* lplpDD, IUnknown* pUnkOuter)
{
	if (!lplpDD) {
		return DDERR_INVALIDPARAMS;
	}

	MiniwinDirectDraw* ddraw = new MiniwinDirectDraw();
	MiniwinBackendId requested;
	ddraw->m_requestedBackend = MiniwinBackendFromGuid(lpGUID, &requested) ? requested : MiniwinBackendActive();

	*lplpDD = ddraw;
	return DD_OK;
}

HRESULT DirectDrawCreateClipper(DWORD dwFlags, LPDIRECTDRAWCLIPPER* lplpDDClipper, IUnknown* pUnkOuter)
{
	if (!lplpDDClipper) {
		return DDERR_INVALIDPARAMS;
	}

	*lplpDDClipper = new IDirectDrawClipper();
	return DD_OK;
}

HRESULT IDirectDrawClipper::SetHWnd(DWORD dwFlags, HWND hWnd)
{
	return DD_OK;
}

MiniwinDirectDraw::~MiniwinDirectDraw() = default;

HRESULT MiniwinDirectDraw::QueryInterface(REFIID riid, void** ppvObject)
{
	if (!ppvObject) {
		return E_NOINTERFACE;
	}

	if (riid == IID_IDirectDraw4) {
		AddRef();
		*ppvObject = static_cast<IDirectDraw*>(this);
		return DD_OK;
	}

	if (riid == IID_IDirect3D3) {
		*ppvObject = MiniwinCreateDirect3D(this);
		return DD_OK;
	}

	*ppvObject = nullptr;
	return E_NOINTERFACE;
}

HRESULT IDirectDraw::QueryInterface(REFIID riid, void** ppvObject)
{
	return IUnknown::QueryInterface(riid, ppvObject);
}

HRESULT MiniwinDirectDraw::CreateClipper(DWORD dwFlags, LPDIRECTDRAWCLIPPER* lplpDDClipper, IUnknown* pUnkOuter)
{
	return DirectDrawCreateClipper(dwFlags, lplpDDClipper, pUnkOuter);
}

HRESULT MiniwinDirectDraw::CreatePalette(
	DWORD dwFlags,
	LPPALETTEENTRY lpDDColorArray,
	LPDIRECTDRAWPALETTE* lplpDDPalette,
	IUnknown* pUnkOuter
)
{
	if (!lplpDDPalette) {
		return DDERR_INVALIDPARAMS;
	}

	*lplpDDPalette = new IDirectDrawPalette(dwFlags, lpDDColorArray);
	return DD_OK;
}

HRESULT MiniwinDirectDraw::CreateSurface(
	LPDDSURFACEDESC2 lpDDSurfaceDesc,
	LPDIRECTDRAWSURFACE* lplpDDSurface,
	IUnknown* pUnkOuter
)
{
	if (!lpDDSurfaceDesc || !lplpDDSurface) {
		return DDERR_INVALIDPARAMS;
	}

	DDSURFACEDESC2 desc = *lpDDSurfaceDesc;
	DWORD caps = desc.ddsCaps.dwCaps;
	MiniwinSurfaceKind kind = MiniwinSurfaceKind::Offscreen;

	DWORD modeBpp = m_modeBpp ? m_modeBpp : 32;
	DWORD modeWidth = m_modeWidth ? m_modeWidth : 640;
	DWORD modeHeight = m_modeHeight ? m_modeHeight : 480;

	if (caps & DDSCAPS_PRIMARYSURFACE) {
		kind = MiniwinSurfaceKind::Primary;
		desc.dwWidth = modeWidth;
		desc.dwHeight = modeHeight;
		MiniwinFillRgbPixelFormat(&desc.ddpfPixelFormat, modeBpp);
	}
	else if (caps & DDSCAPS_ZBUFFER) {
		kind = MiniwinSurfaceKind::ZBuffer;
	}
	else if (caps & DDSCAPS_TEXTURE) {
		kind = MiniwinSurfaceKind::Texture;
	}

	if (!(desc.dwFlags & DDSD_PIXELFORMAT) || desc.ddpfPixelFormat.dwSize == 0 ||
		(kind != MiniwinSurfaceKind::ZBuffer && desc.ddpfPixelFormat.dwRGBBitCount == 0 &&
		 !(desc.ddpfPixelFormat.dwFlags & (DDPF_PALETTEINDEXED8 | DDPF_PALETTEINDEXED4)))) {
		if (kind != MiniwinSurfaceKind::Primary && kind != MiniwinSurfaceKind::ZBuffer) {
			MiniwinFillRgbPixelFormat(&desc.ddpfPixelFormat, modeBpp);
		}
	}

	MiniwinSurface* surface = new MiniwinSurface(desc, kind);
	surface->m_ddraw = this;

	static const char* statsEnv = getenv("RACERS_GL_STATS");
	static int surfaceLog = 0;
	if (statsEnv && surfaceLog < 20) {
		surfaceLog++;
		SDL_LogInfo(
			LOG_CATEGORY_MINIWIN,
			"CreateSurface kind=%d %ux%u bpp=%u flags=%08x caps=%08x",
			(int) kind,
			desc.dwWidth,
			desc.dwHeight,
			desc.ddpfPixelFormat.dwRGBBitCount,
			desc.ddpfPixelFormat.dwFlags,
			caps
		);
	}

	if (kind == MiniwinSurfaceKind::Primary && (caps & DDSCAPS_FLIP) && (caps & DDSCAPS_COMPLEX)) {
		DDSURFACEDESC2 backDesc = desc;
		backDesc.ddsCaps.dwCaps = DDSCAPS_BACKBUFFER | DDSCAPS_3DDEVICE | DDSCAPS_VIDEOMEMORY;
		surface->m_backBuffer = new MiniwinSurface(backDesc, MiniwinSurfaceKind::BackBuffer);
		surface->m_backBuffer->m_ddraw = this;
	}

	*lplpDDSurface = surface;
	return DD_OK;
}

HRESULT MiniwinDirectDraw::GetCaps(LPDDCAPS lpDDDriverCaps, LPDDCAPS lpDDHELCaps)
{
	if (lpDDDriverCaps) {
		DWORD size = lpDDDriverCaps->dwSize;
		memset(lpDDDriverCaps, 0, sizeof(DDCAPS));
		lpDDDriverCaps->dwSize = size;
		lpDDDriverCaps->dwCaps = DDCAPS_3D;
		lpDDDriverCaps->dwCaps2 = DDCAPS2_CANRENDERWINDOWED;
		lpDDDriverCaps->dwVidMemTotal = 256 * 1024 * 1024;
		lpDDDriverCaps->dwVidMemFree = 256 * 1024 * 1024;
	}

	if (lpDDHELCaps) {
		DWORD size = lpDDHELCaps->dwSize;
		memset(lpDDHELCaps, 0, sizeof(DDCAPS));
		lpDDHELCaps->dwSize = size;
	}

	return DD_OK;
}

HRESULT MiniwinDirectDraw::GetDisplayMode(LPDDSURFACEDESC lpDDSurfaceDesc)
{
	if (!lpDDSurfaceDesc) {
		return DDERR_INVALIDPARAMS;
	}

	DWORD width = 640;
	DWORD height = 480;
	const SDL_DisplayMode* mode = SDL_GetDesktopDisplayMode(SDL_GetPrimaryDisplay());
	if (mode) {
		width = (DWORD) mode->w;
		height = (DWORD) mode->h;
	}

	DWORD size = lpDDSurfaceDesc->dwSize;
	memset(lpDDSurfaceDesc, 0, sizeof(DDSURFACEDESC));
	lpDDSurfaceDesc->dwSize = size;
	lpDDSurfaceDesc->dwFlags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT;
	lpDDSurfaceDesc->dwWidth = m_modeWidth ? m_modeWidth : width;
	lpDDSurfaceDesc->dwHeight = m_modeHeight ? m_modeHeight : height;
	// Report 16 bpp before any mode was set: the game compares the desktop depth with
	// its requested depth to decide whether it must go fullscreen, and 16 bpp is its
	// default. SDL abstracts the real desktop format away entirely.
	MiniwinFillRgbPixelFormat(&lpDDSurfaceDesc->ddpfPixelFormat, m_modeBpp ? m_modeBpp : 16);
	return DD_OK;
}

MiniwinRenderBackend* MiniwinDirectDraw::GetBackend()
{
	if (!m_backend && m_hwnd) {
		m_backend = MiniwinBackend_Acquire(
			reinterpret_cast<SDL_Window*>(m_hwnd),
			m_modeWidth ? (int) m_modeWidth : 640,
			m_modeHeight ? (int) m_modeHeight : 480
		);
	}
	return m_backend;
}

HRESULT MiniwinDirectDraw::RestoreDisplayMode()
{
	m_modeWidth = 0;
	m_modeHeight = 0;
	m_modeBpp = 0;
	return DD_OK;
}

HRESULT MiniwinDirectDraw::SetCooperativeLevel(HWND hWnd, DWORD dwFlags)
{
	if (hWnd) {
		m_hwnd = hWnd;
	}
	m_coopFlags = dwFlags;
	return DD_OK;
}

HRESULT MiniwinDirectDraw::SetDisplayMode(
	DWORD dwWidth,
	DWORD dwHeight,
	DWORD dwBPP,
	DWORD dwRefreshRate,
	DWORD dwFlags
)
{
	// SDL owns the actual display; fullscreen is handled at the window level by the
	// application shell. Record the requested mode so surfaces get the right format.
	m_modeWidth = dwWidth;
	m_modeHeight = dwHeight;
	m_modeBpp = dwBPP;
	MINIWIN_TRACE("SetDisplayMode %ux%ux%u", dwWidth, dwHeight, dwBPP);
	return DD_OK;
}
