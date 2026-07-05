// [library:2d] DirectDraw surface emulation. CPU-backed pixel storage; presentation
// and 3D rendering are wired to the render backends (M2).

#include "ddraw_impl.h"
#include "miniwin.h"

#include <stdlib.h>
#include <string.h>

MiniwinSurface::MiniwinSurface(const DDSURFACEDESC2& p_desc, MiniwinSurfaceKind p_kind) : m_desc(p_desc), m_kind(p_kind)
{
	m_desc.dwSize = sizeof(DDSURFACEDESC2);
}

MiniwinSurface::~MiniwinSurface()
{
	if (m_backBuffer) {
		m_backBuffer->Release();
		m_backBuffer = nullptr;
	}
	if (m_pixels) {
		free(m_pixels);
		m_pixels = nullptr;
	}
}

int MiniwinSurface::BytesPerPixel() const
{
	if (m_kind == MiniwinSurfaceKind::ZBuffer) {
		DWORD depth = m_desc.ddpfPixelFormat.dwZBufferBitDepth;
		return depth ? (int) ((depth + 7) / 8) : 2;
	}

	DWORD bpp = m_desc.ddpfPixelFormat.dwRGBBitCount;
	if (m_desc.ddpfPixelFormat.dwFlags & (DDPF_PALETTEINDEXED8 | DDPF_PALETTEINDEXED4 | DDPF_PALETTEINDEXED2 | DDPF_PALETTEINDEXED1)) {
		bpp = 8;
	}
	if (bpp == 0) {
		bpp = 32;
	}
	return (int) ((bpp + 7) / 8);
}

void MiniwinSurface::EnsurePixels()
{
	if (m_pixels) {
		return;
	}

	int width = (int) m_desc.dwWidth;
	int height = (int) m_desc.dwHeight;
	if (width <= 0 || height <= 0) {
		width = 1;
		height = 1;
	}

	m_pitch = width * BytesPerPixel();
	// Align pitch to 8 bytes like typical drivers do.
	m_pitch = (m_pitch + 7) & ~7;
	m_pixels = calloc(1, (size_t) m_pitch * height);
}

HRESULT MiniwinSurface::QueryInterface(REFIID riid, void** ppvObject)
{
	if (!ppvObject) {
		return E_NOINTERFACE;
	}

	if (riid == IID_IDirectDrawSurface || riid == IID_IDirect3DTexture2) {
		AddRef();
		*ppvObject = this;
		return DD_OK;
	}

	*ppvObject = nullptr;
	return E_NOINTERFACE;
}

HRESULT MiniwinSurface::AddAttachedSurface(LPDIRECTDRAWSURFACE lpDDSAttachedSurface)
{
	m_attachedDepth = static_cast<MiniwinSurface*>(lpDDSAttachedSurface);
	return DD_OK;
}

HRESULT MiniwinSurface::DeleteAttachedSurface(DWORD dwFlags, LPDIRECTDRAWSURFACE lpDDSAttachedSurface)
{
	if (m_attachedDepth == static_cast<MiniwinSurface*>(lpDDSAttachedSurface)) {
		m_attachedDepth = nullptr;
	}
	return DD_OK;
}

HRESULT MiniwinSurface::Blt(LPRECT lpDestRect, LPDIRECTDRAWSURFACE lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpDDBltFx)
{
	if (dwFlags & DDBLT_COLORFILL) {
		// Clearing the render target; handled by the render backend (M2).
		return DD_OK;
	}

	// Presentation blit (render surface -> primary) or surface copy; handled by the
	// render backend (M2).
	return DD_OK;
}

HRESULT MiniwinSurface::Flip(LPDIRECTDRAWSURFACE lpDDSurfaceTargetOverride, DWORD dwFlags)
{
	// Presentation; handled by the render backend (M2).
	return DD_OK;
}

HRESULT MiniwinSurface::GetAttachedSurface(LPDDSCAPS2 lpDDSCaps, LPDIRECTDRAWSURFACE* lplpDDAttachedSurface)
{
	if (!lplpDDAttachedSurface) {
		return DDERR_INVALIDPARAMS;
	}

	if (lpDDSCaps && (lpDDSCaps->dwCaps & DDSCAPS_BACKBUFFER) && m_backBuffer) {
		m_backBuffer->AddRef();
		*lplpDDAttachedSurface = m_backBuffer;
		return DD_OK;
	}

	*lplpDDAttachedSurface = nullptr;
	return DDERR_NOTFOUND;
}

HRESULT MiniwinSurface::GetSurfaceDesc(LPDDSURFACEDESC2 lpDDSurfaceDesc)
{
	if (!lpDDSurfaceDesc) {
		return DDERR_INVALIDPARAMS;
	}

	DWORD size = lpDDSurfaceDesc->dwSize;
	*lpDDSurfaceDesc = m_desc;
	lpDDSurfaceDesc->dwSize = size ? size : sizeof(DDSURFACEDESC2);
	lpDDSurfaceDesc->dwFlags = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT | DDSD_PIXELFORMAT | DDSD_PITCH;
	lpDDSurfaceDesc->lPitch = m_pitch ? m_pitch : (LONG) (m_desc.dwWidth * BytesPerPixel());
	lpDDSurfaceDesc->lpSurface = m_pixels;
	return DD_OK;
}

HRESULT MiniwinSurface::GetPixelFormat(LPDDPIXELFORMAT lpDDPixelFormat)
{
	if (!lpDDPixelFormat) {
		return DDERR_INVALIDPARAMS;
	}

	*lpDDPixelFormat = m_desc.ddpfPixelFormat;
	return DD_OK;
}

BOOL MiniwinSurface::IsLost()
{
	return FALSE;
}

