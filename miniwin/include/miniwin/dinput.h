#pragma once

// [library:input] DirectInput 5 subset used by the game's input system, implemented
// over SDL3 keyboard/mouse events and SDL3 gamepads/joysticks (+ rumble for the
// GUID_Sine force-feedback effect).

#include "windows.h"

#define DI_OK ((HRESULT) 0)
#define DIERR_INPUTLOST ((HRESULT) 0x8007001E)
#define DIERR_NOTACQUIRED ((HRESULT) 0x8007000C)
#define DIERR_GENERIC E_FAIL

// Device types
#define DIDEVTYPE_MOUSE 2
#define DIDEVTYPE_KEYBOARD 3
#define DIDEVTYPE_JOYSTICK 4

// Enumeration flags
#define DIEDFL_ALLDEVICES 0x00000000
#define DIEDFL_ATTACHEDONLY 0x00000001
#define DIEDFL_FORCEFEEDBACK 0x00000100

// Cooperative level
#define DISCL_EXCLUSIVE 0x00000001
#define DISCL_NONEXCLUSIVE 0x00000002
#define DISCL_FOREGROUND 0x00000004
#define DISCL_BACKGROUND 0x00000008

// Object types (EnumObjects filters)
#define DIDFT_RELAXIS 0x00000001
#define DIDFT_ABSAXIS 0x00000002
#define DIDFT_AXIS 0x00000003
#define DIDFT_PSHBUTTON 0x00000004
#define DIDFT_TGLBUTTON 0x00000008
#define DIDFT_BUTTON 0x0000000C

// Property headers
#define DIPH_DEVICE 0
#define DIPH_BYOFFSET 1
#define DIPH_BYID 2

// The property "GUIDs" are magic pointers in DirectInput.
#define DIPROP_BUFFERSIZE ((const GUID*) 1)
#define DIPROP_AXISMODE ((const GUID*) 2)
#define DIPROP_RANGE ((const GUID*) 4)
#define DIPROP_DEADZONE ((const GUID*) 5)

// Effect flags
#define DIEFF_OBJECTIDS 0x00000001
#define DIEFF_OBJECTOFFSETS 0x00000002
#define DIEFF_CARTESIAN 0x00000010
#define DIEFF_POLAR 0x00000020
#define DIEB_NOTRIGGER 0xFFFFFFFF
#define DIEP_TYPESPECIFICPARAMS 0x00000020
#define DIES_SOLO 0x00000001

// Joystick object data offsets (DIJOYSTATE2 layout)
#define DIJOFS_X 0
#define DIJOFS_Y 4
#define DIJOFS_Z 8
#define DIJOFS_RX 12
#define DIJOFS_RY 16
#define DIJOFS_RZ 20
#define DIJOFS_SLIDER(n) (24 + (n) * 4)
#define DIJOFS_POV(n) (32 + (n) * 4)
#define DIJOFS_BUTTON(n) (48 + (n))
#define DIJOFS_BUTTON0 DIJOFS_BUTTON(0)

// Mouse object data offsets (DIMOUSESTATE layout)
#define DIMOFS_X 0
#define DIMOFS_Y 4
#define DIMOFS_Z 8
#define DIMOFS_BUTTON0 12
#define DIMOFS_BUTTON1 13
#define DIMOFS_BUTTON2 14
#define DIMOFS_BUTTON3 15

// Well-known GUIDs (storage in miniwin)
extern const GUID GUID_SysKeyboard;
extern const GUID GUID_SysMouse;
extern const GUID GUID_XAxis;
extern const GUID GUID_YAxis;
extern const GUID GUID_ZAxis;
extern const GUID GUID_RxAxis;
extern const GUID GUID_RyAxis;
extern const GUID GUID_RzAxis;
extern const GUID GUID_Slider;
extern const GUID GUID_Button;
extern const GUID GUID_POV;
extern const GUID GUID_Sine;
extern const GUID IID_IDirectInputDevice2A;

