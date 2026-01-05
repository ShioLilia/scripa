#include "TextService.h"
#include <strsafe.h>

// Global instance counter
static LONG g_cRefDll = 0;
static HINSTANCE g_hInst = NULL;

// Helper function to set text in context
static HRESULT _SetText(ITfContext* pContext, TfEditCookie ec, ITfRange* pRange, const WCHAR* pchText, LONG cchText)
{
    return pRange->SetText(ec, 0, pchText, cchText);
}

//+---------------------------------------------------------------------------
//
// CTextService implementation
//
//----------------------------------------------------------------------------

CTextService::CTextService()
{
    _refCount = 1;
    _tfClientId = TF_CLIENTID_NULL;
    _pThreadMgr = NULL;
    _pContext = NULL;
    _dwThreadMgrEventSinkCookie = TF_INVALID_COOKIE;
    _pComposition = NULL;
    _pCandidateWindow = NULL;
    _pToolbar = NULL;
    
    InterlockedIncrement(&g_cRefDll);
}

CTextService::~CTextService()
{
    // Cleanup UI windows
    if (_pCandidateWindow)
    {
        _pCandidateWindow->Destroy();
        delete _pCandidateWindow;
    }
    if (_pToolbar)
    {
        _pToolbar->Destroy();
        delete _pToolbar;
    }
    
    InterlockedDecrement(&g_cRefDll);
}

//+---------------------------------------------------------------------------
//
// IUnknown implementation
//
//----------------------------------------------------------------------------

