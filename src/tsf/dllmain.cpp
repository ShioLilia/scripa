#include <windows.h>
#include <initguid.h>
#include <objbase.h>
#include <string>

// Minimal DLL entry points for a COM DLL. This file provides stubs for
// DllMain, DllRegisterServer and DllUnregisterServer. It does not implement
// a full COM class factory for production use; it's a starting point for
// registering a COM object that will be used by the TSF glue code.

HINSTANCE g_hInst = NULL;

BOOL APIENTRY DllMain(HINSTANCE hInstance, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        g_hInst = hInstance;
        break;
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

// Note: proper DllRegisterServer implementation should register CLSIDs,
// ProgIDs and any additional keys required by TSF. For now we provide a
// simple stub that returns E_NOTIMPL to indicate manual registration is
// required until a full COM implementation is written.

extern "C" HRESULT __stdcall DllRegisterServer()
{
    // TODO: write registry entries for CLSID and ProgID
    return E_NOTIMPL;
}

extern "C" HRESULT __stdcall DllUnregisterServer()
{
    // TODO: remove registry entries created by DllRegisterServer
    return E_NOTIMPL;
}