struct IDirectInputA;
struct IDirectInputDevice2A;
struct IDirectInputEffect;
typedef IDirectInputA IDirectInput;
typedef IDirectInputA* LPDIRECTINPUT;
typedef IDirectInputA* LPDIRECTINPUTA;
typedef IDirectInputDevice2A* LPDIRECTINPUTDEVICE;
typedef IDirectInputDevice2A* LPDIRECTINPUTDEVICEA;
typedef IDirectInputDevice2A* LPDIRECTINPUTDEVICE2A;
typedef IDirectInputEffect* LPDIRECTINPUTEFFECT;

// --- Structs ---
#define MAX_PATH 260

struct DIDEVICEINSTANCEA {
	DWORD dwSize;
	GUID guidInstance;
	GUID guidProduct;
	DWORD dwDevType;
	CHAR tszInstanceName[MAX_PATH];
	CHAR tszProductName[MAX_PATH];
};
typedef DIDEVICEINSTANCEA DIDEVICEINSTANCE;
typedef DIDEVICEINSTANCEA* LPDIDEVICEINSTANCEA;
typedef DIDEVICEINSTANCEA* LPDIDEVICEINSTANCE;
typedef const DIDEVICEINSTANCEA* LPCDIDEVICEINSTANCEA;
typedef const DIDEVICEINSTANCEA* LPCDIDEVICEINSTANCE;

struct DIDEVICEOBJECTINSTANCEA {
	DWORD dwSize;
	GUID guidType;
	DWORD dwOfs;
	DWORD dwType;
	DWORD dwFlags;
	CHAR tszName[MAX_PATH];
};
typedef DIDEVICEOBJECTINSTANCEA DIDEVICEOBJECTINSTANCE;
typedef const DIDEVICEOBJECTINSTANCEA* LPCDIDEVICEOBJECTINSTANCEA;
typedef const DIDEVICEOBJECTINSTANCEA* LPCDIDEVICEOBJECTINSTANCE;

struct DIDEVCAPS {
	DWORD dwSize;
	DWORD dwFlags;
	DWORD dwDevType;
	DWORD dwAxes;
	DWORD dwButtons;
	DWORD dwPOVs;
};
typedef DIDEVCAPS* LPDIDEVCAPS;

struct DIDEVICEOBJECTDATA {
	DWORD dwOfs;
	DWORD dwData;
	DWORD dwTimeStamp;
	DWORD dwSequence;
};
typedef DIDEVICEOBJECTDATA* LPDIDEVICEOBJECTDATA;
typedef const DIDEVICEOBJECTDATA* LPCDIDEVICEOBJECTDATA;

struct DIPROPHEADER {
	DWORD dwSize;
	DWORD dwHeaderSize;
	DWORD dwObj;
	DWORD dwHow;
};
typedef DIPROPHEADER* LPDIPROPHEADER;
typedef const DIPROPHEADER* LPCDIPROPHEADER;

struct DIPROPDWORD {
	DIPROPHEADER diph;
	DWORD dwData;
};

struct DIPROPRANGE {
	DIPROPHEADER diph;
	LONG lMin;
	LONG lMax;
};

struct DIDATAFORMAT {
	DWORD dwSize;
	DWORD dwObjSize;
	DWORD dwFlags;
	DWORD dwDataSize;
	DWORD dwNumObjs;
	void* rgodf;
};
typedef const DIDATAFORMAT* LPCDIDATAFORMAT;

extern const DIDATAFORMAT c_dfDIKeyboard;
extern const DIDATAFORMAT c_dfDIMouse;
extern const DIDATAFORMAT c_dfDIJoystick2;

struct DIMOUSESTATE {
	LONG lX;
	LONG lY;
	LONG lZ;
	BYTE rgbButtons[4];
};

struct DIJOYSTATE2 {
	LONG lX;
	LONG lY;
	LONG lZ;
	LONG lRx;
	LONG lRy;
	LONG lRz;
	LONG rglSlider[2];
	DWORD rgdwPOV[4];
	BYTE rgbButtons[128];
	LONG lVX;
	LONG lVY;
	LONG lVZ;
	LONG lVRx;
	LONG lVRy;
	LONG lVRz;
	LONG rglVSlider[2];
	LONG lAX;
	LONG lAY;
	LONG lAZ;
	LONG lARx;
	LONG lARy;
	LONG lARz;
	LONG rglASlider[2];
	LONG lFX;
	LONG lFY;
	LONG lFZ;
	LONG lFRx;
	LONG lFRy;
	LONG lFRz;
	LONG rglFSlider[2];
};

