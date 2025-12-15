#include "ScripaTSFCOM.h"
#include <comutil.h>
#include <iostream>

ScripaTSFCOM::ScripaTSFCOM()
    : refcount_(1)
{
}

ScripaTSFCOM::~ScripaTSFCOM()
{
}

STDMETHODIMP ScripaTSFCOM::QueryInterface(REFIID riid, void** ppvObject)
{
    if (!ppvObject) return E_POINTER;
    if (riid == IID_IUnknown) {
        *ppvObject = static_cast<IUnknown*>(this);
    } else {
        *ppvObject = nullptr;
        return E_NOINTERFACE;
    }
    AddRef();
    return S_OK;
}

STDMETHODIMP_(ULONG) ScripaTSFCOM::AddRef()
{
    return InterlockedIncrement(&refcount_);
}

STDMETHODIMP_(ULONG) ScripaTSFCOM::Release()
{
    long c = InterlockedDecrement(&refcount_);
    if (c == 0) delete this;
    return c;
}

STDMETHODIMP ScripaTSFCOM::Initialize()
{
    if (!backend_.Init()) return E_FAIL;
    return S_OK;
}

STDMETHODIMP ScripaTSFCOM::Uninitialize()
{
    backend_.Uninit();
    return S_OK;
}

STDMETHODIMP ScripaTSFCOM::OnKeyDown(UINT32 ch, BOOL* handled)
{
    if (!handled) return E_POINTER;
    *handled = backend_.OnKeyDown(static_cast<wchar_t>(ch)) ? TRUE : FALSE;
    return S_OK;
}

STDMETHODIMP ScripaTSFCOM::GetComposition(BSTR* out)
{
    if (!out) return E_POINTER;
    std::wstring comp = backend_.GetComposition();
    *out = SysAllocStringLen(comp.data(), (UINT)comp.size());
    return S_OK;
}

STDMETHODIMP ScripaTSFCOM::GetCandidates(SAFEARRAY** out)
{
    if (!out) return E_POINTER;
    auto cands = backend_.GetCandidates();
    // Build a SAFEARRAY of BSTR
    SAFEARRAYBOUND b[1];
    b[0].lLbound = 0;
    b[0].cElements = (ULONG)cands.size();
    SAFEARRAY* psa = SafeArrayCreate(VT_BSTR, 1, b);
    if (!psa) return E_OUTOFMEMORY;

    LONG idx = 0;
    for (auto &s : cands) {
        BSTR bstr = SysAllocStringLen(s.data(), (UINT)s.size());
        SafeArrayPutElement(psa, &idx, bstr);
        SysFreeString(bstr);
        ++idx;
    }

    *out = psa;
    return S_OK;
}
