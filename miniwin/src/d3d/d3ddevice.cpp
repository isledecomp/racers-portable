// [library:3d] IDirect3DDevice3 / IDirect3DViewport3 / IDirect3DMaterial3 emulation.
// M1: state is recorded and draws are dropped (null backend); the render backends
// (SDL3 GPU / OpenGL / OpenGL ES) attach here in M2.

#include "ddraw_impl.h"
#include "miniwin.h"

#include <miniwin/d3d.h>
#include <string.h>

void MiniwinFillDeviceDesc(D3DDEVICEDESC* p_desc); // d3d.cpp

struct MiniwinD3DDevice : public IDirect3DDevice3 {
	explicit MiniwinD3DDevice(MiniwinSurface* p_renderTarget) : m_renderTarget(p_renderTarget) {}

	HRESULT EnumTextureFormats(LPD3DENUMPIXELFORMATSCALLBACK lpd3dEnumPixelProc, LPVOID lpArg) override;
	HRESULT GetCaps(LPD3DDEVICEDESC lpD3DHWDevDesc, LPD3DDEVICEDESC lpD3DHELDevDesc) override;

	MiniwinSurface* m_renderTarget;
	DWORD m_renderStates[256] = {};
	DWORD m_textureStageStates[2][32] = {};
	MiniwinSurface* m_texture = nullptr;
	IDirect3DViewport3* m_currentViewport = nullptr;
};

IDirect3DDevice3* MiniwinCreateDevice(MiniwinSurface* p_renderTarget)
{
	return new MiniwinD3DDevice(p_renderTarget);
}

static void FillTextureFormat16(DDPIXELFORMAT* format, DWORD r, DWORD g, DWORD b, DWORD a)
{
	memset(format, 0, sizeof(*format));
	format->dwSize = sizeof(*format);
	format->dwFlags = a ? (DDPF_RGB | DDPF_ALPHAPIXELS) : DDPF_RGB;
	format->dwRGBBitCount = 16;
	format->dwRBitMask = r;
	format->dwGBitMask = g;
	format->dwBBitMask = b;
	format->dwRGBAlphaBitMask = a;
}

HRESULT MiniwinD3DDevice::EnumTextureFormats(LPD3DENUMPIXELFORMATSCALLBACK lpd3dEnumPixelProc, LPVOID lpArg)
{
	DDPIXELFORMAT format;

	// RGB565
	FillTextureFormat16(&format, 0xF800, 0x07E0, 0x001F, 0);
	if (lpd3dEnumPixelProc(&format, lpArg) == 0) {
		return D3D_OK;
	}

	// ARGB1555
	FillTextureFormat16(&format, 0x7C00, 0x03E0, 0x001F, 0x8000);
	if (lpd3dEnumPixelProc(&format, lpArg) == 0) {
		return D3D_OK;
	}

	// ARGB4444
	FillTextureFormat16(&format, 0x0F00, 0x00F0, 0x000F, 0xF000);
	if (lpd3dEnumPixelProc(&format, lpArg) == 0) {
		return D3D_OK;
	}

	// XRGB8888
	memset(&format, 0, sizeof(format));
	format.dwSize = sizeof(format);
	format.dwFlags = DDPF_RGB;
	format.dwRGBBitCount = 32;
	format.dwRBitMask = 0x00FF0000;
	format.dwGBitMask = 0x0000FF00;
	format.dwBBitMask = 0x000000FF;
	if (lpd3dEnumPixelProc(&format, lpArg) == 0) {
		return D3D_OK;
	}

	// ARGB8888
	format.dwFlags = DDPF_RGB | DDPF_ALPHAPIXELS;
	format.dwRGBAlphaBitMask = 0xFF000000;
	if (lpd3dEnumPixelProc(&format, lpArg) == 0) {
		return D3D_OK;
	}

	// P8 (palettized)
	memset(&format, 0, sizeof(format));
	format.dwSize = sizeof(format);
	format.dwFlags = DDPF_RGB | DDPF_PALETTEINDEXED8;
	format.dwRGBBitCount = 8;
	lpd3dEnumPixelProc(&format, lpArg);
	return D3D_OK;
}

HRESULT MiniwinD3DDevice::GetCaps(LPD3DDEVICEDESC lpD3DHWDevDesc, LPD3DDEVICEDESC lpD3DHELDevDesc)
{
	if (lpD3DHWDevDesc) {
		MiniwinFillDeviceDesc(lpD3DHWDevDesc);
	}
	if (lpD3DHELDevDesc) {
		memset(lpD3DHELDevDesc, 0, sizeof(*lpD3DHELDevDesc));
		lpD3DHELDevDesc->dwSize = sizeof(*lpD3DHELDevDesc);
	}
	return D3D_OK;
}

// --- IDirect3DDevice3 default bodies (recorded state; draws dropped until M2) ---

HRESULT IDirect3DDevice3::AddViewport(LPDIRECT3DVIEWPORT3 lpDirect3DViewport)
{
	if (lpDirect3DViewport) {
		lpDirect3DViewport->m_device = this;
	}
	return D3D_OK;
}

