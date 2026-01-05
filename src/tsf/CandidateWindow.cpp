#include "CandidateWindow.h"
#include "TextService.h"
#include <strsafe.h>
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif
//maxmin宏污染

using namespace Gdiplus;

static ULONG_PTR g_gdiplusToken = 0;
static ATOM g_atomCandidateWindow = 0;
static ATOM g_atomToolbar = 0;

// Initialize GDI+
void InitializeGdiplus()
{
    if (g_gdiplusToken == 0)
    {
        GdiplusStartupInput gdiplusStartupInput;
        GdiplusStartup(&g_gdiplusToken, &gdiplusStartupInput, NULL);
    }
}

//+---------------------------------------------------------------------------
//
// CCandidateWindow implementation
//
//----------------------------------------------------------------------------

CCandidateWindow::CCandidateWindow(CTextService* pTextService)
{
    _pTextService = pTextService;
    _hwnd = NULL;
    _selectedIndex = 0;
    _pageIndex = 0;
    _itemsPerPage = 8;
    
    InitializeGdiplus();
}

CCandidateWindow::~CCandidateWindow()
{
    Destroy();
}

BOOL CCandidateWindow::Create()
{
    if (_hwnd != NULL)
        return TRUE;

    // Register window class
    if (g_atomCandidateWindow == 0)
    {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = _WindowProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = L"ScripaTSF_CandidateWindow";
        
        g_atomCandidateWindow = RegisterClassExW(&wc);
        if (g_atomCandidateWindow == 0)
            return FALSE;
    }

    // Create window
    _hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
        L"ScripaTSF_CandidateWindow",
        L"",
        WS_POPUP | WS_BORDER,
        0, 0, 600, 120,
        NULL,
        NULL,
        GetModuleHandle(NULL),
        this
    );

    return (_hwnd != NULL);
}

void CCandidateWindow::Destroy()
{
    if (_hwnd)
    {
        DestroyWindow(_hwnd);
        _hwnd = NULL;
    }
}

void CCandidateWindow::Show(BOOL bShow)
{
    if (_hwnd)
    {
        ShowWindow(_hwnd, bShow ? SW_SHOWNOACTIVATE : SW_HIDE);
    }
}