STDMETHODIMP CTextService::QueryInterface(REFIID riid, void** ppvObj)
{
    if (ppvObj == NULL)
        return E_INVALIDARG;

    *ppvObj = NULL;

    if (IsEqualIID(riid, IID_IUnknown) ||
        IsEqualIID(riid, IID_ITfTextInputProcessor))
    {
        *ppvObj = (ITfTextInputProcessor*)this;
    }
    else if (IsEqualIID(riid, IID_ITfThreadMgrEventSink))
    {
        *ppvObj = (ITfThreadMgrEventSink*)this;
    }
    else if (IsEqualIID(riid, IID_ITfKeyEventSink))
    {
        *ppvObj = (ITfKeyEventSink*)this;
    }

    if (*ppvObj)
    {
        AddRef();
        return S_OK;
    }

    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CTextService::AddRef()
{
    return InterlockedIncrement(&_refCount);
}

STDMETHODIMP_(ULONG) CTextService::Release()
{
    LONG cr = InterlockedDecrement(&_refCount);
    if (cr == 0)
    {
        delete this;
    }
    return cr;
}

//+---------------------------------------------------------------------------
//
// ITfTextInputProcessor implementation
//
//----------------------------------------------------------------------------

STDMETHODIMP CTextService::Activate(ITfThreadMgr* pThreadMgr, TfClientId tfClientId)
{
    _pThreadMgr = pThreadMgr;
    _pThreadMgr->AddRef();
    _tfClientId = tfClientId;

    // Initialize backend
    if (!_backend.Init())
    {
        return E_FAIL;
    }

    // Initialize event sinks
    if (!_InitThreadMgrEventSink())
        return E_FAIL;
    
    if (!_InitKeyEventSink())
        return E_FAIL;
    
    if (!_InitPreservedKey())
        return E_FAIL;

    // Create UI windows
    _pCandidateWindow = new CCandidateWindow(this);
    if (!_pCandidateWindow->Create())
    {
        delete _pCandidateWindow;
        _pCandidateWindow = NULL;
        return E_FAIL;
    }
    
    _pToolbar = new CToolbar(this);
    if (!_pToolbar->Create())
    {
        delete _pToolbar;
        _pToolbar = NULL;
        return E_FAIL;
    }
    
    // Initialize toolbar to IPA mode
    _pToolbar->UpdateMode(TRUE);

    return S_OK;
}

STDMETHODIMP CTextService::Deactivate()
{
    // Cleanup UI windows
    if (_pCandidateWindow)
    {
        _pCandidateWindow->Destroy();
        delete _pCandidateWindow;
        _pCandidateWindow = NULL;
    }
    if (_pToolbar)
    {
        _pToolbar->Destroy();
        delete _pToolbar;
        _pToolbar = NULL;
    }
    
    // Cleanup
    _UninitPreservedKey();
    _UninitKeyEventSink();
    _UninitThreadMgrEventSink();
    
    _backend.Uninit();

    if (_pThreadMgr)
    {
        _pThreadMgr->Release();
        _pThreadMgr = NULL;
    }

    _tfClientId = TF_CLIENTID_NULL;

    return S_OK;
}

//+---------------------------------------------------------------------------
//
// ITfThreadMgrEventSink implementation
//
//----------------------------------------------------------------------------

STDMETHODIMP CTextService::OnInitDocumentMgr(ITfDocumentMgr* pDocMgr)
{
    return S_OK;
}

STDMETHODIMP CTextService::OnUninitDocumentMgr(ITfDocumentMgr* pDocMgr)
{
    return S_OK;
}

STDMETHODIMP CTextService::OnSetFocus(ITfDocumentMgr* pDocMgrFocus, ITfDocumentMgr* pDocMgrPrevFocus)
{
    return S_OK;
}

STDMETHODIMP CTextService::OnPushContext(ITfContext* pContext)
{
    return S_OK;
}

STDMETHODIMP CTextService::OnPopContext(ITfContext* pContext)
{
    return S_OK;
}

//+---------------------------------------------------------------------------
//
// ITfKeyEventSink implementation
//
//----------------------------------------------------------------------------

STDMETHODIMP CTextService::OnSetFocus(BOOL fForeground)
{
    return S_OK;
}

STDMETHODIMP CTextService::OnTestKeyDown(ITfContext* pContext, WPARAM wParam, LPARAM lParam, BOOL* pfEaten)
{
    *pfEaten = FALSE;
    
    // Check for big keyboard number keys (not numpad) - candidate selection
    if (wParam >= '1' && wParam <= '9')
    {
        // Check if it's NOT from numpad by checking extended key flag
        BOOL isExtended = (lParam & (1 << 24)) != 0;
        if (!isExtended)  // Big keyboard number
        {
            *pfEaten = TRUE;
            return S_OK;
        }
    }
    
    // Test if we should handle this key
    if (wParam >= 'A' && wParam <= 'Z')
    {
        *pfEaten = TRUE;
    }
    else if (wParam == VK_SPACE || wParam == VK_BACK)
    {
        *pfEaten = TRUE;
    }
    // Numpad digits pass through (will be handled as normal input)
    else if (wParam >= VK_NUMPAD0 && wParam <= VK_NUMPAD9)
    {
        *pfEaten = TRUE; // We handle numpad as input
    }
    
    return S_OK;
}

STDMETHODIMP CTextService::OnKeyDown(ITfContext* pContext, WPARAM wParam, LPARAM lParam, BOOL* pfEaten)
{
    *pfEaten = FALSE;
    
    // Store context
    _pContext = pContext;

    // Check for big keyboard number keys (1-9) - candidate selection
    if (wParam >= '1' && wParam <= '9')
    {
        BOOL isExtended = (lParam & (1 << 24)) != 0;
        if (!isExtended)  // Big keyboard number
        {
            int index = wParam - '1';
            _OnCandidateSelected(index);
            *pfEaten = TRUE;
            return S_OK;
        }
    }
    
    // Handle numpad digits as regular input
    if (wParam >= VK_NUMPAD0 && wParam <= VK_NUMPAD9)
    {
        WCHAR digit = L'0' + (wParam - VK_NUMPAD0);
        bool handled = _backend.OnKeyDown(digit);
        if (handled)
        {
            if (_pComposition == NULL)
            {
                _StartComposition(pContext);
            }
            _UpdateCompositionString(pContext);
            *pfEaten = TRUE;
        }
        return S_OK;
    }

    // Handle backspace
    if (wParam == VK_BACK)
    {
        _backend.deleteLastChar();
        _UpdateCompositionString(pContext);
        *pfEaten = TRUE;
        return S_OK;
    }

    // Handle space - select first candidate
    if (wParam == VK_SPACE)
    {
        auto candidates = _backend.GetCandidates();
        if (!candidates.empty())
        {
            _OnCandidateSelected(0);
            *pfEaten = TRUE;
            return S_OK;
        }
    }

    // Convert to character
    BYTE keyState[256];
    GetKeyboardState(keyState);
    WCHAR ch[2];
    int ret = ToUnicode((UINT)wParam, (UINT)lParam, keyState, ch, 2, 0);
    
    if (ret == 1)
    {
        // Process character through backend
        bool handled = _backend.OnKeyDown(ch[0]);
        if (handled)
        {
            if (_pComposition == NULL)
            {
                _StartComposition(pContext);
            }
            _UpdateCompositionString(pContext);
            *pfEaten = TRUE;
        }
    }

    return S_OK;
}

STDMETHODIMP CTextService::OnTestKeyUp(ITfContext* pContext, WPARAM wParam, LPARAM lParam, BOOL* pfEaten)
{
    *pfEaten = FALSE;
    return S_OK;
}

STDMETHODIMP CTextService::OnKeyUp(ITfContext* pContext, WPARAM wParam, LPARAM lParam, BOOL* pfEaten)
{
    *pfEaten = FALSE;
    return S_OK;
}

STDMETHODIMP CTextService::OnPreservedKey(ITfContext* pContext, REFGUID rguid, BOOL* pfEaten)
{
    *pfEaten = FALSE;
    
    // Check if it's our Ctrl+Shift mode toggle
    if (IsEqualGUID(rguid, GUID_PreservedKey_ModeToggle))
    {
        _OnToggleMode();
        *pfEaten = TRUE;
    }
    
    return S_OK;
}

//+---------------------------------------------------------------------------
//
// Internal helper methods
//
//----------------------------------------------------------------------------

BOOL CTextService::_InitThreadMgrEventSink()
{
    ITfSource* pSource = NULL;
    HRESULT hr = _pThreadMgr->QueryInterface(IID_ITfSource, (void**)&pSource);
    
    if (SUCCEEDED(hr))
    {
        hr = pSource->AdviseSink(IID_ITfThreadMgrEventSink, 
                                 (ITfThreadMgrEventSink*)this, 
                                 &_dwThreadMgrEventSinkCookie);
        pSource->Release();
    }
    
    return SUCCEEDED(hr);
}

void CTextService::_UninitThreadMgrEventSink()
{
    if (_dwThreadMgrEventSinkCookie != TF_INVALID_COOKIE)
    {
        ITfSource* pSource = NULL;
        if (SUCCEEDED(_pThreadMgr->QueryInterface(IID_ITfSource, (void**)&pSource)))
        {
            pSource->UnadviseSink(_dwThreadMgrEventSinkCookie);
            pSource->Release();
        }
        _dwThreadMgrEventSinkCookie = TF_INVALID_COOKIE;
    }
}

BOOL CTextService::_InitKeyEventSink()
{
    ITfKeystrokeMgr* pKeystrokeMgr = NULL;
    HRESULT hr = _pThreadMgr->QueryInterface(IID_ITfKeystrokeMgr, (void**)&pKeystrokeMgr);
    
    if (SUCCEEDED(hr))
    {
        hr = pKeystrokeMgr->AdviseKeyEventSink(_tfClientId, (ITfKeyEventSink*)this, TRUE);
        pKeystrokeMgr->Release();
    }
    
    return SUCCEEDED(hr);
}

void CTextService::_UninitKeyEventSink()
{
    ITfKeystrokeMgr* pKeystrokeMgr = NULL;
    if (SUCCEEDED(_pThreadMgr->QueryInterface(IID_ITfKeystrokeMgr, (void**)&pKeystrokeMgr)))
    {
        pKeystrokeMgr->UnadviseKeyEventSink(_tfClientId);
        pKeystrokeMgr->Release();
    }
}

BOOL CTextService::_InitPreservedKey()
{
    ITfKeystrokeMgr* pKeystrokeMgr = NULL;
    HRESULT hr = _pThreadMgr->QueryInterface(IID_ITfKeystrokeMgr, (void**)&pKeystrokeMgr);
    
    if (SUCCEEDED(hr))
    {
        // Register Ctrl+Shift for mode toggle
        TF_PRESERVEDKEY prekey;
        prekey.uVKey = VK_SHIFT;
        prekey.uModifiers = TF_MOD_CONTROL;
        
        hr = pKeystrokeMgr->PreserveKey(_tfClientId, 
                                        GUID_PreservedKey_ModeToggle,
                                        &prekey,
                                        L"Toggle IPA/ENG Mode",
                                        (ULONG)wcslen(L"Toggle IPA/ENG Mode"));
        pKeystrokeMgr->Release();
    }
    
    return SUCCEEDED(hr);
}

void CTextService::_UninitPreservedKey()
{
    ITfKeystrokeMgr* pKeystrokeMgr = NULL;
    if (SUCCEEDED(_pThreadMgr->QueryInterface(IID_ITfKeystrokeMgr, (void**)&pKeystrokeMgr)))
    {
        TF_PRESERVEDKEY prekey;
        prekey.uVKey = VK_SHIFT;
        prekey.uModifiers = TF_MOD_CONTROL;
        pKeystrokeMgr->UnpreserveKey(GUID_PreservedKey_ModeToggle, &prekey);
        pKeystrokeMgr->Release();
    }
}

// Edit session callback for starting composition
class CStartCompositionEditSession : public ITfEditSession
{
public:
    CStartCompositionEditSession(CTextService* pTextService, ITfContext* pContext)
    {
        _refCount = 1;
        _pTextService = pTextService;
        _pTextService->AddRef();
        _pContext = pContext;
        _pContext->AddRef();
    }

    ~CStartCompositionEditSession()
    {
        _pTextService->Release();
        _pContext->Release();
    }

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj)
    {
        if (ppvObj == NULL) return E_INVALIDARG;
        *ppvObj = NULL;
        
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfEditSession))
        {
            *ppvObj = (ITfEditSession*)this;
        }
        
        if (*ppvObj)
        {
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) AddRef()
    {
        return InterlockedIncrement(&_refCount);
    }

    STDMETHODIMP_(ULONG) Release()
    {
        LONG cr = InterlockedDecrement(&_refCount);
        if (cr == 0) delete this;
        return cr;
    }

    // ITfEditSession
    STDMETHODIMP DoEditSession(TfEditCookie ec)
    {
        ITfInsertAtSelection* pInsertAtSelection = NULL;
        ITfRange* pRangeComposition = NULL;
        ITfContextComposition* pContextComposition = NULL;
        
        HRESULT hr = _pContext->QueryInterface(IID_ITfInsertAtSelection, (void**)&pInsertAtSelection);
        if (SUCCEEDED(hr))
        {
            hr = pInsertAtSelection->InsertTextAtSelection(ec, TF_IAS_QUERYONLY, NULL, 0, &pRangeComposition);
            pInsertAtSelection->Release();
            
            if (SUCCEEDED(hr) && pRangeComposition)
            {
                hr = _pContext->QueryInterface(IID_ITfContextComposition, (void**)&pContextComposition);
                if (SUCCEEDED(hr))
                {
                    hr = pContextComposition->StartComposition(ec, pRangeComposition, _pTextService, &_pTextService->_pComposition);
                    pContextComposition->Release();
                }
                pRangeComposition->Release();
            }
        }
        
        return hr;
    }

