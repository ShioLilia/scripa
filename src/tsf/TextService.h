#pragma once

#include <windows.h>
#include <msctf.h>
#include <combaseapi.h>
#include "ScripaTSF.h"
#include "CandidateWindow.h"

// CLSID and GUID definitions
// {8BC7F7A1-9876-4E3D-A1B2-3C4D5E6F7A8B}
static const GUID CLSID_ScripaTextService = 
{ 0x8bc7f7a1, 0x9876, 0x4e3d, { 0xa1, 0xb2, 0x3c, 0x4d, 0x5e, 0x6f, 0x7a, 0x8b } };

// {9CD8E8B2-A987-4F4E-B2C3-4D5E6F7A8B9C}
static const GUID GUID_ScripaProfile =
{ 0x9cd8e8b2, 0xa987, 0x4f4e, { 0xb2, 0xc3, 0x4d, 0x5e, 0x6f, 0x7a, 0x8b, 0x9c } };

// {1E2F3A4B-5C6D-7E8F-9A0B-1C2D3E4F5A6B} - Ctrl+Shift for mode toggle
static const GUID GUID_PreservedKey_ModeToggle =
{ 0x1e2f3a4b, 0x5c6d, 0x7e8f, { 0x9a, 0x0b, 0x1c, 0x2d, 0x3e, 0x4f, 0x5a, 0x6b } };

class CTextService : public ITfTextInputProcessor,
                     public ITfThreadMgrEventSink,
                     public ITfCompositionSink,
                     public ITfKeyEventSink
{
public:
    CTextService();
    ~CTextService();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    // ITfTextInputProcessor
    STDMETHODIMP Activate(ITfThreadMgr* pThreadMgr, TfClientId tfClientId);
    STDMETHODIMP Deactivate();

    // ITfThreadMgrEventSink
    STDMETHODIMP OnInitDocumentMgr(ITfDocumentMgr* pDocMgr);
    STDMETHODIMP OnUninitDocumentMgr(ITfDocumentMgr* pDocMgr);
    STDMETHODIMP OnSetFocus(ITfDocumentMgr* pDocMgrFocus, ITfDocumentMgr* pDocMgrPrevFocus);
    STDMETHODIMP OnPushContext(ITfContext* pContext);
    STDMETHODIMP OnPopContext(ITfContext* pContext);

    // ITfCompositionSink
    STDMETHODIMP OnCompositionTerminated(TfEditCookie ecWrite,ITfComposition *pComposition);

    // ITfKeyEventSink
    STDMETHODIMP OnSetFocus(BOOL fForeground);
    STDMETHODIMP OnTestKeyDown(ITfContext* pContext, WPARAM wParam, LPARAM lParam, BOOL* pfEaten);
    STDMETHODIMP OnKeyDown(ITfContext* pContext, WPARAM wParam, LPARAM lParam, BOOL* pfEaten);
    STDMETHODIMP OnTestKeyUp(ITfContext* pContext, WPARAM wParam, LPARAM lParam, BOOL* pfEaten);
    STDMETHODIMP OnKeyUp(ITfContext* pContext, WPARAM wParam, LPARAM lParam, BOOL* pfEaten);
    STDMETHODIMP OnPreservedKey(ITfContext* pContext, REFGUID rguid, BOOL* pfEaten);

    // Internal methods
    BOOL _InitThreadMgrEventSink();
    void _UninitThreadMgrEventSink();
    BOOL _InitKeyEventSink();
    void _UninitKeyEventSink();
    BOOL _InitPreservedKey();
    void _UninitPreservedKey();
    void _StartComposition(ITfContext* pContext);
    void _EndComposition(ITfContext* pContext);
    void _UpdateCompositionString(ITfContext* pContext);
    void _UpdateCandidateWindow();
    void _UpdateToolbar();
    void _PositionWindows();
    
    // UI callbacks (called by CandidateWindow and CToolbar)
    void _OnCandidateSelected(int index);
    void _OnToggleMode();

    TfClientId _tfClientId;
    ITfThreadMgr* _pThreadMgr;
    ITfContext* _pContext;
    DWORD _dwThreadMgrEventSinkCookie;
    ITfComposition* _pComposition;
    
    // Backend engine
    ScripaTSF _backend;
    
    // UI windows
    CCandidateWindow* _pCandidateWindow;
    CToolbar* _pToolbar;

    friend class CCandidateWindow;
    friend class CToolbar;
private:
    LONG _refCount;   

};

// Class factory
class CTextServiceFactory : public IClassFactory
{
public:
    CTextServiceFactory();
    ~CTextServiceFactory();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj);
    STDMETHODIMP_(ULONG) AddRef();
    STDMETHODIMP_(ULONG) Release();

    // IClassFactory
    STDMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObj);
    STDMETHODIMP LockServer(BOOL fLock);

private:
    LONG _refCount;
};
