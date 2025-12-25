#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <locale>
#include <codecvt>
#include "../tsf/ScripaTSF.h"
#include "../core/Dic.hpp"
#include <gdiplus.h>
#include <shellapi.h>
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "shell32.lib")


using namespace Gdiplus;

static const wchar_t CLASS_NAME[] = L"ScrIPA";

// Icon & toolbar structure
struct IconSet {
    Bitmap* bcg = nullptr;
    Bitmap* typing_bg = nullptr;
    Bitmap* user = nullptr;
    Bitmap* default_icon = nullptr;
    Bitmap* eng = nullptr;
    Bitmap* ipa = nullptr;
    Bitmap* left = nullptr;
    Bitmap* right = nullptr;
    Bitmap* settings = nullptr;
    // Settings menu icons (16x16)
    Bitmap* icon_else = nullptr;
    Bitmap* icon_skin = nullptr;
    Bitmap* icon_choosefile = nullptr;
    Bitmap* icon_custom = nullptr;

    void LoadAll() {
        bcg = new Bitmap(L"..\\uicon\\bcg.png");
        typing_bg = new Bitmap(L"..\\uicon\\typing_bg.png");
        user = new Bitmap(L"..\\uicon\\user.png");
        default_icon = new Bitmap(L"..\\uicon\\default.png");
        eng = new Bitmap(L"..\\uicon\\ENG.png");
        ipa = new Bitmap(L"..\\uicon\\IPA.png");
        left = new Bitmap(L"..\\uicon\\left.png");
        right = new Bitmap(L"..\\uicon\\right.png");
        settings = new Bitmap(L"..\\uicon\\settings.png");
        // Load menu icons
        icon_else = new Bitmap(L"..\\uicon\\else.png");
        icon_skin = new Bitmap(L"..\\uicon\\skin.png");
        icon_choosefile = new Bitmap(L"..\\uicon\\choosefile.png");
        icon_custom = new Bitmap(L"..\\uicon\\custom.png");
    }

    void UnloadAll() {
        if (bcg) { delete bcg; bcg = nullptr; }
        if (typing_bg) { delete typing_bg; typing_bg = nullptr; }
        if (user) { delete user; user = nullptr; }
        if (default_icon) { delete default_icon; default_icon = nullptr; }
        if (eng) { delete eng; eng = nullptr; }
        if (ipa) { delete ipa; ipa = nullptr; }
        if (left) { delete left; left = nullptr; }
        if (right) { delete right; right = nullptr; }
        if (settings) { delete settings; settings = nullptr; }
        // Unload menu icons
        if (icon_else) { delete icon_else; icon_else = nullptr; }
        if (icon_skin) { delete icon_skin; icon_skin = nullptr; }
        if (icon_choosefile) { delete icon_choosefile; icon_choosefile = nullptr; }
        if (icon_custom) { delete icon_custom; icon_custom = nullptr; }
    }
};

static IconSet g_icons;
static ULONG_PTR g_gdiplusToken = 0;

struct CandidateUI {
    std::vector<std::wstring> items;
    int selected = 0;
    std::wstring composition = L"th";
    int pageIndex = 0;
    int itemsPerPage = 8;
    bool modeIPA = true;
    bool visible = false;
};

static CandidateUI g_ui;
static ScripaTSF g_backend;

// 剪贴板辅助函数：复制文本到系统剪贴板
static bool CopyToClipboard(HWND hwnd, const std::wstring& text) {
    if (!OpenClipboard(hwnd)) {
        return false;
    }
    EmptyClipboard();
    
    size_t size = (text.length() + 1) * sizeof(wchar_t);
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!hMem) {
        CloseClipboard();
        return false;
    }
    
    wchar_t* pMem = (wchar_t*)GlobalLock(hMem);
    if (pMem) {
        memcpy(pMem, text.c_str(), size);
        GlobalUnlock(hMem);
    }
    
    SetClipboardData(CF_UNICODETEXT, hMem);
    CloseClipboard();
    return true;
}

// Button rects (cached)
RECT g_btnDefault = {0, 0, 0, 0};
RECT g_btnUser = {0, 0, 0, 0};
RECT g_btnMode = {0, 0, 0, 0};
RECT g_btnLeft = {0, 0, 0, 0};
RECT g_btnRight = {0, 0, 0, 0};
RECT g_btnSettings = {0, 0, 0, 0};

// Settings menu popup window
static HWND g_hwndSettingsMenu = NULL;
static HWND g_hwndParentMain = NULL;

// Scheme reference popup window
static HWND g_hwndSchemeReference = NULL;

struct SettingsMenuItem {
    std::wstring text;
    Bitmap* icon;
    int id;
};