private:
    LONG _refCount;
    CTextService* _pTextService;
    ITfContext* _pContext;
};

void CTextService::_StartComposition(ITfContext* pContext)
{
    CStartCompositionEditSession* pEditSession = new CStartCompositionEditSession(this, pContext);
    if (pEditSession)
    {
        HRESULT hr;
        pContext->RequestEditSession(_tfClientId, pEditSession, TF_ES_SYNC | TF_ES_READWRITE, &hr);
        pEditSession->Release();
    }
}

// Edit session for inserting selected candidate text
class CInsertTextEditSession : public ITfEditSession
{
public:
    CInsertTextEditSession(CTextService* pTextService, ITfContext* pContext, const std::wstring& text)
        : _text(text)
    {
        _refCount = 1;
        _pTextService = pTextService;
        _pTextService->AddRef();
        _pContext = pContext;
        _pContext->AddRef();
    }

    ~CInsertTextEditSession()
    {
        _pTextService->Release();
        _pContext->Release();
    }

    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj)
    {
        if (ppvObj == NULL) return E_INVALIDARG;
        *ppvObj = NULL;
        
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfEditSession))
        {
            *ppvObj = (ITfEditSession*)this;
        }
        
        if (*ppvObj)
        {
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) AddRef()
    {
        return InterlockedIncrement(&_refCount);
    }

    STDMETHODIMP_(ULONG) Release()
    {
        LONG cr = InterlockedDecrement(&_refCount);
        if (cr == 0) delete this;
        return cr;
    }

    STDMETHODIMP DoEditSession(TfEditCookie ec)
    {
        if (_pTextService->_pComposition)
        {
            ITfRange* pRange = NULL;
            if (SUCCEEDED(_pTextService->_pComposition->GetRange(&pRange)))
            {
                pRange->SetText(ec, 0, _text.c_str(), (LONG)_text.length());
                pRange->Release();
            }
        }
        return S_OK;
    }