HRESULT IDirect3DDevice3::DeleteViewport(LPDIRECT3DVIEWPORT3 lpDirect3DViewport)
{
	return D3D_OK;
}

HRESULT IDirect3DDevice3::SetCurrentViewport(LPDIRECT3DVIEWPORT3 lpDirect3DViewport)
{
	MiniwinD3DDevice* self = static_cast<MiniwinD3DDevice*>(this);
	self->m_currentViewport = lpDirect3DViewport;
	return D3D_OK;
}

HRESULT IDirect3DDevice3::EnumTextureFormats(LPD3DENUMPIXELFORMATSCALLBACK, LPVOID)
{
	return D3DERR_GENERIC;
}

HRESULT IDirect3DDevice3::GetCaps(LPD3DDEVICEDESC, LPD3DDEVICEDESC)
{
	return D3DERR_GENERIC;
}

HRESULT IDirect3DDevice3::BeginScene()
{
	return D3D_OK;
}

HRESULT IDirect3DDevice3::EndScene()
{
	return D3D_OK;
}

HRESULT IDirect3DDevice3::SetRenderState(D3DRENDERSTATETYPE dwRenderStateType, DWORD dwRenderState)
{
	MiniwinD3DDevice* self = static_cast<MiniwinD3DDevice*>(this);
	if ((unsigned) dwRenderStateType < 256) {
		self->m_renderStates[dwRenderStateType] = dwRenderState;
	}
	return D3D_OK;
}

HRESULT IDirect3DDevice3::GetRenderState(D3DRENDERSTATETYPE dwRenderStateType, LPDWORD lpdwRenderState)
{
	MiniwinD3DDevice* self = static_cast<MiniwinD3DDevice*>(this);
	if (lpdwRenderState && (unsigned) dwRenderStateType < 256) {
		*lpdwRenderState = self->m_renderStates[dwRenderStateType];
	}
	return D3D_OK;
}

HRESULT IDirect3DDevice3::SetTextureStageState(DWORD dwStage, D3DTEXTURESTAGESTATETYPE dwState, DWORD dwValue)
{
	MiniwinD3DDevice* self = static_cast<MiniwinD3DDevice*>(this);
	if (dwStage < 2 && (unsigned) dwState < 32) {
		self->m_textureStageStates[dwStage][dwState] = dwValue;
	}
	return D3D_OK;
}

HRESULT IDirect3DDevice3::SetTexture(DWORD dwStage, LPDIRECT3DTEXTURE2 lpTexture)
{
	MiniwinD3DDevice* self = static_cast<MiniwinD3DDevice*>(this);
	if (dwStage == 0) {
		self->m_texture = static_cast<MiniwinSurface*>(lpTexture);
	}
	return D3D_OK;
}

HRESULT IDirect3DDevice3::SetRenderTarget(LPDIRECTDRAWSURFACE lpNewRenderTarget, DWORD dwFlags)
{
	MiniwinD3DDevice* self = static_cast<MiniwinD3DDevice*>(this);
	self->m_renderTarget = static_cast<MiniwinSurface*>(lpNewRenderTarget);
	return D3D_OK;
}

HRESULT IDirect3DDevice3::DrawPrimitive(D3DPRIMITIVETYPE, DWORD, LPVOID, DWORD, DWORD)
{
	// Null backend: draws are dropped until the render backends land (M2).
	return D3D_OK;
}

HRESULT IDirect3DDevice3::DrawIndexedPrimitive(D3DPRIMITIVETYPE, DWORD, LPVOID, DWORD, LPWORD, DWORD, DWORD)
{
	return D3D_OK;
}

// --- IDirect3DViewport3 ---

HRESULT IDirect3DViewport3::SetViewport2(LPD3DVIEWPORT2 lpData)
{
	if (lpData) {
		m_viewport = *lpData;
	}
	return D3D_OK;
}

HRESULT IDirect3DViewport3::GetViewport2(LPD3DVIEWPORT2 lpData)
{
	if (lpData) {
		*lpData = m_viewport;
	}
	return D3D_OK;
}

HRESULT IDirect3DViewport3::SetBackground(D3DMATERIALHANDLE hMat)
{
	m_background = hMat;
	return D3D_OK;
}

HRESULT IDirect3DViewport3::Clear(DWORD dwCount, LPD3DRECT lpRects, DWORD dwFlags)
{
	// Null backend: clears are dropped until the render backends land (M2).
	return D3D_OK;
}

// --- IDirect3DMaterial3 ---

HRESULT IDirect3DMaterial3::SetMaterial(LPD3DMATERIAL lpMat)
{
	if (lpMat) {
		m_material = *lpMat;
	}
	return D3D_OK;
}

HRESULT IDirect3DMaterial3::GetMaterial(LPD3DMATERIAL lpMat)
{
	if (lpMat) {
		*lpMat = m_material;
	}
	return D3D_OK;
}

HRESULT IDirect3DMaterial3::GetHandle(LPDIRECT3DDEVICE3 lpDirect3DDevice, D3DMATERIALHANDLE* lpHandle)
{
	m_handle = (D3DMATERIALHANDLE) (uintptr_t) this;
	if (lpHandle) {
		*lpHandle = m_handle;
	}
	return D3D_OK;
}
