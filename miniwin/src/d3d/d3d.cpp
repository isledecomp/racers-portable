// [library:3d] IDirect3D3 emulation: device enumeration, z-buffer formats, and object
// factories. One hardware-accelerated RGB device is advertised, backed by the miniwin
// render backends.

#include "backends.h"
#include "ddraw_impl.h"
#include "miniwin.h"

#include <miniwin/d3d.h>
#include <string.h>

struct MiniwinDirect3D : public IDirect3D3 {
	explicit MiniwinDirect3D(MiniwinDirectDraw* p_ddraw) : m_ddraw(p_ddraw) {}

	HRESULT EnumDevices(LPD3DENUMDEVICESCALLBACK lpEnumDevicesCallback, LPVOID lpUserArg) override;
	HRESULT EnumZBufferFormats(
		REFCLSID riidDevice,
		LPD3DENUMPIXELFORMATSCALLBACK lpEnumCallback,
		LPVOID lpContext
	) override;
	HRESULT CreateDevice(
		REFCLSID rclsid,
		LPDIRECTDRAWSURFACE lpDDS,
		LPDIRECT3DDEVICE3* lplpD3DDevice,
		IUnknown* pUnkOuter
	) override;
	HRESULT CreateViewport(LPDIRECT3DVIEWPORT3* lplpD3DViewport, IUnknown* pUnkOuter) override;
	HRESULT CreateMaterial(LPDIRECT3DMATERIAL3* lplpDirect3DMaterial, IUnknown* pUnkOuter) override;

	MiniwinDirectDraw* m_ddraw;
};

IDirect3D3* MiniwinCreateDirect3D(MiniwinDirectDraw* p_ddraw)
{
	return new MiniwinDirect3D(p_ddraw);
}

// Defined in d3ddevice.cpp
IDirect3DDevice3* MiniwinCreateDevice(MiniwinSurface* p_renderTarget);

void MiniwinFillDeviceDesc(D3DDEVICEDESC* p_desc)
{
	memset(p_desc, 0, sizeof(*p_desc));
	p_desc->dwSize = sizeof(*p_desc);
	p_desc->dwFlags =
		D3DDD_COLORMODEL | D3DDD_DEVCAPS | D3DDD_TRICAPS | D3DDD_DEVICERENDERBITDEPTH | D3DDD_DEVICEZBUFFERBITDEPTH;
	p_desc->dcmColorModel = D3DCOLOR_RGB;
	p_desc->dwDevCaps = D3DDEVCAPS_TEXTUREVIDEOMEMORY;
	p_desc->dwDeviceRenderBitDepth = DDBD_16 | DDBD_32;
	p_desc->dwDeviceZBufferBitDepth = DDBD_16 | DDBD_24;

	D3DPRIMCAPS& tri = p_desc->dpcTriCaps;
	tri.dwSize = sizeof(tri);
	tri.dwMiscCaps = D3DPMISCCAPS_CULLCW | D3DPMISCCAPS_CULLCCW;
	tri.dwRasterCaps = D3DPRASTERCAPS_DITHER | D3DPRASTERCAPS_SUBPIXEL;
	tri.dwSrcBlendCaps = D3DPBLENDCAPS_ZERO | D3DPBLENDCAPS_ONE | D3DPBLENDCAPS_SRCCOLOR | D3DPBLENDCAPS_INVSRCCOLOR |
						 D3DPBLENDCAPS_SRCALPHA | D3DPBLENDCAPS_INVSRCALPHA | D3DPBLENDCAPS_DESTALPHA |
						 D3DPBLENDCAPS_INVDESTALPHA | D3DPBLENDCAPS_DESTCOLOR | D3DPBLENDCAPS_INVDESTCOLOR |
						 D3DPBLENDCAPS_SRCALPHASAT;
	tri.dwDestBlendCaps = tri.dwSrcBlendCaps;
	tri.dwTextureCaps = D3DPTEXTURECAPS_PERSPECTIVE | D3DPTEXTURECAPS_ALPHA | D3DPTEXTURECAPS_TRANSPARENCY;
	tri.dwTextureFilterCaps = D3DPTFILTERCAPS_NEAREST | D3DPTFILTERCAPS_LINEAR | D3DPTFILTERCAPS_MIPNEAREST |
							  D3DPTFILTERCAPS_MIPLINEAR | D3DPTFILTERCAPS_LINEARMIPNEAREST |
							  D3DPTFILTERCAPS_LINEARMIPLINEAR;
	tri.dwTextureAddressCaps = D3DPTADDRESSCAPS_WRAP;
}

