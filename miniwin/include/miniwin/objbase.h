#pragma once

// [library:window] COM initialization stubs; the port does not use COM activation.

#include "windows.h"

#define CLSCTX_INPROC_SERVER 0x1

HRESULT CoInitialize(LPVOID pvReserved);
void CoUninitialize(void);
HRESULT CoCreateInstance(REFCLSID rclsid, LPUNKNOWN pUnkOuter, DWORD dwClsContext, REFIID riid, LPVOID* ppv);
