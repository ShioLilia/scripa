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

static const wchar_t CLASS_NAME[] = L"ScripaCandidateDemo";

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

    void LoadAll() {
        bcg = new Bitmap(L"uicon\\bcg.png");
        typing_bg = new Bitmap(L"uicon\\typing_bg.png");
        user = new Bitmap(L"uicon\\user.png");
        default_icon = new Bitmap(L"uicon\\default.png");
        eng = new Bitmap(L"uicon\\ENG.png");
        ipa = new Bitmap(L"uicon\\IPA.png");
        left = new Bitmap(L"uicon\\left.png");
        right = new Bitmap(L"uicon\\right.png");
        settings = new Bitmap(L"uicon\\settings.png");
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

// Button rects (cached)
RECT g_btnDefault = {0, 0, 0, 0};
RECT g_btnUser = {0, 0, 0, 0};
RECT g_btnMode = {0, 0, 0, 0};
RECT g_btnLeft = {0, 0, 0, 0};
RECT g_btnRight = {0, 0, 0, 0};
RECT g_btnSettings = {0, 0, 0, 0};

// 字库选择对话框
void ShowSchemeSelectionDialog(HWND hwndParent) {
    // 获取所有可用字库
    auto available = g_backend.GetAvailableSchemes();
    
    if (available.empty()) {
        MessageBoxW(hwndParent, L"No scheme files found in schemes/ directory.", L"Scheme Selection", MB_OK | MB_ICONINFORMATION);
        return;
    }
    
    // 构建消息文本
    std::wstring msg = L"Available Schemes (check to enable):\n\n";
    for (size_t i = 0; i < available.size(); ++i) {
        bool enabled = g_backend.IsSchemeEnabled(available[i]);
        msg += std::to_wstring(i + 1) + L". ";
        msg += enabled ? L"[✓] " : L"[ ] ";
        
        // 转换为 wstring
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
        msg += conv.from_bytes(available[i]) + L"\n";
    }
    
    msg += L"\nCurrently Enabled: ";
    auto enabled_list = g_backend.GetEnabledSchemes();
    if (enabled_list.empty()) {
        msg += L"None";
    } else {
        for (size_t i = 0; i < enabled_list.size(); ++i) {
            if (i > 0) msg += L", ";
            std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
            msg += conv.from_bytes(enabled_list[i]);
        }
    }
    
    msg += L"\n\nEnter scheme number to toggle, or 'R' to reload schemes:";
    
    // 显示输入框（简化版，实际应该用对话框）
    // 这里使用简单的 MessageBox 作为演示
    int result = MessageBoxW(hwndParent, msg.c_str(), L"Scheme Selection - Settings", 
                             MB_OKCANCEL | MB_ICONINFORMATION);
    
    if (result == IDOK) {
        // 实际项目中应该使用自定义对话框
        // 这里只是演示，暂时弹窗说明功能已集成
        MessageBoxW(hwndParent, 
            L"Scheme selection UI integrated!\n\n"
            L"To toggle schemes:\n"
            L"1. Use g_backend.EnableScheme(\"custom\")\n"
            L"2. Use g_backend.DisableScheme(\"simple\")\n"
            L"3. Call g_backend.ReloadSchemes()\n\n"
            L"Full UI dialog can be implemented with custom window + checkboxes.",
            L"Info", MB_OK);
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_CREATE:
        {
            g_backend.Init();
            g_ui.modeIPA = g_backend.GetMode();  // sync with backend
            g_ui.composition = g_backend.GetComposition();
            g_ui.items = g_backend.GetCandidates();
            if (g_ui.items.empty()) {
                g_ui.items = { L"tʃ", L"θ", L"ɡ", L"ŋ", L"ʃ", L"ð", L"æ", L"ɔ", L"ə" };
            }
            g_ui.selected = 0;
            g_ui.pageIndex = 0;
            g_ui.visible = true;
        }
        return 0;


    case WM_KEYDOWN:
        // Backspace in composition mode: clear last character
        if (wParam == VK_BACK) {
            // This should be handled in WM_CHAR, but VK_BACK doesn't generate WM_CHAR
            // So we handle it here: remove last char from composition via backend
            // For now, just clear the whole buffer (can refine later)
            g_ui.composition = L"";
            g_ui.items.clear();
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
                std::wstringstream ss;
                ss << L"Selected #" << (idx + 1) << L": " << g_ui.items[idx];
                MessageBoxW(hwnd, ss.str().c_str(), L"Scripa Demo", MB_OK);
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

            // numeric quick-select for current page
            if (ch >= L'1' && ch <= L'0' + g_ui.itemsPerPage) {
                int num = (int)(ch - L'1');
                int idx = g_ui.pageIndex * g_ui.itemsPerPage + num;
                if (idx >= 0 && idx < (int)g_ui.items.size()) {
                    std::wstringstream ss;
                    ss << L"Selected #" << (idx + 1) << L": " << g_ui.items[idx];
                    MessageBoxW(hwnd, ss.str().c_str(), L"Scripa Demo", MB_OK);
                }
                return 0;
            }

            // forward character to backend and refresh UI
            bool handled = g_backend.OnKeyDown(ch);
            g_ui.composition = g_backend.GetComposition();
            g_ui.items = g_backend.GetCandidates();
            if (g_ui.items.empty()) {
                g_ui.items = { L"" };
            }
            // reset to first page on new composition
            int totalPages = (int)((g_ui.items.size() + g_ui.itemsPerPage - 1) / g_ui.itemsPerPage);
            if (g_ui.pageIndex >= totalPages) g_ui.pageIndex = std::max(0, totalPages - 1);
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
                ShellExecuteW(NULL, L"open", L"uicon\\..\\schemes\\default.txt", NULL, NULL, SW_SHOW);
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
                // 显示字库选择对话框
                ShowSchemeSelectionDialog(hwnd);
                return 0;
            }

            // check candidate items
            {
                RECT rc;
                GetClientRect(hwnd, &rc);
                int itemCount = (int)g_ui.items.size();
                int per = g_ui.itemsPerPage;
                int start = g_ui.pageIndex * per;
                int shown = std::min(per, std::max(0, itemCount - start));
                int padding = 8;
                int itemW = std::max(48, (rc.right - rc.left - padding * shown) / std::max(1, shown));
                int itemH = 36;
                for (int i = 0; i < shown; ++i) {
                    int idx = start + i;
                    int left = padding + i * (itemW + padding);
                    int top = 60;
                    RECT it = { left, top, left + itemW, top + itemH };
                    if (PtInRect(&it, POINT{x, y})) {
                        g_ui.selected = i;  // store page-relative index
                        std::wstringstream ss;
                        ss << L"Selected #" << (idx + 1) << L": " << g_ui.items[idx];
                        MessageBoxW(hwnd, ss.str().c_str(), L"Scripa Demo", MB_OK);
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
                int shown = std::min(per, std::max(0, itemCount - start));
                int padding = 8;
                int itemW = std::max(48, (rc.right - rc.left - padding * shown) / std::max(1, shown));
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
                pss << L"Page " << (g_ui.pageIndex + 1) << L"/" << std::max(1, totalPages);
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

    WNDCLASS wc = { };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"Scripa Candidate UI",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 700, 180,
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