void CCandidateWindow::Move(int x, int y)
{
    if (_hwnd)
    {
        SetWindowPos(_hwnd, HWND_TOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

void CCandidateWindow::Update(const std::vector<std::wstring>& candidates, int selected)
{
    _candidates = candidates;
    _selectedIndex = selected;
    
    if (_hwnd)
    {
        InvalidateRect(_hwnd, NULL, TRUE);
    }
}

LRESULT CALLBACK CCandidateWindow::_WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    CCandidateWindow* pThis = NULL;
    
    if (msg == WM_CREATE)
    {
        CREATESTRUCT* pcs = (CREATESTRUCT*)lParam;
        pThis = (CCandidateWindow*)pcs->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
    }
    else
    {
        pThis = (CCandidateWindow*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }

    switch (msg)
    {
    case WM_PAINT:
        if (pThis)
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            pThis->_OnPaint(hdc);
            EndPaint(hwnd, &ps);
        }
        return 0;

    case WM_LBUTTONDOWN:
        if (pThis)
        {
            pThis->_OnLButtonDown(LOWORD(lParam), HIWORD(lParam));
        }
        return 0;

    case WM_DESTROY:
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void CCandidateWindow::_OnPaint(HDC hdc)
{
    RECT rc;
    GetClientRect(_hwnd, &rc);

    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);

    // Background
    SolidBrush whiteBrush(Color(255, 255, 255));
    g.FillRectangle(&whiteBrush, 0, 0, rc.right, rc.bottom);

    // Draw composition buffer
    HFONT hFont = CreateFontW(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
    HFONT hOld = (HFONT)SelectObject(hdc, hFont);
    SetTextColor(hdc, RGB(0, 0, 0));
    SetBkMode(hdc, TRANSPARENT);

    RECT compRc = {10, 5, rc.right - 10, 20};
    std::wstring comp = L"Buffer: " + _composition;
    DrawTextW(hdc, comp.c_str(), (int)comp.size(), &compRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

    // Draw candidates
    int itemCount = (int)_candidates.size();
    int start = _pageIndex * _itemsPerPage;
    int shown = std::min(_itemsPerPage, std::max(0, itemCount - start));
    int padding = 8;
    int itemW = std::max(48, (int)((rc.right - rc.left - padding * shown) / std::max(1, shown)));
    int itemH = 36;

    for (int i = 0; i < shown; ++i)
    {
        int idx = start + i;
        int left = padding + i * (itemW + padding);
        int top = 35;
        RECT it = { left, top, left + itemW, top + itemH };

        // Background
        COLORREF bgColor = (idx == _selectedIndex) ? RGB(0, 120, 215) : RGB(255, 255, 255);
        HBRUSH hbr = CreateSolidBrush(bgColor);
        FillRect(hdc, &it, hbr);
        FrameRect(hdc, &it, (HBRUSH)GetStockObject(BLACK_BRUSH));
        DeleteObject(hbr);

        // Text
        SetTextColor(hdc, (idx == _selectedIndex) ? RGB(255, 255, 255) : RGB(0, 0, 0));
        DrawTextW(hdc, _candidates[idx].c_str(), (int)_candidates[idx].size(), &it, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        // Index number
        RECT numRc = { left + 4, top + 4, left + 24, top + 20 };
        WCHAR szNum[4];
        StringCchPrintfW(szNum, 4, L"%d", i + 1);
        SetTextColor(hdc, RGB(100, 100, 100));
        DrawTextW(hdc, szNum, (int)wcslen(szNum), &numRc, DT_LEFT | DT_TOP | DT_SINGLELINE);
    }

    // Page indicator
    int totalPages = (itemCount + _itemsPerPage - 1) / _itemsPerPage;
    WCHAR szPage[32];
    StringCchPrintfW(szPage, 32, L"Page %d/%d", _pageIndex + 1, std::max(1, totalPages));
    RECT pageRc = { rc.left + 10, 35 + itemH + 8, rc.right - 10, 35 + itemH + 24 };
    SetTextColor(hdc, RGB(80, 80, 80));
    DrawTextW(hdc, szPage, (int)wcslen(szPage), &pageRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    SelectObject(hdc, hOld);
    DeleteObject(hFont);
}

void CCandidateWindow::_OnLButtonDown(int x, int y)
{
    RECT rc;
    GetClientRect(_hwnd, &rc);
    
    int itemCount = (int)_candidates.size();
    int start = _pageIndex * _itemsPerPage;
    int shown = std::min(_itemsPerPage, std::max(0, itemCount - start));
    int padding = 8;
    int itemW = std::max(48, (int)((rc.right - rc.left - padding * shown) / std::max(1, shown)));
    int itemH = 36;

    for (int i = 0; i < shown; ++i)
    {
        int idx = start + i;
        int left = padding + i * (itemW + padding);
        int top = 35;
        RECT it = { left, top, left + itemW, top + itemH };
        
        POINT pt = {x, y};
        if (PtInRect(&it, pt))
        {
            // Notify text service to select this candidate
            _pTextService->_OnCandidateSelected(idx);
            return;
        }
    }
}

//+---------------------------------------------------------------------------
//
// CToolbar implementation
//
//----------------------------------------------------------------------------

CToolbar::CToolbar(CTextService* pTextService)
{
    _pTextService = pTextService;
    _hwnd = NULL;
    _hwndSettingsMenu = NULL;
    _hwndSchemeReference = NULL;
    _bIPAMode = TRUE;
    
    _iconENG = NULL;
    _iconIPA = NULL;
    _iconSettings = NULL;
    _iconDefault = NULL;
    
    InitializeGdiplus();
}

CToolbar::~CToolbar()
{
    if (_iconENG) delete _iconENG;
    if (_iconIPA) delete _iconIPA;
    if (_iconSettings) delete _iconSettings;
    if (_iconDefault) delete _iconDefault;
    
    Destroy();
}

BOOL CToolbar::Create()
{
    if (_hwnd != NULL)
        return TRUE;

    // Register window class
    if (g_atomToolbar == 0)
    {
        WNDCLASSEXW wc = {};
        wc.cbSize = sizeof(wc);
        wc.style = CS_HREDRAW | CS_VREDRAW;
        wc.lpfnWndProc = _WindowProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.lpszClassName = L"ScripaTSF_Toolbar";
        
        g_atomToolbar = RegisterClassExW(&wc);
        if (g_atomToolbar == 0)
            return FALSE;
    }

    // Create window
    _hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW,
        L"ScripaTSF_Toolbar",
        L"",
        WS_POPUP | WS_BORDER,
        0, 0, 150, 40,
        NULL,
        NULL,
        GetModuleHandle(NULL),
        this
    );

    if (_hwnd)
    {
        _LoadIcons();
    }

    return (_hwnd != NULL);
}

void CToolbar::Destroy()
{
    if (_hwndSchemeReference)
    {
        DestroyWindow(_hwndSchemeReference);
        _hwndSchemeReference = NULL;
    }
    
    if (_hwndSettingsMenu)
    {
        DestroyWindow(_hwndSettingsMenu);
        _hwndSettingsMenu = NULL;
    }
    
    if (_hwnd)
    {
        DestroyWindow(_hwnd);
        _hwnd = NULL;
    }
}

void CToolbar::Show(BOOL bShow)
{
    if (_hwnd)
    {
        ShowWindow(_hwnd, bShow ? SW_SHOWNOACTIVATE : SW_HIDE);
    }
}

void CToolbar::Move(int x, int y)
{
    if (_hwnd)
    {
        SetWindowPos(_hwnd, HWND_TOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

void CToolbar::UpdateMode(BOOL bIPA)
{
    _bIPAMode = bIPA;
    if (_hwnd)
    {
        InvalidateRect(_hwnd, NULL, TRUE);
    }
}

void CToolbar::_LoadIcons()
{
    // Load icons from uicon directory
    WCHAR szPath[MAX_PATH];
    GetModuleFileNameW(GetModuleHandle(NULL), szPath, MAX_PATH);
    
    // Navigate to uicon directory (DLL is in System32 or build, icons in relative path)
    // For development, assume icons are in ../uicon relative to DLL
    // For production, copy icons to System32 or use absolute path
    
    _iconENG = new Bitmap(L"..\\uicon\\ENG.png");
    _iconIPA = new Bitmap(L"..\\uicon\\IPA.png");
    _iconSettings = new Bitmap(L"..\\uicon\\settings.png");
    _iconDefault = new Bitmap(L"..\\uicon\\default.png");
}

LRESULT CALLBACK CToolbar::_WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    CToolbar* pThis = NULL;
    
    if (msg == WM_CREATE)
    {
        CREATESTRUCT* pcs = (CREATESTRUCT*)lParam;
        pThis = (CToolbar*)pcs->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
    }
    else
    {
        pThis = (CToolbar*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }

    switch (msg)
    {
    case WM_PAINT:
        if (pThis)
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            pThis->_OnPaint(hdc);
            EndPaint(hwnd, &ps);
        }
        return 0;

    case WM_LBUTTONDOWN:
        if (pThis)
        {
            pThis->_OnLButtonDown(LOWORD(lParam), HIWORD(lParam));
        }
        return 0;

    case WM_DESTROY:
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

void CToolbar::_OnPaint(HDC hdc)
{
    RECT rc;
    GetClientRect(_hwnd, &rc);

    Graphics g(hdc);
    g.SetSmoothingMode(SmoothingModeAntiAlias);

    // Background
    SolidBrush bgBrush(Color(255, 240, 240, 240));
    g.FillRectangle(&bgBrush, 0, 0, rc.right, rc.bottom);

    // Draw buttons
    int btnSize = 30;
    int btnY = 5;
    int btnX = 5;
    int spacing = 5;

    // Mode button (ENG/IPA)
    _rectMode = {btnX, btnY, btnX + btnSize, btnY + btnSize};
    Bitmap* iconMode = _bIPAMode ? _iconIPA : _iconENG;
    if (iconMode && iconMode->GetLastStatus() == Ok)
    {
        g.DrawImage(iconMode, btnX, btnY, btnSize, btnSize);
    }
    Pen btnPen(Color(0, 0, 0), 1);
    g.DrawRectangle(&btnPen, btnX, btnY, btnSize, btnSize);

    btnX += btnSize + spacing;

    // Default button (Scheme Reference)
    _rectDefault = {btnX, btnY, btnX + btnSize, btnY + btnSize};
    if (_iconDefault && _iconDefault->GetLastStatus() == Ok)
    {
        g.DrawImage(_iconDefault, btnX, btnY, btnSize, btnSize);
    }
    g.DrawRectangle(&btnPen, btnX, btnY, btnSize, btnSize);

    btnX += btnSize + spacing;

    // Settings button
    _rectSettings = {btnX, btnY, btnX + btnSize, btnY + btnSize};
    if (_iconSettings && _iconSettings->GetLastStatus() == Ok)
    {
        g.DrawImage(_iconSettings, btnX, btnY, btnSize, btnSize);
    }
    g.DrawRectangle(&btnPen, btnX, btnY, btnSize, btnSize);
}

void CToolbar::_OnLButtonDown(int x, int y)
{
    POINT pt = {x, y};
    
    // Mode button
    if (PtInRect(&_rectMode, pt))
    {
        _pTextService->_OnToggleMode();
        return;
    }
    
    // Default button - show scheme reference
    if (PtInRect(&_rectDefault, pt))
    {
        _ShowSchemeReference();
        return;
    }
    
    // Settings button
    if (PtInRect(&_rectSettings, pt))
    {
        _ShowSettingsMenu();
        return;
    }
}

void CToolbar::_ShowSettingsMenu()
{
    // TODO: Implement settings menu (copy from demo exe)
    MessageBoxW(_hwnd, L"Settings menu coming soon", L"Scripa TSF", MB_OK);
}

void CToolbar::_ShowSchemeReference()
{
    // TODO: Implement scheme reference window (copy from demo exe)
    MessageBoxW(_hwnd, L"Scheme reference coming soon", L"Scripa TSF", MB_OK);
}