static std::vector<SettingsMenuItem> g_menuItems;

// Scheme selection dialog with checkboxes
static std::vector<std::string> g_schemeFiles;
static std::vector<HWND> g_schemeCheckboxes;

LRESULT CALLBACK SchemeSelectionDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_INITDIALOG:
    {
        // Get available schemes
        g_schemeFiles = g_backend.GetAvailableSchemes();
        g_schemeCheckboxes.clear();
        
        // Create checkboxes for each scheme
        int yPos = 15;
        for (size_t i = 0; i < g_schemeFiles.size(); ++i) {
            bool enabled = g_backend.IsSchemeEnabled(g_schemeFiles[i]);
            
            // Convert scheme name to wstring
            std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
            std::wstring schemeName = conv.from_bytes(g_schemeFiles[i]);
            
            // Create checkbox
            HWND hwndCheck = CreateWindowW(
                L"BUTTON",
                schemeName.c_str(),
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                20, yPos, 320, 20,
                hwndDlg,
                (HMENU)(INT_PTR)(1000 + i),
                GetModuleHandle(NULL),
                NULL
            );
            
            // Set checkbox state
            SendMessage(hwndCheck, BM_SETCHECK, enabled ? BST_CHECKED : BST_UNCHECKED, 0);
            g_schemeCheckboxes.push_back(hwndCheck);
            
            yPos += 25;
        }
        
        // Create Apply and Cancel buttons
        CreateWindowW(
            L"BUTTON",
            L"Apply",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            80, yPos + 15, 80, 30,
            hwndDlg,
            (HMENU)IDOK,
            GetModuleHandle(NULL),
            NULL
        );
        
        CreateWindowW(
            L"BUTTON",
            L"Cancel",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            180, yPos + 15, 80, 30,
            hwndDlg,
            (HMENU)IDCANCEL,
            GetModuleHandle(NULL),
            NULL
        );
        
        // Resize dialog based on number of schemes
        int dialogHeight = yPos + 80;  // Increased for button visibility
        SetWindowPos(hwndDlg, NULL, 0, 0, 380, dialogHeight, SWP_NOMOVE | SWP_NOZORDER);
        
        return 0;
    }
    
    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK) {
            // Apply changes
            for (size_t i = 0; i < g_schemeCheckboxes.size(); ++i) {
                LRESULT checked = SendMessage(g_schemeCheckboxes[i], BM_GETCHECK, 0, 0);
                bool shouldEnable = (checked == BST_CHECKED);
                bool currentlyEnabled = g_backend.IsSchemeEnabled(g_schemeFiles[i]);
                
                if (shouldEnable && !currentlyEnabled) {
                    g_backend.EnableScheme(g_schemeFiles[i]);
                } else if (!shouldEnable && currentlyEnabled) {
                    g_backend.DisableScheme(g_schemeFiles[i]);
                }
            }
            
            // Reload dictionary with new scheme selection
            g_backend.ReloadSchemes();
            
            g_schemeCheckboxes.clear();
            DestroyWindow(hwndDlg);
            return 0;
        } else if (LOWORD(wParam) == IDCANCEL) {
            g_schemeCheckboxes.clear();
            DestroyWindow(hwndDlg);
            return 0;
        }
        break;
    
    case WM_CLOSE:
        g_schemeCheckboxes.clear();
        DestroyWindow(hwndDlg);
        return 0;
    }
    
    return DefWindowProcW(hwndDlg, msg, wParam, lParam);
}