HRESULT MiniwinDirect3D::EnumDevices(LPD3DENUMDEVICESCALLBACK lpEnumDevicesCallback, LPVOID lpUserArg)
{
	D3DDEVICEDESC halDesc;
	D3DDEVICEDESC helDesc;
	MiniwinFillDeviceDesc(&halDesc);
	memset(&helDesc, 0, sizeof(helDesc));
	helDesc.dwSize = sizeof(helDesc);

	char description[] = "SDL3 hardware acceleration (miniwin)";
	char name[] = "miniwin";
	GUID guid = MINIWIN_GUID_Direct3DDevice;
	lpEnumDevicesCallback(&guid, description, name, &halDesc, &helDesc, lpUserArg);
	return D3D_OK;
}

HRESULT MiniwinDirect3D::EnumZBufferFormats(
	REFCLSID riidDevice,
	LPD3DENUMPIXELFORMATSCALLBACK lpEnumCallback,
	LPVOID lpContext
)
{
	DDPIXELFORMAT format;
	memset(&format, 0, sizeof(format));
	format.dwSize = sizeof(format);
	format.dwFlags = DDPF_ZBUFFER;
	format.dwZBufferBitDepth = 16;
	format.dwZBitMask = 0xFFFF;

	if (lpEnumCallback(&format, lpContext) == 0) {
		return D3D_OK;
	}

	memset(&format, 0, sizeof(format));
	format.dwSize = sizeof(format);
	format.dwFlags = DDPF_ZBUFFER;
	format.dwZBufferBitDepth = 24;
	format.dwZBitMask = 0xFFFFFF;
	lpEnumCallback(&format, lpContext);
	return D3D_OK;
}

HRESULT MiniwinDirect3D::CreateDevice(
	REFCLSID rclsid,
	LPDIRECTDRAWSURFACE lpDDS,
	LPDIRECT3DDEVICE3* lplpD3DDevice,
	IUnknown* pUnkOuter
)
{
	if (!lplpD3DDevice) {
		return DDERR_INVALIDPARAMS;
	}

	// CreateDevice runs only for real rendering (never during enumeration). If the game
	// selected a different backend (in-game Options -> Video, or a saved preference that
	// differs from the running process), the window's graphics binding is already fixed,
	// so switch renderers by relaunching the process. The active backend is enumerated
	// first, so the default device match never trips this.
	MiniwinBackendId requested = m_ddraw->m_requestedBackend;
	if (requested != MiniwinBackendActive() && MiniwinBackendUsable(requested)) {
		MiniwinBackendSavePref(requested);
		MiniwinBackend_Relaunch(requested);
	}

	*lplpD3DDevice = MiniwinCreateDevice(static_cast<MiniwinSurface*>(lpDDS));
	return D3D_OK;
}

HRESULT MiniwinDirect3D::CreateViewport(LPDIRECT3DVIEWPORT3* lplpD3DViewport, IUnknown* pUnkOuter)
{
	if (!lplpD3DViewport) {
		return DDERR_INVALIDPARAMS;
	}

	*lplpD3DViewport = new IDirect3DViewport3();
	return D3D_OK;
}

HRESULT MiniwinDirect3D::CreateMaterial(LPDIRECT3DMATERIAL3* lplpDirect3DMaterial, IUnknown* pUnkOuter)
{
	if (!lplpDirect3DMaterial) {
		return DDERR_INVALIDPARAMS;
	}

	*lplpDirect3DMaterial = new IDirect3DMaterial3();
	return D3D_OK;
}

// --- Base-interface default bodies ---

HRESULT IDirect3D3::EnumDevices(LPD3DENUMDEVICESCALLBACK, LPVOID)
{
	return D3DERR_GENERIC;
}
HRESULT IDirect3D3::EnumZBufferFormats(REFCLSID, LPD3DENUMPIXELFORMATSCALLBACK, LPVOID)
{
	return D3DERR_GENERIC;
}
HRESULT IDirect3D3::CreateDevice(REFCLSID, LPDIRECTDRAWSURFACE, LPDIRECT3DDEVICE3*, IUnknown*)
{
	return D3DERR_GENERIC;
}
HRESULT IDirect3D3::CreateViewport(LPDIRECT3DVIEWPORT3*, IUnknown*)
{
	return D3DERR_GENERIC;
}
HRESULT IDirect3D3::CreateMaterial(LPDIRECT3DMATERIAL3*, IUnknown*)
{
	return D3DERR_GENERIC;
}