private:
    LONG _refCount;
    CTextService* _pTextService;
    ITfContext* _pContext;
    std::wstring _text;
};

// Edit session for ending composition
class CEndCompositionEditSession : public ITfEditSession
{
public:
    CEndCompositionEditSession(CTextService* pTextService, ITfContext* pContext)
    {
        _refCount = 1;
        _pTextService = pTextService;
        _pTextService->AddRef();
        _pContext = pContext;
        _pContext->AddRef();
    }

    ~CEndCompositionEditSession()
    {
        _pTextService->Release();
        _pContext->Release();
    }

    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj)
    {
        if (ppvObj == NULL) return E_INVALIDARG;
        *ppvObj = NULL;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfEditSession))
            *ppvObj = (ITfEditSession*)this;
        if (*ppvObj) { AddRef(); return S_OK; }
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) AddRef() { return InterlockedIncrement(&_refCount); }
    STDMETHODIMP_(ULONG) Release()
    {
        LONG cr = InterlockedDecrement(&_refCount);
        if (cr == 0) delete this;
        return cr;
    }

    STDMETHODIMP DoEditSession(TfEditCookie ec)
    {
        if (_pTextService->_pComposition)
        {
            _pTextService->_pComposition->EndComposition(ec);
            _pTextService->_pComposition->Release();
            _pTextService->_pComposition = NULL;
        }
        return S_OK;
    }

