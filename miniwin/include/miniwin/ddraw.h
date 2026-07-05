#pragma once

// [library:2d] DirectDraw (DirectX 6) subset used by GolDP, re-implemented over SDL3.
//
// Interface generations are collapsed: the game creates surfaces through IDirectDraw4
// with DDSURFACEDESC2 but also holds IDirectDrawSurface (v1) pointers and queries
// IDirect3DTexture2 from texture surfaces. miniwin implements a single object per
// concept and aliases the generation typedefs onto it.

#include "windows.h"

// --- Error codes (MAKE_DDHRESULT-compatible values) ---
#define DD_OK ((HRESULT) 0)
#define _FACDD 0x876
#define MAKE_DDHRESULT(code) ((HRESULT) (0x88760000 | (code)))
#define DDERR_INVALIDPARAMS MAKE_DDHRESULT(87)
#define DDERR_INVALIDRECT MAKE_DDHRESULT(150)
#define DDERR_NOTFOUND MAKE_DDHRESULT(255)
#define DDERR_OUTOFMEMORY MAKE_DDHRESULT(310)
#define DDERR_OUTOFVIDEOMEMORY MAKE_DDHRESULT(380)
#define DDERR_SURFACEBUSY MAKE_DDHRESULT(430)
#define DDERR_SURFACELOST MAKE_DDHRESULT(450)
#define DDERR_UNSUPPORTED MAKE_DDHRESULT(480)
#define DDERR_WASSTILLDRAWING MAKE_DDHRESULT(540)
#define DDERR_GENERIC E_FAIL

// --- Cooperative level ---
#define DDSCL_FULLSCREEN 0x00000001
#define DDSCL_ALLOWREBOOT 0x00000002
#define DDSCL_NORMAL 0x00000008
#define DDSCL_EXCLUSIVE 0x00000010
#define DDSCL_FPUSETUP 0x00000800

// --- DDSURFACEDESC flags ---
#define DDSD_CAPS 0x00000001
#define DDSD_HEIGHT 0x00000002
#define DDSD_WIDTH 0x00000004
#define DDSD_PITCH 0x00000008
#define DDSD_BACKBUFFERCOUNT 0x00000020
#define DDSD_ZBUFFERBITDEPTH 0x00000040
#define DDSD_ALPHABITDEPTH 0x00000080
#define DDSD_LPSURFACE 0x00000800
#define DDSD_PIXELFORMAT 0x00001000
#define DDSD_MIPMAPCOUNT 0x00020000
#define DDSD_ALL 0x007ff9ee

// --- Surface caps ---
#define DDSCAPS_BACKBUFFER 0x00000004
#define DDSCAPS_COMPLEX 0x00000008
#define DDSCAPS_FLIP 0x00000010
#define DDSCAPS_OFFSCREENPLAIN 0x00000040
#define DDSCAPS_PRIMARYSURFACE 0x00000200
#define DDSCAPS_TEXTURE 0x00001000
#define DDSCAPS_3DDEVICE 0x00002000
#define DDSCAPS_VIDEOMEMORY 0x00004000
#define DDSCAPS_ZBUFFER 0x00020000
#define DDSCAPS_MIPMAP 0x00400000
#define DDSCAPS_SYSTEMMEMORY 0x00800000

#define DDSCAPS2_TEXTUREMANAGE 0x00000010

// --- Driver caps ---
#define DDCAPS_3D 0x00000001
#define DDCAPS2_CANRENDERWINDOWED 0x00080000

// --- Palette caps ---
#define DDPCAPS_4BIT 0x00000001
#define DDPCAPS_8BITENTRIES 0x00000002
#define DDPCAPS_8BIT 0x00000004
#define DDPCAPS_ALLOW256 0x00000040
#define DDPCAPS_1BIT 0x00000100
#define DDPCAPS_2BIT 0x00000200
#define DDPCAPS_ALPHA 0x00000400

// --- Pixel format flags ---
#define DDPF_ALPHAPIXELS 0x00000001
#define DDPF_PALETTEINDEXED4 0x00000008
#define DDPF_PALETTEINDEXED8 0x00000020
#define DDPF_RGB 0x00000040
#define DDPF_ZBUFFER 0x00000400
#define DDPF_PALETTEINDEXED1 0x00000800
#define DDPF_PALETTEINDEXED2 0x00001000

// --- Bit depth flags ---
#define DDBD_1 0x00004000
#define DDBD_2 0x00002000
#define DDBD_4 0x00001000
#define DDBD_8 0x00000800
#define DDBD_16 0x00000400
#define DDBD_24 0x00000200
#define DDBD_32 0x00000100

// --- Lock flags ---
#define DDLOCK_WAIT 0x00000001
#define DDLOCK_READONLY 0x00000010
#define DDLOCK_WRITEONLY 0x00000020

// --- Blt flags ---
#define DDBLT_COLORFILL 0x00000400
#define DDBLT_WAIT 0x01000000