struct DIENVELOPE {
	DWORD dwSize;
	DWORD dwAttackLevel;
	DWORD dwAttackTime;
	DWORD dwFadeLevel;
	DWORD dwFadeTime;
};
typedef DIENVELOPE* LPDIENVELOPE;

struct DIEFFECT {
	DWORD dwSize;
	DWORD dwFlags;
	DWORD dwDuration;
	DWORD dwSamplePeriod;
	DWORD dwGain;
	DWORD dwTriggerButton;
	DWORD dwTriggerRepeatInterval;
	DWORD cAxes;
	LPDWORD rgdwAxes;
	LPLONG rglDirection;
	LPDIENVELOPE lpEnvelope;
	DWORD cbTypeSpecificParams;
	LPVOID lpvTypeSpecificParams;
};
typedef DIEFFECT* LPDIEFFECT;
typedef const DIEFFECT* LPCDIEFFECT;

struct DIPERIODIC {
	DWORD dwMagnitude;
	LONG lOffset;
	DWORD dwPhase;
	DWORD dwPeriod;
};

// --- Callbacks ---
typedef BOOL(CALLBACK* LPDIENUMDEVICESCALLBACKA)(LPCDIDEVICEINSTANCEA lpddi, LPVOID pvRef);
typedef BOOL(CALLBACK* LPDIENUMDEVICEOBJECTSCALLBACKA)(LPCDIDEVICEOBJECTINSTANCEA lpddoi, LPVOID pvRef);

// --- Interfaces ---
struct IDirectInputEffect : virtual public IUnknown {
	virtual HRESULT Start(DWORD dwIterations, DWORD dwFlags);
	virtual HRESULT Stop();
	virtual HRESULT SetParameters(LPCDIEFFECT peff, DWORD dwFlags);
};

struct IDirectInputDevice2A : virtual public IUnknown {
	virtual HRESULT Acquire();
	virtual HRESULT Unacquire();
	virtual HRESULT CreateEffect(REFGUID rguid, LPCDIEFFECT lpeff, LPDIRECTINPUTEFFECT* ppdeff, IUnknown* punkOuter);
	virtual HRESULT EnumObjects(LPDIENUMDEVICEOBJECTSCALLBACKA lpCallback, LPVOID pvRef, DWORD dwFlags);
	virtual HRESULT GetCapabilities(LPDIDEVCAPS lpDIDevCaps);
	virtual HRESULT GetDeviceData(DWORD cbObjectData, LPDIDEVICEOBJECTDATA rgdod, LPDWORD pdwInOut, DWORD dwFlags);
	virtual HRESULT GetDeviceInfo(LPDIDEVICEINSTANCEA pdidi);
	virtual HRESULT GetDeviceState(DWORD cbData, LPVOID lpvData);
	virtual HRESULT Poll();
	virtual HRESULT SetCooperativeLevel(HWND hwnd, DWORD dwFlags);
	virtual HRESULT SetDataFormat(LPCDIDATAFORMAT lpdf);
	virtual HRESULT SetProperty(const GUID* rguidProp, LPCDIPROPHEADER pdiph);
};
typedef IDirectInputDevice2A IDirectInputDeviceA;

struct IDirectInputA : virtual public IUnknown {
	virtual HRESULT CreateDevice(REFGUID rguid, LPDIRECTINPUTDEVICEA* lplpDirectInputDevice, IUnknown* pUnkOuter);
	virtual HRESULT EnumDevices(DWORD dwDevType, LPDIENUMDEVICESCALLBACKA lpCallback, LPVOID pvRef, DWORD dwFlags);
};

HRESULT DirectInputCreate(HINSTANCE hinst, DWORD dwVersion, LPDIRECTINPUT* ppDI, IUnknown* punkOuter);
#define DirectInputCreateA DirectInputCreate