private:
    LONG _refCount;
    CTextService* _pTextService;
    ITfContext* _pContext;
};

void CTextService::_EndComposition(ITfContext* pContext)
{
    if (_pComposition)
    {
        CEndCompositionEditSession* pEditSession = new CEndCompositionEditSession(this, pContext);
        if (pEditSession)
        {
            HRESULT hr;
            pContext->RequestEditSession(_tfClientId, pEditSession, TF_ES_SYNC | TF_ES_READWRITE, &hr);
            pEditSession->Release();
        }
        
        _backend.clearBuffer();
    }
}

// Edit session for updating composition
class CUpdateCompositionEditSession : public ITfEditSession
{
public:
    CUpdateCompositionEditSession(CTextService* pTextService, ITfContext* pContext)
    {
        _refCount = 1;
        _pTextService = pTextService;
        _pTextService->AddRef();
        _pContext = pContext;
        _pContext->AddRef();
    }

    ~CUpdateCompositionEditSession()
    {
        _pTextService->Release();
        _pContext->Release();
    }

    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj)
    {
        if (ppvObj == NULL) return E_INVALIDARG;
        *ppvObj = NULL;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfEditSession))
            *ppvObj = (ITfEditSession*)this;
        if (*ppvObj) { AddRef(); return S_OK; }
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) AddRef() { return InterlockedIncrement(&_refCount); }
    STDMETHODIMP_(ULONG) Release()
    {
        LONG cr = InterlockedDecrement(&_refCount);
        if (cr == 0) delete this;
        return cr;
    }

    STDMETHODIMP DoEditSession(TfEditCookie ec)
    {
        ITfRange* pRangeComposition = NULL;
        if (_pTextService->_pComposition)
        {
            HRESULT hr = _pTextService->_pComposition->GetRange(&pRangeComposition);
            if (SUCCEEDED(hr))
            {
                std::wstring comp = _pTextService->_backend.GetComposition();
                _SetText(_pContext, ec, pRangeComposition, comp.c_str(), (LONG)comp.length());
                pRangeComposition->Release();
            }
        }
        return S_OK;
    }

private:
    LONG _refCount;
    CTextService* _pTextService;
    ITfContext* _pContext;
};

void CTextService::_UpdateCompositionString(ITfContext* pContext)
{
    CUpdateCompositionEditSession* pEditSession = new CUpdateCompositionEditSession(this, pContext);
    if (pEditSession)
    {
        HRESULT hr;
        pContext->RequestEditSession(_tfClientId, pEditSession, TF_ES_SYNC | TF_ES_READWRITE, &hr);
        pEditSession->Release();
    }
    
    // Update UI windows
    _UpdateCandidateWindow();
    _PositionWindows();
}

void CTextService::_UpdateCandidateWindow()
{
    if (!_pCandidateWindow)
        return;
    
    std::wstring buffer = _backend.GetBuffer();
    auto candidates = _backend.GetCandidates();
    
    if (buffer.empty())
    {
        _pCandidateWindow->Show(FALSE);
        return;
    }
    
    _pCandidateWindow->Update(candidates, 0);
    _pCandidateWindow->Show(TRUE);
}

