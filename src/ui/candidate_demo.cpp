#include <windows.h>
#include <vector>
#include <string>
#include <sstream>
#include "../tsf/ScripaTSF.h"

static const wchar_t CLASS_NAME[] = L"ScripaCandidateDemo";

struct CandidateUI {
    std::vector<std::wstring> items;
    int selected = 0;
    std::wstring composition = L"th"; // demo composition buffer
    int pageIndex = 0;
    int itemsPerPage = 8;
};

static CandidateUI g_ui;
static ScripaTSF g_backend;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_CREATE:
        {
            // initialize backend and fetch candidates
            g_backend.Init();
            g_ui.composition = g_backend.GetComposition();
            g_ui.items = g_backend.GetCandidates();
            if (g_ui.items.empty()) {
                // fallback sample
                g_ui.items = { L"tʃ", L"θ", L"ɡ", L"ŋ", L"ʃ", L"ð", L"æ", L"ɔ", L"ə" };
            }
            g_ui.selected = 0;
            g_ui.pageIndex = 0;
        }
        return 0;
    case WM_KEYDOWN:
        // keep navigation keys in WM_KEYDOWN for immediate behavior
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
            std::wstring msg = L"Selected: " + g_ui.items[g_ui.selected];
            MessageBoxW(hwnd, msg.c_str(), L"Scripa Demo", MB_OK);
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
            // page navigation with [ and ]
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

            // numeric quick-select for current page (1..itemsPerPage)
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

            // forward character input to backend and refresh UI
            g_backend.OnKeyDown(ch);
            g_ui.composition = g_backend.GetComposition();
            g_ui.items = g_backend.GetCandidates();
            if (g_ui.items.empty()) {
                // keep at least one placeholder
                g_ui.items = { L"" };
            }
            // make sure current page is still valid
            int totalPages = (int)((g_ui.items.size() + g_ui.itemsPerPage - 1) / g_ui.itemsPerPage);
            if (g_ui.pageIndex >= totalPages) g_ui.pageIndex = max(0, totalPages - 1);
            InvalidateRect(hwnd, NULL, TRUE);
            return 0;
        }
    case WM_LBUTTONDOWN:
        {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            // compute item rects
            RECT rc;
            GetClientRect(hwnd, &rc);
            int padding = 8;
            int itemCount = (int)g_ui.items.size();
            int per = g_ui.itemsPerPage;
            int start = g_ui.pageIndex * per;
            int shown = min(per, max(0, itemCount - start));
            int itemW = max(48, (rc.right - rc.left - padding * shown) / max(1, shown));
            int itemH = 36;
            // prev/next button rects
            RECT prevBtn = {8, 44 + itemH + 8, 48, 44 + itemH + 8 + 24};
            RECT nextBtn = {rc.right - 56, 44 + itemH + 8, rc.right - 8, 44 + itemH + 8 + 24};
            // check buttons
            if (x >= prevBtn.left && x <= prevBtn.right && y >= prevBtn.top && y <= prevBtn.bottom) {
                if (g_ui.pageIndex > 0) g_ui.pageIndex--;
                InvalidateRect(hwnd, NULL, TRUE);
                return 0;
            }
            if (x >= nextBtn.left && x <= nextBtn.right && y >= nextBtn.top && y <= nextBtn.bottom) {
                int totalPages = (int)((itemCount + per - 1) / per);
                if (g_ui.pageIndex + 1 < totalPages) g_ui.pageIndex++;
                InvalidateRect(hwnd, NULL, TRUE);
                return 0;
            }

            for (int i = 0; i < shown; ++i) {
                int left = padding + i * (itemW + padding);
                int top = 40;
                RECT it = { left, top, left + itemW, top + itemH };
                if (x >= it.left && x <= it.right && y >= it.top && y <= it.bottom) {
                    int idx = start + i;
                    g_ui.selected = idx;
                    InvalidateRect(hwnd, NULL, TRUE);
                    std::wstringstream ss;
                    ss << L"Selected #" << (idx + 1) << L": " << g_ui.items[idx];
                    MessageBoxW(hwnd, ss.str().c_str(), L"Scripa Demo", MB_OK);
                    break;
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

            // background
            FillRect(hdc, &rc, (HBRUSH)(COLOR_WINDOW+1));

            // composition text
            HFONT hFontOld = NULL;
            HFONT hFont = CreateFontW(18,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
            hFontOld = (HFONT)SelectObject(hdc, hFont);
            SetTextColor(hdc, RGB(32,32,32));
            SetBkMode(hdc, TRANSPARENT);
            RECT compRc = {8,8, rc.right-8, 36};
            std::wstring comp = L"Buffer: " + g_ui.composition;
            DrawTextW(hdc, comp.c_str(), (int)comp.size(), &compRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

            // draw candidates
            int padding = 8;
            int itemCount = (int)g_ui.items.size();
            int per = g_ui.itemsPerPage;
            int start = g_ui.pageIndex * per;
            int shown = min(per, max(0, itemCount - start));
            int itemW = max(48, (rc.right - rc.left - padding * shown) / max(1, shown));
            int itemH = 36;
            for (int i = 0; i < shown; ++i) {
                int idx = start + i;
                int left = padding + i * (itemW + padding);
                int top = 40;
                RECT it = { left, top, left + itemW, top + itemH };
                // background (white bg overall; candidate highlight blue)
                HBRUSH hbr = CreateSolidBrush(idx == g_ui.selected ? RGB(0,120,215) : RGB(255,255,255));
                FillRect(hdc, &it, hbr);
                // border
                FrameRect(hdc, &it, (HBRUSH)GetStockObject(BLACK_BRUSH));
                // text
                SetTextColor(hdc, idx == g_ui.selected ? RGB(255,255,255) : RGB(0,0,0));
                DrawTextW(hdc, g_ui.items[idx].c_str(), (int)g_ui.items[idx].size(), &it, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                DeleteObject(hbr);
                // draw index number at top-left of item
                RECT numRc = { left + 4, top + 4, left + 24, top + 20 };
                std::wstringstream ss;
                ss << (i + 1);
                SetTextColor(hdc, RGB(100,100,100));
                DrawTextW(hdc, ss.str().c_str(), (int)ss.str().size(), &numRc, DT_LEFT | DT_TOP | DT_SINGLELINE);
            }

            // draw prev/next buttons and page indicator
            RECT prevBtn = {8, 44 + itemH + 8, 48, 44 + itemH + 8 + 24};
            RECT nextBtn = {rc.right - 56, 44 + itemH + 8, rc.right - 8, 44 + itemH + 8 + 24};
            HBRUSH hPrev = CreateSolidBrush(RGB(230,230,230));
            HBRUSH hNext = CreateSolidBrush(RGB(230,230,230));
            FillRect(hdc, &prevBtn, hPrev);
            FillRect(hdc, &nextBtn, hNext);
            FrameRect(hdc, &prevBtn, (HBRUSH)GetStockObject(BLACK_BRUSH));
            FrameRect(hdc, &nextBtn, (HBRUSH)GetStockObject(BLACK_BRUSH));
            DrawTextW(hdc, L"<", 1, &prevBtn, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            DrawTextW(hdc, L">", 1, &nextBtn, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            DeleteObject(hPrev);
            DeleteObject(hNext);

            int totalPages = (int)((itemCount + per - 1) / per);
            std::wstringstream pss;
            pss << L"Page " << (g_ui.pageIndex + 1) << L"/" << max(1, totalPages);
            RECT pageRc = { rc.left + 60, 44 + itemH + 8, rc.right - 60, 44 + itemH + 8 + 24 };
            SetTextColor(hdc, RGB(80,80,80));
            DrawTextW(hdc, pss.str().c_str(), (int)pss.str().size(), &pageRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            SelectObject(hdc, hFontOld);
            DeleteObject(hFont);
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
    WNDCLASS wc = { };
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        0,
        CLASS_NAME,
        L"Scripa Candidate Demo",
        WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 520, 160,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    if (!hwnd) return 0;

    ShowWindow(hwnd, nCmdShow);

    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}