HRESULT MiniwinSurface::Lock(LPRECT lpDestRect, LPDDSURFACEDESC2 lpDDSurfaceDesc, DWORD dwFlags, HANDLE hEvent)
{
	EnsurePixels();

	if (lpDDSurfaceDesc) {
		DWORD size = lpDDSurfaceDesc->dwSize;
		*lpDDSurfaceDesc = m_desc;
		lpDDSurfaceDesc->dwSize = size ? size : sizeof(DDSURFACEDESC2);
		lpDDSurfaceDesc->lPitch = m_pitch;
		lpDDSurfaceDesc->lpSurface = m_pixels;
	}

	return DD_OK;
}

HRESULT MiniwinSurface::Unlock(LPRECT lpRect)
{
	return DD_OK;
}

HRESULT MiniwinSurface::Restore()
{
	return DD_OK;
}

HRESULT MiniwinSurface::SetClipper(LPDIRECTDRAWCLIPPER lpDDClipper)
{
	m_clipper = lpDDClipper;
	return DD_OK;
}

HRESULT MiniwinSurface::SetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey)
{
	if (lpDDColorKey) {
		m_colorKey = *lpDDColorKey;
		m_hasColorKey = true;
	}
	else {
		m_hasColorKey = false;
	}
	return DD_OK;
}

HRESULT MiniwinSurface::SetPalette(LPDIRECTDRAWPALETTE lpDDPalette)
{
	m_palette = lpDDPalette;
	return DD_OK;
}

HRESULT MiniwinSurface::GetHandle(void* lpDirect3DDevice, DWORD* lpHandle)
{
	if (lpHandle) {
		*lpHandle = (DWORD) (uintptr_t) this;
	}
	return DD_OK;
}

HRESULT MiniwinSurface::PaletteChanged(DWORD dwStart, DWORD dwCount)
{
	return DD_OK;
}

// --- Base-interface default bodies (never instantiated directly) ---

HRESULT IDirectDrawSurface::QueryInterface(REFIID riid, void** ppvObject)
{
	return IUnknown::QueryInterface(riid, ppvObject);
}

HRESULT IDirectDrawSurface::AddAttachedSurface(LPDIRECTDRAWSURFACE)
{
	return DDERR_UNSUPPORTED;
}
HRESULT IDirectDrawSurface::Blt(LPRECT, LPDIRECTDRAWSURFACE, LPRECT, DWORD, LPDDBLTFX)
{
	return DDERR_UNSUPPORTED;
}
HRESULT IDirectDrawSurface::DeleteAttachedSurface(DWORD, LPDIRECTDRAWSURFACE)
{
	return DDERR_UNSUPPORTED;
}
HRESULT IDirectDrawSurface::Flip(LPDIRECTDRAWSURFACE, DWORD)
{
	return DDERR_UNSUPPORTED;
}
HRESULT IDirectDrawSurface::GetAttachedSurface(LPDDSCAPS2, LPDIRECTDRAWSURFACE*)
{
	return DDERR_UNSUPPORTED;
}
HRESULT IDirectDrawSurface::GetSurfaceDesc(LPDDSURFACEDESC2)
{
	return DDERR_UNSUPPORTED;
}
HRESULT IDirectDrawSurface::GetPixelFormat(LPDDPIXELFORMAT)
{
	return DDERR_UNSUPPORTED;
}
BOOL IDirectDrawSurface::IsLost()
{
	return FALSE;
}
HRESULT IDirectDrawSurface::Lock(LPRECT, LPDDSURFACEDESC2, DWORD, HANDLE)
{
	return DDERR_UNSUPPORTED;
}
HRESULT IDirectDrawSurface::Restore()
{
	return DD_OK;
}
HRESULT IDirectDrawSurface::SetClipper(LPDIRECTDRAWCLIPPER)
{
	return DD_OK;
}
HRESULT IDirectDrawSurface::SetColorKey(DWORD, LPDDCOLORKEY)
{
	return DD_OK;
}
HRESULT IDirectDrawSurface::SetPalette(LPDIRECTDRAWPALETTE)
{
	return DD_OK;
}
HRESULT IDirectDrawSurface::Unlock(LPRECT)
{
	return DD_OK;
}
HRESULT IDirectDrawSurface::GetHandle(void*, DWORD*)
{
	return DDERR_UNSUPPORTED;
}
HRESULT IDirectDrawSurface::PaletteChanged(DWORD, DWORD)
{
	return DD_OK;
}

// --- IDirectDraw base bodies ---

HRESULT IDirectDraw::CreateClipper(DWORD, LPDIRECTDRAWCLIPPER*, IUnknown*)
{
	return DDERR_UNSUPPORTED;
}
HRESULT IDirectDraw::CreatePalette(DWORD, LPPALETTEENTRY, LPDIRECTDRAWPALETTE*, IUnknown*)
{
	return DDERR_UNSUPPORTED;
}
HRESULT IDirectDraw::CreateSurface(LPDDSURFACEDESC2, LPDIRECTDRAWSURFACE*, IUnknown*)
{
	return DDERR_UNSUPPORTED;
}
HRESULT IDirectDraw::GetCaps(LPDDCAPS, LPDDCAPS)
{
	return DDERR_UNSUPPORTED;
}
HRESULT IDirectDraw::GetDisplayMode(LPDDSURFACEDESC)
{
	return DDERR_UNSUPPORTED;
}
HRESULT IDirectDraw::RestoreDisplayMode()
{
	return DD_OK;
}
HRESULT IDirectDraw::SetCooperativeLevel(HWND, DWORD)
{
	return DD_OK;
}
HRESULT IDirectDraw::SetDisplayMode(DWORD, DWORD, DWORD, DWORD, DWORD)
{
	return DD_OK;
}
