#pragma once

#include <windows.h>
#include <vector>
#include <string>
#include <gdiplus.h>

class CTextService;

// Candidate Window - displays IPA conversion candidates
class CCandidateWindow
{
public:
    CCandidateWindow(CTextService* pTextService);
    ~CCandidateWindow();

    BOOL Create();
    void Destroy();
    void Show(BOOL bShow);
    void Move(int x, int y);
    void Update(const std::vector<std::wstring>& candidates, int selected);
    
    HWND GetWnd() { return _hwnd; }
    
private:
    static LRESULT CALLBACK _WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void _OnPaint(HDC hdc);
    void _OnLButtonDown(int x, int y);
    
    CTextService* _pTextService;
    HWND _hwnd;
    std::vector<std::wstring> _candidates;
    int _selectedIndex;
    
    // UI state
    std::wstring _composition;
    int _pageIndex;
    int _itemsPerPage;
};

// Toolbar - shows mode indicator and settings
class CToolbar
{
public:
    CToolbar(CTextService* pTextService);
    ~CToolbar();

    BOOL Create();
    void Destroy();
    void Show(BOOL bShow);
    void Move(int x, int y);
    void UpdateMode(BOOL bIPA);
    
    HWND GetWnd() { return _hwnd; }
    
private:
    static LRESULT CALLBACK _WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    void _OnPaint(HDC hdc);
    void _OnLButtonDown(int x, int y);
    void _LoadIcons();
    void _ShowSettingsMenu();
    void _ShowSchemeReference();
    
    CTextService* _pTextService;
    HWND _hwnd;
    HWND _hwndSettingsMenu;
    HWND _hwndSchemeReference;
    BOOL _bIPAMode;
    
    // GDI+ resources
    Gdiplus::Bitmap* _iconENG;
    Gdiplus::Bitmap* _iconIPA;
    Gdiplus::Bitmap* _iconSettings;
    Gdiplus::Bitmap* _iconDefault;
    
    // Button rects
    RECT _rectMode;
    RECT _rectSettings;
    RECT _rectDefault;
};

// Settings Menu Window
LRESULT CALLBACK SettingsMenuProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Scheme Reference Window
LRESULT CALLBACK SchemeReferenceWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