// --- Color key flags ---
#define DDCKEY_SRCBLT 0x00000008

struct IDirectDraw;
struct IDirectDrawSurface;
struct IDirectDrawPalette;
struct IDirectDrawClipper;

typedef IDirectDraw IDirectDraw4;
typedef IDirectDrawSurface IDirectDrawSurface4;
typedef IDirectDraw* LPDIRECTDRAW;
typedef IDirectDraw* LPDIRECTDRAW4;
typedef IDirectDrawSurface* LPDIRECTDRAWSURFACE;
typedef IDirectDrawSurface* LPDIRECTDRAWSURFACE4;
typedef IDirectDrawPalette* LPDIRECTDRAWPALETTE;
typedef IDirectDrawClipper* LPDIRECTDRAWCLIPPER;

extern const GUID IID_IDirectDraw4;
extern const GUID IID_IDirectDrawSurface;

// --- Structs ---
struct DDCOLORKEY {
	DWORD dwColorSpaceLowValue;
	DWORD dwColorSpaceHighValue;
};
typedef DDCOLORKEY* LPDDCOLORKEY;

struct DDPIXELFORMAT {
	DWORD dwSize;
	DWORD dwFlags;
	DWORD dwFourCC;
	union {
		DWORD dwRGBBitCount;
		DWORD dwYUVBitCount;
		DWORD dwZBufferBitDepth;
		DWORD dwAlphaBitDepth;
	};
	union {
		DWORD dwRBitMask;
		DWORD dwYBitMask;
	};
	union {
		DWORD dwGBitMask;
		DWORD dwUBitMask;
		DWORD dwZBitMask;
	};
	union {
		DWORD dwBBitMask;
		DWORD dwVBitMask;
	};
	union {
		DWORD dwRGBAlphaBitMask;
		DWORD dwYUVAlphaBitMask;
	};
};
typedef DDPIXELFORMAT* LPDDPIXELFORMAT;

struct DDSCAPS {
	DWORD dwCaps;
};
typedef DDSCAPS* LPDDSCAPS;

struct DDSCAPS2 {
	DWORD dwCaps;
	DWORD dwCaps2;
	DWORD dwCaps3;
	DWORD dwCaps4;
};
typedef DDSCAPS2* LPDDSCAPS2;

struct DDCAPS {
	DWORD dwSize;
	DWORD dwCaps;
	DWORD dwCaps2;
	DWORD dwCKeyCaps;
	DWORD dwFXCaps;
	DWORD dwFXAlphaCaps;
	DWORD dwPalCaps;
	DWORD dwSVCaps;
	DWORD dwVidMemTotal;
	DWORD dwVidMemFree;
	DWORD dwReserved[48];
};
typedef DDCAPS* LPDDCAPS;

struct DDSURFACEDESC {
	DWORD dwSize;
	DWORD dwFlags;
	DWORD dwHeight;
	DWORD dwWidth;
	union {
		LONG lPitch;
		DWORD dwLinearSize;
	};
	DWORD dwBackBufferCount;
	union {
		DWORD dwMipMapCount;
		DWORD dwRefreshRate;
	};
	DWORD dwAlphaBitDepth;
	DWORD dwReserved;
	LPVOID lpSurface;
	DDCOLORKEY ddckCKDestOverlay;
	DDCOLORKEY ddckCKDestBlt;
	DDCOLORKEY ddckCKSrcOverlay;
	DDCOLORKEY ddckCKSrcBlt;
	DDPIXELFORMAT ddpfPixelFormat;
	DDSCAPS ddsCaps;
};
typedef DDSURFACEDESC* LPDDSURFACEDESC;

struct DDSURFACEDESC2 {
	DWORD dwSize;
	DWORD dwFlags;
	DWORD dwHeight;
	DWORD dwWidth;
	union {
		LONG lPitch;
		DWORD dwLinearSize;
	};
	DWORD dwBackBufferCount;
	union {
		DWORD dwMipMapCount;
		DWORD dwRefreshRate;
	};
	DWORD dwAlphaBitDepth;
	DWORD dwReserved;
	LPVOID lpSurface;
	DDCOLORKEY ddckCKDestOverlay;
	DDCOLORKEY ddckCKDestBlt;
	DDCOLORKEY ddckCKSrcOverlay;
	DDCOLORKEY ddckCKSrcBlt;
	DDPIXELFORMAT ddpfPixelFormat;
	DDSCAPS2 ddsCaps;
	DWORD dwTextureStage;
};
typedef DDSURFACEDESC2* LPDDSURFACEDESC2;

struct DDBLTFX {
	DWORD dwSize;
	DWORD dwDDFX;
	DWORD dwROP;
	DWORD dwDDROP;
	DWORD dwRotationAngle;
	union {
		DWORD dwFillColor;
		DWORD dwFillDepth;
	};
	DDCOLORKEY ddckDestColorkey;
	DDCOLORKEY ddckSrcColorkey;
	DWORD dwReserved[8];
};
typedef DDBLTFX* LPDDBLTFX;