void ShowSchemeSelectionDialog(HWND hwndParent) {
    // Create dialog window class if not exists
    static bool registered = false;
    if (!registered) {
        WNDCLASSW wc = {};
        wc.lpfnWndProc = SchemeSelectionDlgProc;  // Use our custom dialog proc
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = L"SchemeSelectionDialog";
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        RegisterClassW(&wc);
        registered = true;
    }
    
    // Create dialog
    HWND hwndDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        L"SchemeSelectionDialog",
        L"Choose Schemes",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, -280, -40,
        hwndParent,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );
    
    if (!hwndDlg) {
        MessageBoxW(hwndParent, L"Failed to create dialog window", L"Error", MB_OK | MB_ICONERROR);
        return;
    }
    
    // Center dialog on parent
    RECT rcParent, rcDlg;
    GetWindowRect(hwndParent, &rcParent);
    GetWindowRect(hwndDlg, &rcDlg);
    int x = rcParent.left + (rcParent.right - rcParent.left - (rcDlg.right - rcDlg.left)) / 2;
    int y = rcParent.top + (rcParent.bottom - rcParent.top - (rcDlg.bottom - rcDlg.top)) / 2;
    SetWindowPos(hwndDlg, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    
    // Manually trigger initialization (since WM_CREATE doesn't handle WM_INITDIALOG)
    SchemeSelectionDlgProc(hwndDlg, WM_INITDIALOG, 0, 0);
    
    // Disable parent window
    EnableWindow(hwndParent, FALSE);
    
    // Modal message loop
    MSG msg;
    BOOL bRet;
    while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0) {
        if (bRet == -1) {
            break;
        }
        
        if (!IsWindow(hwndDlg)) {
            break;
        }
        
        if (msg.hwnd == hwndDlg || IsChild(hwndDlg, msg.hwnd)) {
            if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE) {
                SendMessage(hwndDlg, WM_COMMAND, IDCANCEL, 0);
                continue;
            }
            if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN) {
                SendMessage(hwndDlg, WM_COMMAND, IDOK, 0);
                continue;
            }
            
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            // Dispatch messages for other windows
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    
    // Re-enable parent window
    EnableWindow(hwndParent, TRUE);
    SetForegroundWindow(hwndParent);
    
    if (IsWindow(hwndDlg)) {
        DestroyWindow(hwndDlg);
    }
}

// Scheme Reference popup window procedure
LRESULT CALLBACK SchemeReferenceWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_CREATE:
        return 0;
    
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        
        Graphics g(hdc);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintAntiAlias);
        
        // Background
        SolidBrush bgBrush(Color(255, 250, 250, 250));
        g.FillRectangle(&bgBrush, 0, 0, rc.right, rc.bottom);
        
        // Title
        SolidBrush titleBrush(Color(255, 60, 60, 60));
        Gdiplus::Font titleFont(L"Segoe UI Semibold", 16);
        StringFormat centerFormat;
        centerFormat.SetAlignment(StringAlignmentCenter);
        centerFormat.SetLineAlignment(StringAlignmentNear);
        
        RectF titleRect(10.0f, 20.0f, (float)rc.right - 20, 40.0f);
        g.DrawString(L"Scheme Reference", -1, &titleFont, titleRect, &centerFormat, &titleBrush);
        
        // Placeholder content
        SolidBrush textBrush(Color(255, 100, 100, 100));
        Gdiplus::Font textFont(L"Segoe UI", 11);
        StringFormat leftFormat;
        leftFormat.SetAlignment(StringAlignmentNear);
        leftFormat.SetLineAlignment(StringAlignmentNear);
        
        RectF contentRect(20.0f, 80.0f, (float)rc.right - 40, (float)rc.bottom - 100);
        std::wstring content = 
            L"IPA Input Scheme Reference\\n\\n"
            L"[Image/Chart Placeholder]\\n\\n"
            L"This window displays:\\n"
            L"  • Character mappings\\n"
            L"  • Key combinations\\n"
            L"  • Tone markers (T1-T5)\\n"
            L"  • Special symbols\\n\\n"
            L"Future: Load reference image from\\n"
            L"  uicon\\\\scheme_reference.png\\n\\n"
            L"This window does not block\\n"
            L"the main input window.";
        g.DrawString(content.c_str(), -1, &textFont, contentRect, &leftFormat, &textBrush);
        
        EndPaint(hwnd, &ps);
        return 0;
    }
    
    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        return 0;
    
    case WM_DESTROY:
        return 0;
    }
    
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// Settings menu window procedure
LRESULT CALLBACK SettingsMenuProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_CREATE:
        g_menuItems.clear();
        g_menuItems.push_back({L"Other Settings", g_icons.icon_else, 1});
        g_menuItems.push_back({L"Change Theme", g_icons.icon_skin, 2});
        g_menuItems.push_back({L"Choose Schemes", g_icons.icon_choosefile, 3});
        g_menuItems.push_back({L"Custom Dictionary", g_icons.icon_custom, 4});
        return 0;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        
        Graphics g(hdc);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintAntiAlias);
        
        // Background
        SolidBrush bgBrush(Color(255, 245, 245, 245));
        g.FillRectangle(&bgBrush, 0, 0, 200, (int)g_menuItems.size() * 32);
        
        // Draw menu items
        SolidBrush textBrush(Color(255, 50, 50, 50));
        Gdiplus::Font font(L"Segoe UI", 10);
        StringFormat format;
        format.SetAlignment(StringAlignmentNear);
        format.SetLineAlignment(StringAlignmentCenter);
        
        POINT pt;
        GetCursorPos(&pt);
        ScreenToClient(hwnd, &pt);
        
        for (size_t i = 0; i < g_menuItems.size(); i++) {
            int y = (int)i * 32;
            RECT itemRect = {0, y, 200, y + 32};
            
            // Hover effect
            if (PtInRect(&itemRect, pt)) {
                SolidBrush hoverBrush(Color(255, 220, 220, 220));
                g.FillRectangle(&hoverBrush, 0, y, 200, 32);
            }
            
            // Draw icon (16x16)
            if (g_menuItems[i].icon && g_menuItems[i].icon->GetLastStatus() == Ok) {
                g.DrawImage(g_menuItems[i].icon, 8, y + 8, 16, 16);
            }
            
            // Draw text
            RectF textRect(32.0f, (float)y, 168.0f, 32.0f);
            g.DrawString(g_menuItems[i].text.c_str(), -1, &font, textRect, &format, &textBrush);
        }
        
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_MOUSEMOVE:
    {
        static POINT lastPt = {-1, -1};
        POINT pt = {LOWORD(lParam), HIWORD(lParam)};
        
        // Only invalidate if mouse moved to different menu item
        int oldItem = lastPt.y / 32;
        int newItem = pt.y / 32;
        if (oldItem != newItem) {
            InvalidateRect(hwnd, NULL, FALSE);
            lastPt = pt;
        }
        
        // Track mouse leave
        TRACKMOUSEEVENT tme = {};
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = hwnd;
        TrackMouseEvent(&tme);
        return 0;
    }

    case WM_MOUSELEAVE:
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    case WM_LBUTTONDOWN:
    {
        int x = LOWORD(lParam);
        int y = HIWORD(lParam);
        int itemIndex = y / 32;
        
        if (itemIndex >= 0 && itemIndex < (int)g_menuItems.size()) {
            int menuId = g_menuItems[itemIndex].id;
            
            // Close menu
            ShowWindow(hwnd, SW_HIDE);
            
            // Handle menu action
            switch (menuId) {
            case 1: // Other Settings
                MessageBoxW(g_hwndParentMain, L"Other Settings (TBD)", L"ScrIPA", MB_OK);
                break;
            case 2: // Change Theme
                MessageBoxW(g_hwndParentMain, L"Change Theme (TBD)", L"ScrIPA", MB_OK);
                break;
            case 3: // Choose Schemes
                ShowSchemeSelectionDialog(g_hwndParentMain);
                break;
            case 4: // Custom Dictionary
            {
                // Open custom.txt with notepad
                std::wstring customPath = L"..\\schemes\\custom.txt";
                ShellExecuteW(NULL, L"open", L"notepad.exe", customPath.c_str(), NULL, SW_SHOW);
                break;
            }
            }
        }
        return 0;
    }

    case WM_KILLFOCUS:
    case WM_ACTIVATE:
        if (wParam == WA_INACTIVE) {
            ShowWindow(hwnd, SW_HIDE);
        }
        return 0;

    case WM_DESTROY:
        g_menuItems.clear();
        return 0;
    }
    
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}



LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_CREATE:
        {
            bool initOk = g_backend.Init();
            if (!initOk) {
                MessageBoxW(hwnd, L"Failed to load scheme files! Check console output.", L"ScrIPA Error", MB_OK | MB_ICONERROR);
            }
            g_backend.clearBuffer();  // 清空初始 buffer
            g_ui.modeIPA = g_backend.GetMode();  // sync with backend
            g_ui.composition = g_backend.GetComposition();
            g_ui.items = g_backend.GetCandidates();
            // 不再使用假数据，如果没有候选就留空
            if (g_ui.items.empty()) {
                g_ui.items = { L"" };
            }
            g_ui.selected = 0;
            g_ui.pageIndex = 0;
            g_ui.visible = true;
        }
        return 0;


    case WM_KEYDOWN:
        // 大键盘数字键 1-9：用于选择候选项
        if (wParam >= '1' && wParam <= '9') {
            int num = (int)(wParam - '1');
            int idx = g_ui.pageIndex * g_ui.itemsPerPage + num;
            if (idx >= 0 && idx < (int)g_ui.items.size()) {
                // 复制到剪贴板
                std::wstring selectedText = g_ui.items[idx];
                if (CopyToClipboard(hwnd, selectedText)) {
                    std::wstringstream ss;
                    // ss << L"Selected #" << (idx + 1) << L": " << g_ui.items[idx];
                    // MessageBoxW(hwnd, ss.str().c_str(), L"ScrIPA Demo", MB_OK);
                    ss << L"Selected & Copied:\n" << selectedText;
                    MessageBoxW(hwnd, ss.str().c_str(), L"ScrIPA", MB_OK | MB_ICONINFORMATION);
                } else {
                    MessageBoxW(hwnd, L"Failed to copy to clipboard", L"ScrIPA Error", MB_OK | MB_ICONERROR);
                }
                
                // 清空 buffer 并重置 UI
                g_backend.clearBuffer();
                g_ui.composition = L"";
                g_ui.items = { L"" };
                g_ui.selected = 0;
                g_ui.pageIndex = 0;
                InvalidateRect(hwnd, NULL, TRUE);
            }
            return 0;  // 阻止生成 WM_CHAR
        }
        
        // Backspace in composition mode: clear last character
        if (wParam == VK_BACK) {
            // Delete last character from buffer
            g_backend.deleteLastChar();
            // Update UI
            g_ui.composition = g_backend.GetComposition();
            g_ui.items = g_backend.GetCandidates();
            g_ui.selected = 0;
            g_ui.pageIndex = 0;
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }
        if (wParam == VK_RIGHT) {
            g_ui.selected = (g_ui.selected + 1) % (int)g_ui.items.size();
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }
        if (wParam == VK_LEFT) {
            g_ui.selected = (g_ui.selected - 1 + (int)g_ui.items.size()) % (int)g_ui.items.size();
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }
        if (wParam == VK_RETURN) {
            int idx = g_ui.pageIndex * g_ui.itemsPerPage + g_ui.selected;
            if (idx >= 0 && idx < (int)g_ui.items.size()) {
                // 复制到剪贴板，测试后删除
                std::wstring selectedText = g_ui.items[idx];
                if (CopyToClipboard(hwnd, selectedText)) {
                    std::wstringstream ss;
                    // ss << L"Selected #" << (idx + 1) << L": " << g_ui.items[idx];
                    // MessageBoxW(hwnd, ss.str().c_str(), L"ScrIPA Demo", MB_OK);
                    ss << L"Selected & Copied:\n" << selectedText;
                    MessageBoxW(hwnd, ss.str().c_str(), L"ScrIPA", MB_OK | MB_ICONINFORMATION);
                } else {
                    MessageBoxW(hwnd, L"Failed to copy to clipboard", L"ScrIPA Error", MB_OK | MB_ICONERROR);
                }
                
                // 清空 buffer 并重置 UI
                g_backend.clearBuffer();
                g_ui.composition = L"";
                g_ui.items = { L"" };
                g_ui.selected = 0;
                g_ui.pageIndex = 0;
                InvalidateRect(hwnd, NULL, TRUE);
            }
            return 0;
        }
        if (wParam == VK_ESCAPE) {
            PostQuitMessage(0);
            return 0;
        }
        return 0;

    case WM_CHAR:
        {
            wchar_t ch = (wchar_t)wParam;
            
            // 忽略 backspace 字符，已在 WM_KEYDOWN 中处理
            if (ch == 0x08) {
                return 0;
            }
            
            // page navigation
            if (ch == L'[') {
                if (g_ui.pageIndex > 0) g_ui.pageIndex--;
                InvalidateRect(hwnd, NULL, TRUE);
                return 0;
            }
            if (ch == L']') {
                int totalPages = (int)((g_ui.items.size() + g_ui.itemsPerPage - 1) / g_ui.itemsPerPage);
                if (g_ui.pageIndex + 1 < totalPages) g_ui.pageIndex++;
                InvalidateRect(hwnd, NULL, TRUE);
                return 0;
            }

            // 小键盘数字会到达这里，作为普通字符输入
            // 大键盘数字已在 WM_KEYDOWN 中被拦截，不会到达这里

            // forward character to backend and refresh UI
            bool handled = g_backend.OnKeyDown(ch);
            g_ui.composition = g_backend.GetComposition();
            g_ui.items = g_backend.GetCandidates();
            if (g_ui.items.empty()) {
                g_ui.items = { L"" };
            }
            // reset to first page on new composition
            int totalPages = (int)((g_ui.items.size() + g_ui.itemsPerPage - 1) / g_ui.itemsPerPage);
            if (g_ui.pageIndex >= totalPages) g_ui.pageIndex = (std::max)(0, totalPages - 1);
            g_ui.selected = 0;  // reset selection to first item
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }

    case WM_LBUTTONDOWN:
        {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);

            // check toolbar buttons
            if (PtInRect(&g_btnDefault, POINT{x, y})) {
                // Toggle scheme reference window
                if (!g_hwndSchemeReference) {
                    // Register window class
                    WNDCLASSW wcRef = {};
                    wcRef.lpfnWndProc = SchemeReferenceWndProc;
                    wcRef.hInstance = GetModuleHandle(NULL);
                    wcRef.lpszClassName = L"SchemeReferenceWindow";
                    wcRef.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
                    wcRef.hCursor = LoadCursor(NULL, IDC_ARROW);
                    RegisterClassW(&wcRef);
                    
                    // Create window
                    g_hwndSchemeReference = CreateWindowExW(
                        WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
                        L"SchemeReferenceWindow",
                        L"Scheme Reference",
                        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME,
                        CW_USEDEFAULT, CW_USEDEFAULT, 350, 500,
                        NULL,  // No parent, independent window
                        NULL,
                        GetModuleHandle(NULL),
                        NULL
                    );
                }
                
                if (g_hwndSchemeReference) {
                    // Toggle visibility
                    if (IsWindowVisible(g_hwndSchemeReference)) {
                        ShowWindow(g_hwndSchemeReference, SW_HIDE);
                    } else {
                        // Position below main window
                        RECT rcMain;
                        GetWindowRect(hwnd, &rcMain);
                        SetWindowPos(g_hwndSchemeReference, HWND_TOPMOST,
                                     rcMain.left, rcMain.bottom,
                                     0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
                    }
                }
                return 0;
            }
            if (PtInRect(&g_btnUser, POINT{x, y})) {
                MessageBoxW(hwnd, L"User settings (TBD)", L"Scripa", MB_OK);
                return 0;
            }
            if (PtInRect(&g_btnMode, POINT{x, y})) {
                g_backend.ToggleMode();
                g_ui.modeIPA = g_backend.GetMode();
                // Refresh candidates for new mode
                g_ui.composition = g_backend.GetComposition();
                g_ui.items = g_backend.GetCandidates();
                if (g_ui.items.empty()) {
                    g_ui.items = { L"" };
                }
                g_ui.pageIndex = 0;
                g_ui.selected = 0;
                InvalidateRect(hwnd, NULL, TRUE);
                return 0;
            }
            if (PtInRect(&g_btnLeft, POINT{x, y})) {
                if (g_ui.pageIndex > 0) g_ui.pageIndex--;
                InvalidateRect(hwnd, NULL, TRUE);
                return 0;
            }
            if (PtInRect(&g_btnRight, POINT{x, y})) {
                int totalPages = (int)((g_ui.items.size() + g_ui.itemsPerPage - 1) / g_ui.itemsPerPage);
                if (g_ui.pageIndex + 1 < totalPages) g_ui.pageIndex++;
                InvalidateRect(hwnd, NULL, TRUE);
                return 0;
            }
            if (PtInRect(&g_btnSettings, POINT{x, y})) {
                // Show settings dropdown menu
                if (!g_hwndSettingsMenu) {
                    // Create menu window on first use
                    WNDCLASSW wcMenu = {};
                    wcMenu.lpfnWndProc = SettingsMenuProc;
                    wcMenu.hInstance = GetModuleHandle(NULL);
                    wcMenu.lpszClassName = L"ScripaSettingsMenu";
                    wcMenu.hCursor = LoadCursor(NULL, IDC_ARROW);
                    RegisterClassW(&wcMenu);
                    
                    g_hwndSettingsMenu = CreateWindowExW(
                        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
                        L"ScripaSettingsMenu",
                        L"",
                        WS_POPUP | WS_BORDER,
                        0, 0, 200, 128,  // 4 items * 32px height
                        hwnd,
                        NULL,
                        GetModuleHandle(NULL),
                        NULL
                    );
                    g_hwndParentMain = hwnd;
                }
                
                // Position menu below settings button
                POINT pt = {g_btnSettings.left, g_btnSettings.bottom};
                ClientToScreen(hwnd, &pt);
                SetWindowPos(g_hwndSettingsMenu, HWND_TOPMOST, pt.x, pt.y, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
                SetFocus(g_hwndSettingsMenu);
                
                return 0;
            }

            // check candidate items
            {
                RECT rc;
                GetClientRect(hwnd, &rc);
                int itemCount = (int)g_ui.items.size();
                int per = g_ui.itemsPerPage;
                int start = g_ui.pageIndex * per;
                int shown = (std::min)(per, (std::max)(0, itemCount - start));
                int padding = 8;
                int itemW = (std::max)(48, (int)((rc.right - rc.left - padding * shown) / (std::max)(1, shown)));
                int itemH = 36;
                for (int i = 0; i < shown; ++i) {
                    int idx = start + i;
                    int left = padding + i * (itemW + padding);
                    int top = 60;
                    RECT it = { left, top, left + itemW, top + itemH };
                    if (PtInRect(&it, POINT{x, y})) {
                        g_ui.selected = i;  // store page-relative index
                        
                        // 复制到剪贴板
                        std::wstring selectedText = g_ui.items[idx];
                        if (CopyToClipboard(hwnd, selectedText)) {
                            std::wstringstream ss;
                            // ss << L"Selected #" << (idx + 1) << L": " << g_ui.items[idx];
                            // MessageBoxW(hwnd, ss.str().c_str(), L"Scripa Demo", MB_OK);
                            ss << L"Selected & Copied:\n" << selectedText;
                            MessageBoxW(hwnd, ss.str().c_str(), L"ScrIPA", MB_OK | MB_ICONINFORMATION);
                        } else {
                            MessageBoxW(hwnd, L"Failed to copy to clipboard", L"ScrIPA Error", MB_OK | MB_ICONERROR);
                        }
                        
                        // 清空 buffer 并重置 UI
                        g_backend.clearBuffer();
                        g_ui.composition = L"";
                        g_ui.items = { L"" };
                        g_ui.selected = 0;
                        g_ui.pageIndex = 0;
                        InvalidateRect(hwnd, NULL, TRUE);
                        return 0;
                    }
                }
            }
        }
        return 0;

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);

            Graphics g(hdc);
            g.SetSmoothingMode(SmoothingModeAntiAlias);

            // background (white)
            SolidBrush whiteBrush(Color(255, 255, 255));
            g.FillRectangle(&whiteBrush, 0, 0, rc.right, rc.bottom);

            // draw toolbar (top 40 pixels)
            int toolbarY = 8;
            int btnX = 10;
            int btnSize = 30;
            int spacing = 8;

            // default button
            g_btnDefault = {btnX, toolbarY, btnX + btnSize, toolbarY + btnSize};
            if (g_icons.default_icon && g_icons.default_icon->GetLastStatus() == Ok) {
                g.DrawImage(g_icons.default_icon, btnX, toolbarY, btnSize, btnSize);
            } else {
                SolidBrush btnBrush(Color(200, 200, 200));
                g.FillRectangle(&btnBrush, btnX, toolbarY, btnSize, btnSize);
            }
            Pen btnPen(Color(0, 0, 0), 1);
            g.DrawRectangle(&btnPen, btnX, toolbarY, btnSize, btnSize);

            btnX += btnSize + spacing;

            // user button
            g_btnUser = {btnX, toolbarY, btnX + btnSize, toolbarY + btnSize};
            if (g_icons.user && g_icons.user->GetLastStatus() == Ok) {
                g.DrawImage(g_icons.user, btnX, toolbarY, btnSize, btnSize);
            } else {
                SolidBrush btnBrush(Color(200, 200, 200));
                g.FillRectangle(&btnBrush, btnX, toolbarY, btnSize, btnSize);
            }
            g.DrawRectangle(&btnPen, btnX, toolbarY, btnSize, btnSize);

            btnX += btnSize + spacing + 20;  // extra space before mode button

            // mode button (ENG/IPA)
            g_btnMode = {btnX, toolbarY, btnX + btnSize, toolbarY + btnSize};
            if (g_ui.modeIPA && g_icons.ipa && g_icons.ipa->GetLastStatus() == Ok) {
                g.DrawImage(g_icons.ipa, btnX, toolbarY, btnSize, btnSize);
            } else if (!g_ui.modeIPA && g_icons.eng && g_icons.eng->GetLastStatus() == Ok) {
                g.DrawImage(g_icons.eng, btnX, toolbarY, btnSize, btnSize);
            } else {
                SolidBrush modeBrush(Color(150, 150, 200));
                g.FillRectangle(&modeBrush, btnX, toolbarY, btnSize, btnSize);
            }
            g.DrawRectangle(&btnPen, btnX, toolbarY, btnSize, btnSize);

            // positioning for right-aligned buttons
            btnX = rc.right - 10 - (btnSize + spacing) * 3;

            // left button
            g_btnLeft = {btnX, toolbarY, btnX + btnSize, toolbarY + btnSize};
            if (g_icons.left && g_icons.left->GetLastStatus() == Ok) {
                g.DrawImage(g_icons.left, btnX, toolbarY, btnSize, btnSize);
            } else {
                SolidBrush btnBrush(Color(200, 200, 200));
                g.FillRectangle(&btnBrush, btnX, toolbarY, btnSize, btnSize);
            }
            g.DrawRectangle(&btnPen, btnX, toolbarY, btnSize, btnSize);

            btnX += btnSize + spacing;

            // right button
            g_btnRight = {btnX, toolbarY, btnX + btnSize, toolbarY + btnSize};
            if (g_icons.right && g_icons.right->GetLastStatus() == Ok) {
                g.DrawImage(g_icons.right, btnX, toolbarY, btnSize, btnSize);
            } else {
                SolidBrush btnBrush(Color(200, 200, 200));
                g.FillRectangle(&btnBrush, btnX, toolbarY, btnSize, btnSize);
            }
            g.DrawRectangle(&btnPen, btnX, toolbarY, btnSize, btnSize);

            btnX += btnSize + spacing;

            // settings button
            g_btnSettings = {btnX, toolbarY, btnX + btnSize, toolbarY + btnSize};
            if (g_icons.settings && g_icons.settings->GetLastStatus() == Ok) {
                g.DrawImage(g_icons.settings, btnX, toolbarY, btnSize, btnSize);
            } else {
                SolidBrush btnBrush(Color(200, 200, 200));
                g.FillRectangle(&btnBrush, btnX, toolbarY, btnSize, btnSize);
            }
            g.DrawRectangle(&btnPen, btnX, toolbarY, btnSize, btnSize);

            // draw composition & candidates
            {
                HFONT hFont = CreateFontW(16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
                HFONT hOld = (HFONT)SelectObject(hdc, hFont);
                SetTextColor(hdc, RGB(0, 0, 0));
                SetBkMode(hdc, TRANSPARENT);

                // composition buffer display
                RECT compRc = {10, 45, rc.right - 10, 58};
                std::wstring comp = L"Buffer: " + g_ui.composition;
                DrawTextW(hdc, comp.c_str(), (int)comp.size(), &compRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

                // draw candidates
                int itemCount = (int)g_ui.items.size();
                int per = g_ui.itemsPerPage;
                int start = g_ui.pageIndex * per;
                int shown = (std::min)(per, (std::max)(0, itemCount - start));
                int padding = 8;
                int itemW = (std::max)(48, (int)((rc.right - rc.left - padding * shown) / (std::max)(1, shown)));
                int itemH = 36;

                for (int i = 0; i < shown; ++i) {
                    int idx = start + i;
                    int left = padding + i * (itemW + padding);
                    int top = 60;
                    RECT it = { left, top, left + itemW, top + itemH };

                    // draw candidate background and border
                    COLORREF bgColor = (idx == g_ui.selected) ? RGB(0, 120, 215) : RGB(255, 255, 255);
                    HBRUSH hbr = CreateSolidBrush(bgColor);
                    FillRect(hdc, &it, hbr);
                    FrameRect(hdc, &it, (HBRUSH)GetStockObject(BLACK_BRUSH));
                    DeleteObject(hbr);

                    // draw text
                    SetTextColor(hdc, (idx == g_ui.selected) ? RGB(255, 255, 255) : RGB(0, 0, 0));
                    DrawTextW(hdc, g_ui.items[idx].c_str(), (int)g_ui.items[idx].size(), &it, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                    // draw index number
                    RECT numRc = { left + 4, top + 4, left + 24, top + 20 };
                    std::wstringstream ss;
                    ss << (i + 1);
                    SetTextColor(hdc, RGB(100, 100, 100));
                    DrawTextW(hdc, ss.str().c_str(), (int)ss.str().size(), &numRc, DT_LEFT | DT_TOP | DT_SINGLELINE);
                }

                // draw page indicator
                int totalPages = (int)((itemCount + per - 1) / per);
                std::wstringstream pss;
                pss << L"Page " << (g_ui.pageIndex + 1) << L"/" << (std::max)(1, totalPages);
                RECT pageRc = { rc.left + 10, 44 + itemH + 8 + 8, rc.right - 10, 44 + itemH + 8 + 24 };
                SetTextColor(hdc, RGB(80, 80, 80));
                DrawTextW(hdc, pss.str().c_str(), (int)pss.str().size(), &pageRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                SelectObject(hdc, hOld);
                DeleteObject(hFont);
            }

            EndPaint(hwnd, &ps);
        }
        return 0;

    case WM_DESTROY:
        if (g_hwndSettingsMenu) {
            DestroyWindow(g_hwndSettingsMenu);
            g_hwndSettingsMenu = NULL;
        }
        if (g_hwndSchemeReference) {
            DestroyWindow(g_hwndSchemeReference);
            g_hwndSchemeReference = NULL;
        }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int nCmdShow)
{
    // Initialize GDI+
    GdiplusStartupInput gdiSI;
    GdiplusStartup(&g_gdiplusToken, &gdiSI, nullptr);

    // Load icons
    g_icons.LoadAll();

    WNDCLASSW wc = { };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"ScrIPA",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 700, 170,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (!hwnd) {
        GdiplusShutdown(g_gdiplusToken);
        return 0;
    }

    ShowWindow(hwnd, nCmdShow);

    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    // Cleanup
    g_icons.UnloadAll();
    GdiplusShutdown(g_gdiplusToken);

    return 0;
}
