#pragma once
#include <windows.h>
#include <unknwn.h>
#include <vector>
#include <string>
#include "ScripaTSF.h"

// {B0A9C9D9-1D2F-4C6E-9D6D-9C3A3B2A1F00}
static const GUID CLSID_ScripaTSF =
{ 0xb0a9c9d9, 0x1d2f, 0x4c6e, { 0x9d, 0x6d, 0x9c, 0x3a, 0x3b, 0x2a, 0x1f, 0x0 } };

// IScripaTSF: minimal custom COM interface for the skeleton.
// Not a standard TSF interface — serves as a bridge for wiring during development.
struct IScripaTSF : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE Initialize() = 0;
    virtual HRESULT STDMETHODCALLTYPE Uninitialize() = 0;
    virtual HRESULT STDMETHODCALLTYPE OnKeyDown(UINT32 ch, BOOL* handled) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetComposition(BSTR* out) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetCandidates(SAFEARRAY** out) = 0;
};

// Concrete COM object
class ScripaTSFCOM : public IScripaTSF {
public:
    ScripaTSFCOM();
    virtual ~ScripaTSFCOM();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObject) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IScripaTSF
    STDMETHODIMP Initialize() override;
    STDMETHODIMP Uninitialize() override;
    STDMETHODIMP OnKeyDown(UINT32 ch, BOOL* handled) override;
    STDMETHODIMP GetComposition(BSTR* out) override;
    STDMETHODIMP GetCandidates(SAFEARRAY** out) override;

private:
    long refcount_;
    ScripaTSF backend_;
};