// --- Callbacks / creation ---
typedef BOOL(CALLBACK* LPDDENUMCALLBACKA)(GUID* lpGUID, LPSTR lpDriverDescription, LPSTR lpDriverName, LPVOID lpContext);
typedef LPDDENUMCALLBACKA LPDDENUMCALLBACK;

HRESULT DirectDrawEnumerate(LPDDENUMCALLBACKA lpCallback, LPVOID lpContext);
HRESULT DirectDrawCreate(GUID* lpGUID, LPDIRECTDRAW* lplpDD, IUnknown* pUnkOuter);
HRESULT DirectDrawCreateClipper(DWORD dwFlags, LPDIRECTDRAWCLIPPER* lplpDDClipper, IUnknown* pUnkOuter);

// --- Interfaces ---
struct IDirectDrawClipper : virtual public IUnknown {
	virtual HRESULT SetHWnd(DWORD dwFlags, HWND hWnd);
};

struct IDirectDrawPalette : virtual public IUnknown {
	IDirectDrawPalette(DWORD dwFlags, const PALETTEENTRY* lpEntries);

	virtual HRESULT GetCaps(LPDWORD lpdwCaps);
	virtual HRESULT GetEntries(DWORD dwFlags, DWORD dwBase, DWORD dwNumEntries, LPPALETTEENTRY lpEntries);
	virtual HRESULT SetEntries(DWORD dwFlags, DWORD dwStartingEntry, DWORD dwCount, const PALETTEENTRY* lpEntries);

	// Backing store (256 entries max, as created by the game)
	PALETTEENTRY m_entries[256];
	DWORD m_flags;
	int m_version = 0; // bumped on SetEntries so surfaces can re-convert
};

struct IDirectDrawSurface : virtual public IUnknown {
	HRESULT QueryInterface(REFIID riid, void** ppvObject) override;

	virtual HRESULT AddAttachedSurface(LPDIRECTDRAWSURFACE lpDDSAttachedSurface);
	virtual HRESULT Blt(LPRECT lpDestRect, LPDIRECTDRAWSURFACE lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpDDBltFx);
	virtual HRESULT DeleteAttachedSurface(DWORD dwFlags, LPDIRECTDRAWSURFACE lpDDSAttachedSurface);
	virtual HRESULT Flip(LPDIRECTDRAWSURFACE lpDDSurfaceTargetOverride, DWORD dwFlags);
	virtual HRESULT GetAttachedSurface(LPDDSCAPS2 lpDDSCaps, LPDIRECTDRAWSURFACE* lplpDDAttachedSurface);
	virtual HRESULT GetSurfaceDesc(LPDDSURFACEDESC2 lpDDSurfaceDesc);
	virtual HRESULT GetPixelFormat(LPDDPIXELFORMAT lpDDPixelFormat);
	virtual BOOL IsLost();
	virtual HRESULT Lock(LPRECT lpDestRect, LPDDSURFACEDESC2 lpDDSurfaceDesc, DWORD dwFlags, HANDLE hEvent);
	virtual HRESULT Restore();
	virtual HRESULT SetClipper(LPDIRECTDRAWCLIPPER lpDDClipper);
	virtual HRESULT SetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey);
	virtual HRESULT SetPalette(LPDIRECTDRAWPALETTE lpDDPalette);
	virtual HRESULT Unlock(LPRECT lpRect);

	// --- IDirect3DTexture2 view of a texture surface ---
	virtual HRESULT GetHandle(void* lpDirect3DDevice, DWORD* lpHandle);
	virtual HRESULT PaletteChanged(DWORD dwStart, DWORD dwCount);
};

struct IDirectDraw : virtual public IUnknown {
	HRESULT QueryInterface(REFIID riid, void** ppvObject) override;

	virtual HRESULT CreateClipper(DWORD dwFlags, LPDIRECTDRAWCLIPPER* lplpDDClipper, IUnknown* pUnkOuter);
	virtual HRESULT CreatePalette(DWORD dwFlags, LPPALETTEENTRY lpDDColorArray, LPDIRECTDRAWPALETTE* lplpDDPalette, IUnknown* pUnkOuter);
	virtual HRESULT CreateSurface(LPDDSURFACEDESC2 lpDDSurfaceDesc, LPDIRECTDRAWSURFACE* lplpDDSurface, IUnknown* pUnkOuter);
	virtual HRESULT GetCaps(LPDDCAPS lpDDDriverCaps, LPDDCAPS lpDDHELCaps);
	virtual HRESULT GetDisplayMode(LPDDSURFACEDESC lpDDSurfaceDesc);
	virtual HRESULT RestoreDisplayMode();
	virtual HRESULT SetCooperativeLevel(HWND hWnd, DWORD dwFlags);
	virtual HRESULT SetDisplayMode(DWORD dwWidth, DWORD dwHeight, DWORD dwBPP, DWORD dwRefreshRate, DWORD dwFlags);
};
