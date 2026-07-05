// [library:input] DirectInput 5 emulation. M1: functional-empty devices so the input
// manager initializes; SDL3 event wiring lands in M4.

#include "miniwin.h"

#include <miniwin/dinput.h>
#include <string.h>

struct MiniwinInputDevice : public IDirectInputDevice2A {
	enum class Kind {
		Keyboard,
		Mouse,
		Joystick,
	};

	explicit MiniwinInputDevice(Kind p_kind) : m_kind(p_kind) {}

	HRESULT GetDeviceInfo(LPDIDEVICEINSTANCEA pdidi) override
	{
		if (!pdidi) {
			return DIERR_GENERIC;
		}

		DWORD size = pdidi->dwSize;
		memset(pdidi, 0, sizeof(DIDEVICEINSTANCEA));
		pdidi->dwSize = size;

		switch (m_kind) {
		case Kind::Keyboard:
			pdidi->dwDevType = DIDEVTYPE_KEYBOARD;
			strcpy(pdidi->tszProductName, "Keyboard");
			strcpy(pdidi->tszInstanceName, "Keyboard");
			pdidi->guidInstance = GUID_SysKeyboard;
			break;
		case Kind::Mouse:
			pdidi->dwDevType = DIDEVTYPE_MOUSE;
			strcpy(pdidi->tszProductName, "Mouse");
			strcpy(pdidi->tszInstanceName, "Mouse");
			pdidi->guidInstance = GUID_SysMouse;
			break;
		case Kind::Joystick:
			pdidi->dwDevType = DIDEVTYPE_JOYSTICK;
			strcpy(pdidi->tszProductName, "Gamepad");
			strcpy(pdidi->tszInstanceName, "Gamepad");
			break;
		}

		return DI_OK;
	}

	HRESULT GetCapabilities(LPDIDEVCAPS lpDIDevCaps) override
	{
		if (!lpDIDevCaps) {
			return DIERR_GENERIC;
		}

		DWORD size = lpDIDevCaps->dwSize;
		memset(lpDIDevCaps, 0, sizeof(DIDEVCAPS));
		lpDIDevCaps->dwSize = size;

		switch (m_kind) {
		case Kind::Keyboard:
			lpDIDevCaps->dwButtons = 256;
			break;
		case Kind::Mouse:
			lpDIDevCaps->dwAxes = 3;
			lpDIDevCaps->dwButtons = 4;
			break;
		case Kind::Joystick:
			lpDIDevCaps->dwAxes = 4;
			lpDIDevCaps->dwButtons = 16;
			break;
		}

		return DI_OK;
	}

	Kind m_kind;
	bool m_acquired = false;
};

struct MiniwinDirectInput : public IDirectInputA {
	HRESULT CreateDevice(REFGUID rguid, LPDIRECTINPUTDEVICEA* lplpDirectInputDevice, IUnknown* pUnkOuter) override
	{
		if (!lplpDirectInputDevice) {
			return DIERR_GENERIC;
		}

		MiniwinInputDevice::Kind kind;
		if (rguid == GUID_SysKeyboard) {
			kind = MiniwinInputDevice::Kind::Keyboard;
		}
		else if (rguid == GUID_SysMouse) {
			kind = MiniwinInputDevice::Kind::Mouse;
		}
		else {
			kind = MiniwinInputDevice::Kind::Joystick;
		}

		*lplpDirectInputDevice = new MiniwinInputDevice(kind);
		return DI_OK;
	}

	HRESULT EnumDevices(DWORD dwDevType, LPDIENUMDEVICESCALLBACKA lpCallback, LPVOID pvRef, DWORD dwFlags) override
	{
		// No joysticks yet; SDL3 gamepad wiring lands in M4.
		return DI_OK;
	}
};

HRESULT DirectInputCreate(HINSTANCE hinst, DWORD dwVersion, LPDIRECTINPUT* ppDI, IUnknown* punkOuter)
{
	if (!ppDI) {
		return DIERR_GENERIC;
	}

	*ppDI = new MiniwinDirectInput();
	return DI_OK;
}

// --- Base-interface default bodies ---

HRESULT IDirectInputA::CreateDevice(REFGUID, LPDIRECTINPUTDEVICEA*, IUnknown*)
{
	return DIERR_GENERIC;
}
HRESULT IDirectInputA::EnumDevices(DWORD, LPDIENUMDEVICESCALLBACKA, LPVOID, DWORD)
{
	return DI_OK;
}

HRESULT IDirectInputDevice2A::Acquire()
{
	return DI_OK;
}
HRESULT IDirectInputDevice2A::Unacquire()
{
	return DI_OK;
}
HRESULT IDirectInputDevice2A::CreateEffect(REFGUID, LPCDIEFFECT, LPDIRECTINPUTEFFECT* ppdeff, IUnknown*)
{
	// Force feedback lands in M4 (SDL rumble); report failure so the game disables FF.
	if (ppdeff) {
		*ppdeff = nullptr;
	}
	return DIERR_GENERIC;
}
HRESULT IDirectInputDevice2A::EnumObjects(LPDIENUMDEVICEOBJECTSCALLBACKA, LPVOID, DWORD)
{
	return DI_OK;
}
HRESULT IDirectInputDevice2A::GetCapabilities(LPDIDEVCAPS)
{
	return DIERR_GENERIC;
}
HRESULT IDirectInputDevice2A::GetDeviceData(DWORD, LPDIDEVICEOBJECTDATA, LPDWORD pdwInOut, DWORD)
{
	if (pdwInOut) {
		*pdwInOut = 0;
	}
	return DI_OK;
}
HRESULT IDirectInputDevice2A::GetDeviceInfo(LPDIDEVICEINSTANCEA)
{
	return DIERR_GENERIC;
}
HRESULT IDirectInputDevice2A::GetDeviceState(DWORD cbData, LPVOID lpvData)
{
	if (lpvData) {
		memset(lpvData, 0, cbData);
	}
	return DI_OK;
}
HRESULT IDirectInputDevice2A::Poll()
{
	return DI_OK;
}
HRESULT IDirectInputDevice2A::SetCooperativeLevel(HWND, DWORD)
{
	return DI_OK;
}
HRESULT IDirectInputDevice2A::SetDataFormat(LPCDIDATAFORMAT)
{
	return DI_OK;
}
HRESULT IDirectInputDevice2A::SetProperty(const GUID*, LPCDIPROPHEADER)
{
	return DI_OK;
}

HRESULT IDirectInputEffect::Start(DWORD, DWORD)
{
	return DI_OK;
}
HRESULT IDirectInputEffect::Stop()
{
	return DI_OK;
}
HRESULT IDirectInputEffect::SetParameters(LPCDIEFFECT, DWORD)
{
	return DI_OK;
}
