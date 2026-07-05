// [library:2d] DirectDraw surface emulation. CPU-backed pixel storage; presentation
// and 3D rendering are wired to the render backends (M2).

#include "ddraw_impl.h"
#include "miniwin.h"
#include "renderbackend.h"

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
	if (m_desc.ddpfPixelFormat.dwFlags &
		(DDPF_PALETTEINDEXED8 | DDPF_PALETTEINDEXED4 | DDPF_PALETTEINDEXED2 | DDPF_PALETTEINDEXED1)) {
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

HRESULT MiniwinSurface::Blt(
	LPRECT lpDestRect,
	LPDIRECTDRAWSURFACE lpDDSrcSurface,
	LPRECT lpSrcRect,
	DWORD dwFlags,
	LPDDBLTFX lpDDBltFx
)
{
	MiniwinRenderBackend* backend = m_ddraw ? m_ddraw->GetBackend() : nullptr;

	if (dwFlags & DDBLT_COLORFILL) {
		// Fill of the 3D render target (usually black). Only a full clear is used.
		if (backend && lpDDBltFx) {
			backend->Clear(nullptr, 0.f, 0.f, 0.f, true, false);
		}
		return DD_OK;
	}

	// A blit from the render surface to the primary surface is the windowed-mode
	// present.
	MiniwinSurface* source = static_cast<MiniwinSurface*>(lpDDSrcSurface);
	if (backend && m_kind == MiniwinSurfaceKind::Primary && source) {
		if (source->m_pixelsDirty) {
			source->CompositeToBackend(backend);
		}
		backend->Present();
	}

	return DD_OK;
}

HRESULT MiniwinSurface::Flip(LPDIRECTDRAWSURFACE lpDDSurfaceTargetOverride, DWORD dwFlags)
{
	// Fullscreen present.
	MiniwinRenderBackend* backend = m_ddraw ? m_ddraw->GetBackend() : nullptr;
	if (backend) {
		if (m_backBuffer && m_backBuffer->m_pixelsDirty) {
			m_backBuffer->CompositeToBackend(backend);
		}
		if (m_pixelsDirty) {
			CompositeToBackend(backend);
		}
		backend->Present();
	}
	return DD_OK;
}

void MiniwinSurface::CompositeToBackend(MiniwinRenderBackend* p_backend)
{
	m_pixelsDirty = false;
	if (!m_pixels) {
		return;
	}

	int width = (int) m_desc.dwWidth;
	int height = (int) m_desc.dwHeight;
	void* rgba = malloc((size_t) width * height * 4);
	if (!rgba || !ConvertToRGBA(rgba)) {
		free(rgba);
		return;
	}

	if (m_compositeTexture) {
		p_backend->UpdateTexture(m_compositeTexture, width, height, rgba, false);
	}
	else {
		m_compositeTexture = p_backend->CreateTexture(width, height, rgba, false);
	}
	free(rgba);
	if (!m_compositeTexture) {
		return;
	}

	// Fullscreen quad in the logical coordinate space, on top of everything.
	float w = (float) p_backend->m_width;
	float h = (float) p_backend->m_height;
	D3DTLVERTEX vertices[4];
	memset(vertices, 0, sizeof(vertices));
	for (int i = 0; i < 4; i++) {
		vertices[i].sx = (i == 1 || i == 3) ? w : 0.0f;
		vertices[i].sy = (i >= 2) ? h : 0.0f;
		vertices[i].sz = 0.0f;
		vertices[i].rhw = 1.0f;
		vertices[i].color = 0xFFFFFFFF;
		vertices[i].specular = 0;
		vertices[i].tu = (i == 1 || i == 3) ? 1.0f : 0.0f;
		vertices[i].tv = (i >= 2) ? 1.0f : 0.0f;
	}
	const Uint16 indices[6] = {0, 1, 2, 2, 1, 3};

	MiniwinRasterState state;
	state.zEnable = false;
	state.zWrite = false;
	state.alphaBlend = false;
	state.cullMode = D3DCULL_NONE;
	state.textured = true;
	state.textureId = m_compositeTexture;
	state.textureLinear = false;
	state.textureWrap = false;
	state.colorOp = MiniwinTextureOp::Texture;
	state.alphaOp = MiniwinTextureOp::Texture;
	p_backend->DrawTriangles(state, vertices, 4, indices, 6);
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
	if (m_kind == MiniwinSurfaceKind::Texture) {
		m_textureDirty = true;
	}
	else {
		m_pixelsDirty = true;
	}
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
	if (m_kind == MiniwinSurfaceKind::Texture) {
		m_textureDirty = true;
	}
	return DD_OK;
}

HRESULT MiniwinSurface::SetPalette(LPDIRECTDRAWPALETTE lpDDPalette)
{
	m_palette = lpDDPalette;
	if (m_kind == MiniwinSurfaceKind::Texture) {
		m_textureDirty = true;
		m_paletteVersion = -1;
	}
	return DD_OK;
}

HRESULT MiniwinSurface::GetHandle(void* lpDirect3DDevice, DWORD* lpHandle)
{
	static DWORD counter = 0;
	if (lpHandle) {
		*lpHandle = ++counter;
	}
	return DD_OK;
}

HRESULT MiniwinSurface::PaletteChanged(DWORD dwStart, DWORD dwCount)
{
	return DD_OK;
}

bool MiniwinSurface::ConvertToRGBA(void* p_out) const
{
	if (!m_pixels) {
		return false;
	}

	const int width = (int) m_desc.dwWidth;
	const int height = (int) m_desc.dwHeight;
	const DDPIXELFORMAT& format = m_desc.ddpfPixelFormat;
	Uint8* out = (Uint8*) p_out;

	const bool paletted = (format.dwFlags & (DDPF_PALETTEINDEXED8 | DDPF_PALETTEINDEXED4)) != 0;
	const DWORD keyValue = m_colorKey.dwColorSpaceLowValue;

	if (paletted) {
		const PALETTEENTRY* entries = m_palette ? m_palette->m_entries : nullptr;
		for (int y = 0; y < height; y++) {
			const Uint8* src = (const Uint8*) m_pixels + (size_t) y * m_pitch;
			for (int x = 0; x < width; x++) {
				Uint8 index = src[x];
				Uint8* dst = out + ((size_t) y * width + x) * 4;
				if (entries) {
					dst[0] = entries[index].peRed;
					dst[1] = entries[index].peGreen;
					dst[2] = entries[index].peBlue;
				}
				else {
					dst[0] = dst[1] = dst[2] = index;
				}
				dst[3] = (m_hasColorKey && index == keyValue) ? 0 : 255;
			}
		}
		return true;
	}

	const DWORD bpp = format.dwRGBBitCount;
	const DWORD rMask = format.dwRBitMask;
	const DWORD gMask = format.dwGBitMask;
	const DWORD bMask = format.dwBBitMask;
	const DWORD aMask = (format.dwFlags & DDPF_ALPHAPIXELS) ? format.dwRGBAlphaBitMask : 0;

	// Expands a masked channel to 8 bits.
	auto expand = [](DWORD value, DWORD mask) -> Uint8 {
		if (!mask) {
			return 255;
		}
		int shift = 0;
		DWORD m = mask;
		while (!(m & 1)) {
			m >>= 1;
			shift++;
		}
		int bits = 0;
		while (m & 1) {
			m >>= 1;
			bits++;
		}
		DWORD channel = (value & mask) >> shift;
		if (bits >= 8) {
			return (Uint8) (channel >> (bits - 8));
		}
		// Replicate high bits into the low end for a full 0-255 range.
		return (Uint8) ((channel * 255) / ((1u << bits) - 1));
	};

	for (int y = 0; y < height; y++) {
		const Uint8* src = (const Uint8*) m_pixels + (size_t) y * m_pitch;
		for (int x = 0; x < width; x++) {
			DWORD value;
			if (bpp == 16) {
				value = ((const Uint16*) src)[x];
			}
			else {
				value = ((const Uint32*) src)[x];
			}

			Uint8* dst = out + ((size_t) y * width + x) * 4;
			dst[0] = expand(value, rMask);
			dst[1] = expand(value, gMask);
			dst[2] = expand(value, bMask);

			if (m_hasColorKey && value == keyValue) {
				dst[3] = 0;
			}
			else if (aMask) {
				dst[3] = expand(value, aMask);
			}
			else {
				dst[3] = 255;
			}
		}
	}

	return true;
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
