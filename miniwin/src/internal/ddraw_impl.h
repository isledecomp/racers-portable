#pragma once

// Internal object model for the DirectDraw/Direct3D emulation.

#include <SDL3/SDL.h>
#include <miniwin/d3d.h>
#include <miniwin/ddraw.h>

enum class MiniwinSurfaceKind {
	Primary,
	BackBuffer,
	Offscreen,
	Texture,
	ZBuffer,
};

struct MiniwinSurface : public IDirectDrawSurface {
	MiniwinSurface(const DDSURFACEDESC2& p_desc, MiniwinSurfaceKind p_kind);
	~MiniwinSurface() override;

	HRESULT QueryInterface(REFIID riid, void** ppvObject) override;
	HRESULT AddAttachedSurface(LPDIRECTDRAWSURFACE lpDDSAttachedSurface) override;
	HRESULT Blt(LPRECT lpDestRect, LPDIRECTDRAWSURFACE lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpDDBltFx) override;
	HRESULT DeleteAttachedSurface(DWORD dwFlags, LPDIRECTDRAWSURFACE lpDDSAttachedSurface) override;
	HRESULT Flip(LPDIRECTDRAWSURFACE lpDDSurfaceTargetOverride, DWORD dwFlags) override;
	HRESULT GetAttachedSurface(LPDDSCAPS2 lpDDSCaps, LPDIRECTDRAWSURFACE* lplpDDAttachedSurface) override;
	HRESULT GetSurfaceDesc(LPDDSURFACEDESC2 lpDDSurfaceDesc) override;
	HRESULT GetPixelFormat(LPDDPIXELFORMAT lpDDPixelFormat) override;
	BOOL IsLost() override;
	HRESULT Lock(LPRECT lpDestRect, LPDDSURFACEDESC2 lpDDSurfaceDesc, DWORD dwFlags, HANDLE hEvent) override;
	HRESULT Restore() override;
	HRESULT SetClipper(LPDIRECTDRAWCLIPPER lpDDClipper) override;
	HRESULT SetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey) override;
	HRESULT SetPalette(LPDIRECTDRAWPALETTE lpDDPalette) override;
	HRESULT Unlock(LPRECT lpRect) override;
	HRESULT GetHandle(void* lpDirect3DDevice, DWORD* lpHandle) override;
	HRESULT PaletteChanged(DWORD dwStart, DWORD dwCount) override;

	void EnsurePixels();
	int BytesPerPixel() const;

	DDSURFACEDESC2 m_desc;
	MiniwinSurfaceKind m_kind;
	MiniwinSurface* m_backBuffer = nullptr;    // primary flip chain
	MiniwinSurface* m_attachedDepth = nullptr; // render target's z-buffer
	IDirectDrawPalette* m_palette = nullptr;
	IDirectDrawClipper* m_clipper = nullptr;
	DDCOLORKEY m_colorKey = {};
	bool m_hasColorKey = false;
	void* m_pixels = nullptr;
	int m_pitch = 0;
};

struct MiniwinDirectDraw : public IDirectDraw {
	~MiniwinDirectDraw() override;

	HRESULT QueryInterface(REFIID riid, void** ppvObject) override;
	HRESULT CreateClipper(DWORD dwFlags, LPDIRECTDRAWCLIPPER* lplpDDClipper, IUnknown* pUnkOuter) override;
	HRESULT CreatePalette(DWORD dwFlags, LPPALETTEENTRY lpDDColorArray, LPDIRECTDRAWPALETTE* lplpDDPalette, IUnknown* pUnkOuter) override;
	HRESULT CreateSurface(LPDDSURFACEDESC2 lpDDSurfaceDesc, LPDIRECTDRAWSURFACE* lplpDDSurface, IUnknown* pUnkOuter) override;
	HRESULT GetCaps(LPDDCAPS lpDDDriverCaps, LPDDCAPS lpDDHELCaps) override;
	HRESULT GetDisplayMode(LPDDSURFACEDESC lpDDSurfaceDesc) override;
	HRESULT RestoreDisplayMode() override;
	HRESULT SetCooperativeLevel(HWND hWnd, DWORD dwFlags) override;
	HRESULT SetDisplayMode(DWORD dwWidth, DWORD dwHeight, DWORD dwBPP, DWORD dwRefreshRate, DWORD dwFlags) override;

	HWND m_hwnd = nullptr;
	DWORD m_coopFlags = 0;
	DWORD m_modeWidth = 0;
	DWORD m_modeHeight = 0;
	DWORD m_modeBpp = 0;
};

// Fills a DDPIXELFORMAT for a plain RGB mode of the given depth (565 for 16 bpp,
// XRGB8888 for 32 bpp).
void MiniwinFillRgbPixelFormat(DDPIXELFORMAT* p_format, DWORD p_bpp);

// The one Direct3D device GUID miniwin advertises.
extern const GUID MINIWIN_GUID_Direct3DDevice;
