#include <windows.h>
#include <initguid.h>
#include <objbase.h>
#include <msctf.h>
#include <string>
#include <strsafe.h>
#include "TextService.h"

extern void SetGlobalInstance(HINSTANCE hInst);
extern LONG GetDllRefCount();

static HINSTANCE g_hInst = NULL;
static CTextServiceFactory* g_pFactory = NULL;

// Text service description
static const WCHAR c_szServiceDesc[] = L"Scripa IPA Input Method";
static const WCHAR c_szProfileDesc[] = L"Scripa IPA";
static const LANGID c_langid = MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US);

BOOL APIENTRY DllMain(HINSTANCE hInstance, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        g_hInst = hInstance;
        SetGlobalInstance(hInstance);
        DisableThreadLibraryCalls(hInstance);
        break;
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

// Helper function to write registry key
static HRESULT RegisterKey(HKEY hKeyRoot, const WCHAR* pszKey, const WCHAR* pszValueName, const WCHAR* pszValue)
{
    HKEY hKey;
    LONG result = RegCreateKeyExW(hKeyRoot, pszKey, 0, NULL, REG_OPTION_NON_VOLATILE,
                                   KEY_WRITE, NULL, &hKey, NULL);
    if (result != ERROR_SUCCESS)
        return E_FAIL;

    if (pszValueName && pszValue)
    {
        result = RegSetValueExW(hKey, pszValueName, 0, REG_SZ,
                                (BYTE*)pszValue, (DWORD)((wcslen(pszValue) + 1) * sizeof(WCHAR)));
    }

    RegCloseKey(hKey);
    return (result == ERROR_SUCCESS) ? S_OK : E_FAIL;
}

// Helper function to delete registry key
static void UnregisterKey(HKEY hKeyRoot, const WCHAR* pszKey)
{
    RegDeleteTreeW(hKeyRoot, pszKey);
}

extern "C" HRESULT __stdcall DllRegisterServer()
{
    WCHAR szModulePath[MAX_PATH];
    WCHAR szCLSID[128];
    WCHAR szProfileGUID[128];
    WCHAR szKey[MAX_PATH];

    // Get DLL path
    if (GetModuleFileNameW(g_hInst, szModulePath, ARRAYSIZE(szModulePath)) == 0)
        return E_FAIL;

    // Convert GUIDs to strings
    StringFromGUID2(CLSID_ScripaTextService, szCLSID, ARRAYSIZE(szCLSID));
    StringFromGUID2(GUID_ScripaProfile, szProfileGUID, ARRAYSIZE(szProfileGUID));

    HRESULT hr;

    // Register CLSID
    StringCchPrintfW(szKey, ARRAYSIZE(szKey), L"CLSID\\%s", szCLSID);
    hr = RegisterKey(HKEY_CLASSES_ROOT, szKey, NULL, c_szServiceDesc);
    if (FAILED(hr)) return hr;

    // Register InprocServer32
    StringCchPrintfW(szKey, ARRAYSIZE(szKey), L"CLSID\\%s\\InprocServer32", szCLSID);
    hr = RegisterKey(HKEY_CLASSES_ROOT, szKey, NULL, szModulePath);
    if (FAILED(hr)) return hr;

    hr = RegisterKey(HKEY_CLASSES_ROOT, szKey, L"ThreadingModel", L"Apartment");
    if (FAILED(hr)) return hr;

    // Register as TSF Text Service
    StringCchPrintfW(szKey, ARRAYSIZE(szKey), 
                     L"SOFTWARE\\Microsoft\\CTF\\TIP\\%s", szCLSID);
    hr = RegisterKey(HKEY_LOCAL_MACHINE, szKey, L"Description", c_szServiceDesc);
    if (FAILED(hr)) return hr;

    // Register language profile
    StringCchPrintfW(szKey, ARRAYSIZE(szKey),
                     L"SOFTWARE\\Microsoft\\CTF\\TIP\\%s\\LanguageProfile\\0x%08x\\%s",
                     szCLSID, c_langid, szProfileGUID);
    hr = RegisterKey(HKEY_LOCAL_MACHINE, szKey, L"Description", c_szProfileDesc);
    if (FAILED(hr)) return hr;

    hr = RegisterKey(HKEY_LOCAL_MACHINE, szKey, L"IconFile", szModulePath);
    if (FAILED(hr)) return hr;

    hr = RegisterKey(HKEY_LOCAL_MACHINE, szKey, L"IconIndex", L"0");
    if (FAILED(hr)) return hr;

    // Register with ITfInputProcessorProfiles
    ITfInputProcessorProfiles* pProfiles = NULL;
    hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, NULL, CLSCTX_INPROC_SERVER,
                         IID_ITfInputProcessorProfiles, (void**)&pProfiles);
    if (SUCCEEDED(hr))
    {
        hr = pProfiles->Register(CLSID_ScripaTextService);
        if (SUCCEEDED(hr))
        {
            hr = pProfiles->AddLanguageProfile(CLSID_ScripaTextService,
                                              c_langid,
                                              GUID_ScripaProfile,
                                              c_szProfileDesc,
                                              (ULONG)wcslen(c_szProfileDesc),
                                              szModulePath,
                                              (ULONG)wcslen(szModulePath),
                                              0);
        }
        pProfiles->Release();
    }

    return hr;
}

extern "C" HRESULT __stdcall DllUnregisterServer()
{
    WCHAR szCLSID[128];
    WCHAR szProfileGUID[128];
    WCHAR szKey[MAX_PATH];

    StringFromGUID2(CLSID_ScripaTextService, szCLSID, ARRAYSIZE(szCLSID));
    StringFromGUID2(GUID_ScripaProfile, szProfileGUID, ARRAYSIZE(szProfileGUID));

    // Unregister from TSF
    ITfInputProcessorProfiles* pProfiles = NULL;
    HRESULT hr = CoCreateInstance(CLSID_TF_InputProcessorProfiles, NULL, CLSCTX_INPROC_SERVER,
                                  IID_ITfInputProcessorProfiles, (void**)&pProfiles);
    if (SUCCEEDED(hr))
    {
        pProfiles->Unregister(CLSID_ScripaTextService);
        pProfiles->Release();
    }

    // Remove registry keys
    StringCchPrintfW(szKey, ARRAYSIZE(szKey), L"CLSID\\%s", szCLSID);
    UnregisterKey(HKEY_CLASSES_ROOT, szKey);

    StringCchPrintfW(szKey, ARRAYSIZE(szKey),
                     L"SOFTWARE\\Microsoft\\CTF\\TIP\\%s", szCLSID);
    UnregisterKey(HKEY_LOCAL_MACHINE, szKey);

    return S_OK;
}

extern "C" HRESULT __stdcall DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppvObj)
{
    if (ppvObj == NULL)
        return E_INVALIDARG;

    *ppvObj = NULL;

    if (!IsEqualIID(rclsid, CLSID_ScripaTextService))
        return CLASS_E_CLASSNOTAVAILABLE;

    if (g_pFactory == NULL)
    {
        g_pFactory = new CTextServiceFactory();
        if (g_pFactory == NULL)
            return E_OUTOFMEMORY;
    }

    return g_pFactory->QueryInterface(riid, ppvObj);
}

extern "C" HRESULT __stdcall DllCanUnloadNow()
{
    return (GetDllRefCount() == 0) ? S_OK : S_FALSE;
}