void CTextService::_UpdateToolbar()
{
    if (!_pToolbar)
        return;
    
    // Update mode indicator
    _pToolbar->UpdateMode(_backend.IsIPAMode());
}

void CTextService::_PositionWindows()
{
    if (!_pCandidateWindow && !_pToolbar)
        return;
    
    // Get caret position
    POINT pt = {100, 100}; // Default position
    
    // Try to get actual caret position from context
    if (_pContext)
    {
        ITfContextView* pContextView = NULL;
        if (SUCCEEDED(_pContext->GetActiveView(&pContextView)))
        {
            if (_pComposition)
            {
                ITfRange* pRange = NULL;
                if (SUCCEEDED(_pComposition->GetRange(&pRange)))
                {
                    RECT rc;
                    BOOL fClipped;
                    if (SUCCEEDED(pContextView->GetTextExt(0, pRange, &rc, &fClipped)))
                    {
                        pt.x = rc.left;
                        pt.y = rc.bottom;
                    }
                    pRange->Release();
                }
            }
            pContextView->Release();
        }
    }
    
    // Position candidate window at caret
    if (_pCandidateWindow)
    {
        _pCandidateWindow->Move(pt.x, pt.y);
    }
    
    // Position toolbar at top-right of screen
    if (_pToolbar)
    {
        int screenWidth = GetSystemMetrics(SM_CXSCREEN);
        _pToolbar->Move(screenWidth - 160, 10);
        _pToolbar->Show(TRUE);
    }
}

void CTextService::_OnCandidateSelected(int index)
{
    if (!_pContext)
        return;
    
    auto candidates = _backend.GetCandidates();
    if (index < 0 || index >= (int)candidates.size())
        return;
    
    // Get the selected candidate text
    std::wstring selectedText = candidates[index];
    
    // End composition with the selected text
    if (_pComposition)
    {
        ITfRange* pRange = NULL;
        if (SUCCEEDED(_pComposition->GetRange(&pRange)))
        {
            // Request edit session to insert selected text
            HRESULT hr;
            _pContext->RequestEditSession(_tfClientId, 
                new CInsertTextEditSession(this, _pContext, selectedText), 
                TF_ES_SYNC | TF_ES_READWRITE, &hr);
            
            pRange->Release();
        }
        
        _EndComposition(_pContext);
    }
    
    // Clear backend buffer
    _backend.clearBuffer();
    
    // Hide candidate window
    if (_pCandidateWindow)
        _pCandidateWindow->Show(FALSE);
}

void CTextService::_OnToggleMode()
{
    _backend.ToggleMode();
    _UpdateToolbar();
}

//+---------------------------------------------------------------------------
//
// CTextServiceFactory implementation
//
//----------------------------------------------------------------------------

CTextServiceFactory::CTextServiceFactory()
{
    _refCount = 1;
}

CTextServiceFactory::~CTextServiceFactory()
{
}

STDMETHODIMP CTextServiceFactory::QueryInterface(REFIID riid, void** ppvObj)
{
    if (ppvObj == NULL) return E_INVALIDARG;
    *ppvObj = NULL;
    
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory))
    {
        *ppvObj = (IClassFactory*)this;
    }
    
    if (*ppvObj)
    {
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) CTextServiceFactory::AddRef()
{
    return InterlockedIncrement(&_refCount);
}

STDMETHODIMP_(ULONG) CTextServiceFactory::Release()
{
    LONG cr = InterlockedDecrement(&_refCount);
    if (cr == 0) delete this;
    return cr;
}

STDMETHODIMP CTextServiceFactory::CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObj)
{
    if (pUnkOuter != NULL)
        return CLASS_E_NOAGGREGATION;

    CTextService* pTextService = new CTextService();
    if (pTextService == NULL)
        return E_OUTOFMEMORY;

    HRESULT hr = pTextService->QueryInterface(riid, ppvObj);
    pTextService->Release();
    
    return hr;
}

STDMETHODIMP CTextServiceFactory::LockServer(BOOL fLock)
{
    if (fLock)
        InterlockedIncrement(&g_cRefDll);
    else
        InterlockedDecrement(&g_cRefDll);
    
    return S_OK;
}

//+---------------------------------------------------------------------------
//
// DLL exports
//
//----------------------------------------------------------------------------

void SetGlobalInstance(HINSTANCE hInst)
{
    g_hInst = hInst;
}

LONG GetDllRefCount()
{
    return g_cRefDll;
}
