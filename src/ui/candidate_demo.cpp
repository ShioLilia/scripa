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
#include <map>
#include <set>
#include <fstream>
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
    bool darkMode = false;  // 夜间模式
    bool chordMode = false;  // 和弦模式（在Other Settings中启用）
    
    // Chord mode specific
    std::vector<std::wstring> chordSequence;  // 按键序列（C, C#, D...）
    std::wstring chordResult;  // 和弦名称结果（如Cmaj）
};

static CandidateUI g_ui;
static ScripaTSF g_backend;

// Saved schemes state before entering chord mode
static std::vector<std::string> g_savedEnabledSchemes;

// Helper: Convert note name to semitone value (C=0, C#=1, D=2, etc.)
int GetSemitone(const std::wstring& note) {
    if (note.empty()) return -1;
    
    wchar_t base = note[0];
    int semitone = -1;
    
    // Map base note to semitone
    switch (base) {
        case L'C': semitone = 0; break;
        case L'D': semitone = 2; break;
        case L'E': semitone = 4; break;
        case L'F': semitone = 5; break;
        case L'G': semitone = 7; break;
        case L'A': semitone = 9; break;
        case L'B': semitone = 11; break;
        default: return -1;
    }
    
    // Check for sharp (#) or flat (b)
    if (note.length() > 1) {
        if (note[1] == L'#') {
            semitone = (semitone + 1) % 12;
        } else if (note[1] == L'b') {
            semitone = (semitone - 1 + 12) % 12;
        }
    }
    
    return semitone;
}

// Helper: Calculate intervals from root note
std::vector<int> CalculateIntervals(const std::vector<std::wstring>& notes) {
    std::vector<int> intervals;
    if (notes.empty()) return intervals;
    
    int root = GetSemitone(notes[0]);
    if (root < 0) return intervals;
    
    intervals.push_back(0);  // Root is always 0
    int previousSemitone = root;
    int octaveOffset = 0;
    
    // Calculate intervals for remaining notes
    for (size_t i = 1; i < notes.size(); ++i) {
        const auto& note = notes[i];
        int semitone = GetSemitone(note);
        if (semitone < 0) continue;
        
        // If current note's semitone <= previous note's semitone, it's in next octave
        if (semitone <= previousSemitone) {
            octaveOffset += 12;
        }
        
        // Calculate interval from root with octave offset
        int interval = (semitone - root + octaveOffset);
        
        intervals.push_back(interval);
        previousSemitone = semitone;
    }
    
    return intervals;
}

// Helper: Identify chord type from intervals
std::wstring IdentifyChordType(const std::vector<int>& intervals) {
    if (intervals.empty()) return L"";
    
    // Sort intervals and create a pattern
    std::vector<int> sorted = intervals;
    std::sort(sorted.begin(), sorted.end());
    
    // Remove duplicates and get unique intervals within first octave
    std::set<int> uniqueIntervals;
    for (int interval : sorted) {
        uniqueIntervals.insert(interval % 12);
    }
    
    // Convert to vector for pattern matching
    std::vector<int> pattern(uniqueIntervals.begin(), uniqueIntervals.end());
    
    // Match common chord patterns (intervals from root)
    // Major triad: 0, 4, 7
    if (pattern == std::vector<int>{0, 4, 7}) return L"maj";
    
    // Minor triad: 0, 3, 7
    if (pattern == std::vector<int>{0, 3, 7}) return L"min";
    
    // Suspended 2nd: 0, 2, 7
    if (pattern == std::vector<int>{0, 2, 7}) return L"sus2";
    
    // Suspended 4th: 0, 5, 7
    if (pattern == std::vector<int>{0, 5, 7}) return L"sus4";
    
    // Diminished: 0, 3, 6
    if (pattern == std::vector<int>{0, 3, 6}) return L"dim";
    
    // Augmented: 0, 4, 8
    if (pattern == std::vector<int>{0, 4, 8}) return L"aug";
    
    // Power chord (5th): 0, 7
    if (pattern == std::vector<int>{0, 7}) return L"5";
    /////////////////////////////////////////
    
    // Dominant 7th: 0, 4, 7, 10
    if (pattern == std::vector<int>{0, 4, 7, 10}) return L"7";
    
    // Major 7th: 0, 4, 7, 11
    if (pattern == std::vector<int>{0, 4, 7, 11}) return L"M7";
    
    // Minor 7th: 0, 3, 7, 10
    if (pattern == std::vector<int>{0, 3, 7, 10}) return L"m7";
    
    // Minor major 7th: 0, 3, 7, 11
    if (pattern == std::vector<int>{0, 3, 7, 11}) return L"mM7";
    
    // Major 6th: 0, 4, 7, 9
    if (pattern == std::vector<int>{0, 4, 7, 9}) return L"6";
    
    // Minor 6th: 0, 3, 7, 9
    if (pattern == std::vector<int>{0, 3, 7, 9}) return L"min6";
    /////////////////////////////////////////
    
    // Dominant 9th: 0, 4, 7, 10, 13 
    if (pattern == std::vector<int>{0, 4, 7, 10, 13}) return L"7b9";
    
    // Major 9th: 0, 4, 7, 11, 13 
    if (pattern == std::vector<int>{0, 4, 7, 11, 13}) return L"M7b9";
    
    // Minor 9th: 0, 3, 7, 10, 13
    if (pattern == std::vector<int>{0, 3, 7, 10, 13}) return L"m7b9";
    
    // Add9: 0, 4, 7, 13
    if (pattern == std::vector<int>{0, 4, 7, 13}) return L"addb9";
 /////////////////////////////////////////   

    // Dominant 9th: 0, 4, 7, 10, 14 
    if (pattern == std::vector<int>{0, 4, 7, 10, 14}) return L"9";
    
    // Major 9th: 0, 4, 7, 11, 14
    if (pattern == std::vector<int>{0, 4, 7, 11, 14}) return L"M9";
    
    // Minor 9th: 0, 3, 7, 10, 14
    if (pattern == std::vector<int>{0, 3, 7, 10, 14}) return L"m9";
    
    // Add9: 0, 4, 7, 14 
    if (pattern == std::vector<int>{0, 4, 7, 14}) return L"add9";
/////////////////////////////////////////

    // Half-diminished 7th: 0, 3, 6, 10
    if (pattern == std::vector<int>{0, 3, 6, 10}) return L"m7b5";
    
    // Diminished 7th: 0, 3, 6, 9
    if (pattern == std::vector<int>{0, 3, 6, 9}) return L"dim7";
    
    // If no match, return empty
    return L"";
}

// Helper: Detect inversion and return new root index and suffix
std::pair<int, std::wstring> DetectInversion(const std::vector<std::wstring>& notes, const std::vector<int>& intervals) {
    if (notes.size() < 3 || intervals.size() < 3) {
        return {0, L""};
    }
    
    // Check for 6th interval (8 or 9 semitones) - indicates 1st inversion
    for (size_t i = 1; i < intervals.size(); ++i) {
        int interval = intervals[i] % 12;
        if (interval == 8 || interval == 9) {
            // This is likely the root in 1st inversion
            return {(int)i, L"/1st"};
        }
    }
    
    // Check for 4th interval (5 semitones) - indicates 2nd inversion
    for (size_t i = 1; i < intervals.size(); ++i) {
        int interval = intervals[i] % 12;
        if (interval == 5) {
            // This might be the root in 2nd inversion
            return {(int)i, L"/2nd"};
        }
    }
    
    // Check for 2nd interval (2 semitones) - less common
    for (size_t i = 1; i < intervals.size(); ++i) {
        int interval = intervals[i] % 12;
        if (interval == 2) {
            return {(int)i, L"/sus2"};
        }
    }
    
    return {0, L""};
}

// Helper: Match chord names using interval calculation algorithm
std::vector<std::wstring> MatchChordNames(const std::vector<std::wstring>& chordSequence) {
    std::vector<std::wstring> results;
    
    if (chordSequence.empty()) {
        return results;
    }
    
    // Special case: single note
    if (chordSequence.size() == 1) {
        results.push_back(chordSequence[0]);
        return results;
    }
    
    // Calculate intervals from the first note (assumed root)
    std::vector<int> intervals = CalculateIntervals(chordSequence);
    
    if (intervals.empty()) {
        return results;
    }
    
    // Try to identify the chord in root position first
    std::wstring rootNote = chordSequence[0];
    std::wstring chordType = IdentifyChordType(intervals);
    
    if (!chordType.empty()) {
        // Found chord in root position
        results.push_back(rootNote + chordType);
    }
    
    // Check for inversions
    auto [inversionIndex, inversionSuffix] = DetectInversion(chordSequence, intervals);
    if (inversionIndex > 0 && inversionIndex < (int)chordSequence.size()) {
        // Recalculate with new root
        std::vector<std::wstring> reorderedNotes;
        for (size_t i = inversionIndex; i < chordSequence.size(); ++i) {
            reorderedNotes.push_back(chordSequence[i]);
        }
        for (size_t i = 0; i < inversionIndex; ++i) {
            reorderedNotes.push_back(chordSequence[i]);
        }
        
        std::vector<int> newIntervals = CalculateIntervals(reorderedNotes);
        std::wstring newChordType = IdentifyChordType(newIntervals);
        
        if (!newChordType.empty()) {
            // Found inverted chord
            std::wstring inversionName = reorderedNotes[0] + newChordType;
            inversionName += L"/" + chordSequence[0];  // Add bass note
            results.push_back(inversionName);
        }
    }
    
    // If no chord identified, just show the notes
    if (results.empty()) {
        std::wstring noteList;
        for (const auto& note : chordSequence) {
            if (!noteList.empty()) noteList += L"+";
            noteList += note;
        }
        results.push_back(noteList);
    }
    
    return results;
}

// Helper: Update chord results after sequence changes
void UpdateChordResults(HWND hwnd) {
    // Build display string: "C+E+G"
    std::wstring newResult = L"";
    for (size_t i = 0; i < g_ui.chordSequence.size(); ++i) {
        if (i > 0) newResult += L"+";
        newResult += g_ui.chordSequence[i];
    }
    
    // Get chord name candidates
    auto chordNames = MatchChordNames(g_ui.chordSequence);
    
    // Update candidate items
    std::vector<std::wstring> newItems;
    if (chordNames.empty()) {
        newItems = {L""};
    } else {
        newItems = chordNames;
    }
    
    // Only redraw if something actually changed
    bool changed = (newResult != g_ui.chordResult) || (newItems != g_ui.items);
    
    g_ui.chordResult = newResult;
    g_ui.items = newItems;
    g_ui.selected = 0;
    g_ui.pageIndex = 0;
    
    if (changed) {
        InvalidateRect(hwnd, NULL, FALSE);
    }
}

// Configuration file path
static const wchar_t* CONFIG_FILE = L"..\\scripa_config.ini";

// Load configuration from file
void LoadConfig() {
    std::wifstream file(CONFIG_FILE);
    if (!file.is_open()) {
        // Use default values if config file doesn't exist
        return;
    }
    
    std::wstring line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == L'#') continue;
        
        size_t pos = line.find(L'=');
        if (pos == std::wstring::npos) continue;
        
        std::wstring key = line.substr(0, pos);
        std::wstring value = line.substr(pos + 1);
        
        // Trim whitespace
        key.erase(0, key.find_first_not_of(L" \t"));
        key.erase(key.find_last_not_of(L" \t") + 1);
        value.erase(0, value.find_first_not_of(L" \t"));
        value.erase(value.find_last_not_of(L" \t") + 1);
        
        if (key == L"itemsPerPage") {
            int val = _wtoi(value.c_str());
            if (val >= 4 && val <= 10) {
                g_ui.itemsPerPage = val;
            }
        } else if (key == L"darkMode") {
            g_ui.darkMode = (value == L"1" || value == L"true");
        } else if (key == L"chordMode") {
            g_ui.chordMode = (value == L"1" || value == L"true");
        } else if (key == L"enabledSchemes") {
            // Parse comma-separated scheme list
            std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
            std::string schemes = conv.to_bytes(value);
            
            // First disable all schemes
            auto allSchemes = g_backend.GetAvailableSchemes();
            for (const auto& scheme : allSchemes) {
                g_backend.DisableScheme(scheme);
            }
            
            // Then enable listed schemes (but skip chord if not in chord mode)
            size_t start = 0;
            while (start < schemes.length()) {
                size_t end = schemes.find(',', start);
                if (end == std::string::npos) end = schemes.length();
                
                std::string scheme = schemes.substr(start, end - start);
                // Trim
                scheme.erase(0, scheme.find_first_not_of(" \t"));
                scheme.erase(scheme.find_last_not_of(" \t") + 1);
                
                // Skip chord and chord2 schemes - they will be enabled later if chordMode is true
                if (!scheme.empty() && scheme != "chord" && scheme != "chord2") {
                    g_backend.EnableScheme(scheme);
                    // Save IPA schemes for later restoration when exiting chord mode
                    g_savedEnabledSchemes.push_back(scheme);
                }
                
                start = end + 1;
            }
        }
    }
    
    file.close();
    
    // If chord mode is enabled, enable chord and chord2 schemes
    if (g_ui.chordMode) {
        g_backend.EnableScheme("chord");
        g_backend.EnableScheme("chord2");
        // g_savedEnabledSchemes now contains the IPA mode schemes
    } else {
        // Not in chord mode, clear saved schemes
        g_savedEnabledSchemes.clear();
    }
}

// Save configuration to file
void SaveConfig() {
    std::wofstream file(CONFIG_FILE);
    if (!file.is_open()) {
        return;
    }
    
    file << L"# ScrIPA Configuration File\n";
    file << L"# This file is automatically generated\n\n";
    
    file << L"itemsPerPage=" << g_ui.itemsPerPage << L"\n";
    file << L"darkMode=" << (g_ui.darkMode ? L"1" : L"0") << L"\n";
    file << L"chordMode=" << (g_ui.chordMode ? L"1" : L"0") << L"\n";
    
    // Save enabled schemes (exclude chord and chord2 - they're managed by chordMode)
    auto allSchemes = g_backend.GetAvailableSchemes();
    std::wstring enabledSchemes;
    for (const auto& scheme : allSchemes) {
        if (scheme != "chord" && scheme != "chord2" && g_backend.IsSchemeEnabled(scheme)) {
            std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
            if (!enabledSchemes.empty()) enabledSchemes += L",";
            enabledSchemes += conv.from_bytes(scheme);
        }
    }
    file << L"enabledSchemes=" << enabledSchemes << L"\n";
    
    file.close();
}

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

// Piano keyboard buttons for chord mode (12 keys)
// White keys: C D E F G A B (7 keys)
// Black keys: C# D# F# G# A# (5 keys)
struct PianoKey {
    std::wstring note;
    RECT rect;
    bool isBlack;
};

static std::vector<PianoKey> g_pianoKeys;

// Settings menu popup window
static HWND g_hwndSettingsMenu = NULL;
static HWND g_hwndParentMain = NULL;

// Scheme reference popup window
static HWND g_hwndSchemeReference = NULL;

// About dialog window
static HWND g_hwndAboutDialog = NULL;

// Scheme reference images
struct SchemeReferenceData {
    Bitmap* cons1 = nullptr;
    Bitmap* cons2 = nullptr;
    Bitmap* cons3 = nullptr;
    Bitmap* vowel = nullptr;
    Bitmap* elseAndTones = nullptr;
    int currentView = 0;  // 0=C1, 1=C2(both cons2&cons3), 2=V, 3=Else
    
    void LoadAll() {
        cons1 = new Bitmap(L"..\\src\\dictionary\\cons1.png");
        cons2 = new Bitmap(L"..\\src\\dictionary\\cons2.png");
        cons3 = new Bitmap(L"..\\src\\dictionary\\cons3.png");
        vowel = new Bitmap(L"..\\src\\dictionary\\vowel.png");
        elseAndTones = new Bitmap(L"..\\src\\dictionary\\else and tones.png");
    }
    
    void UnloadAll() {
        if (cons1) { delete cons1; cons1 = nullptr; }
        if (cons2) { delete cons2; cons2 = nullptr; }
        if (cons3) { delete cons3; cons3 = nullptr; }
        if (vowel) { delete vowel; vowel = nullptr; }
        if (elseAndTones) { delete elseAndTones; elseAndTones = nullptr; }
    }
};

static SchemeReferenceData g_schemeRefData;

struct SettingsMenuItem {
    std::wstring text;
    Bitmap* icon;
    int id;
};

static std::vector<SettingsMenuItem> g_menuItems;

// Scheme selection dialog with checkboxes
struct SchemeGroup {
    std::string name;
    std::vector<std::string> schemes;
    bool expanded = false;  // 默认收起
    HWND checkboxGroup = NULL;
    HWND buttonExpand = NULL;
    std::vector<HWND> checkboxes;
};

static std::vector<SchemeGroup> g_schemeGroups;
static std::vector<std::string> g_standaloneSchemes;  // custom等独立scheme

// Forward declarations
void ShowAboutDialog(HWND hwndParent);

// Other Settings Dialog Procedure
LRESULT CALLBACK OtherSettingsDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static HWND hComboItemsPerPage = NULL;
    static HWND hCheckChordMode = NULL;
    
    switch (msg) {
    case WM_INITDIALOG:
    {
        {
        // Create label for items per page
        CreateWindowW(L"STATIC", L"Words per page (4-10):",
            WS_CHILD | WS_VISIBLE | SS_LEFT,
            20, 20, 200, 20,
            hwndDlg, NULL, GetModuleHandle(NULL), NULL);
        
        // Create combo box
        hComboItemsPerPage = CreateWindowW(L"COMBOBOX", NULL,
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            20, 45, 100, 200,
            hwndDlg, (HMENU)1001, GetModuleHandle(NULL), NULL);
        
        // Populate combo box with values 4-10
        for (int i = 4; i <= 10; ++i) {
            wchar_t buf[8];
            swprintf(buf, 8, L"%d", i);
            SendMessageW(hComboItemsPerPage, CB_ADDSTRING, 0, (LPARAM)buf);
            if (i == g_ui.itemsPerPage) {
                SendMessageW(hComboItemsPerPage, CB_SETCURSEL, i - 4, 0);
            }
        }
        
        // Create Chord Mode checkbox
        hCheckChordMode = CreateWindowW(L"BUTTON", L"Enable Chord Mode (piano keyboard)",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            20, 85, 300, 20,
            hwndDlg, (HMENU)1002, GetModuleHandle(NULL), NULL);
        
        SendMessageW(hCheckChordMode, BM_SETCHECK, g_ui.chordMode ? BST_CHECKED : BST_UNCHECKED, 0);
        
        // Create Apply and Cancel buttons
        CreateWindowW(L"BUTTON", L"Apply",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            80, 120, 80, 30,
            hwndDlg, (HMENU)IDOK, GetModuleHandle(NULL), NULL);
        
        CreateWindowW(L"BUTTON", L"Cancel",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            180, 120, 80, 30,
            hwndDlg, (HMENU)IDCANCEL, GetModuleHandle(NULL), NULL);
        
        return TRUE;
        }
    }
    
    case WM_COMMAND:
    {
        {
        int id = LOWORD(wParam);
        
        if (id == IDOK) {
            // Get selected items per page value
            int sel = (int)SendMessageW(hComboItemsPerPage, CB_GETCURSEL, 0, 0);
            if (sel != CB_ERR) {
                int newItemsPerPage = 4 + sel;
                g_ui.itemsPerPage = newItemsPerPage;
                
                // Reset page index if needed
                int itemCount = (int)g_ui.items.size();
                int totalPages = (itemCount + newItemsPerPage - 1) / newItemsPerPage;
                if (g_ui.pageIndex >= totalPages) {
                    g_ui.pageIndex = (std::max)(0, totalPages - 1);
                }
            }
            
            // Get chord mode checkbox state
            LRESULT chordChecked = SendMessageW(hCheckChordMode, BM_GETCHECK, 0, 0);
            bool wasChordMode = g_ui.chordMode;
            g_ui.chordMode = (chordChecked == BST_CHECKED);
            
            // Manage schemes based on chord mode
            if (g_ui.chordMode && !wasChordMode) {
                // Entering chord mode
                // 1. Save current enabled schemes
                g_savedEnabledSchemes.clear();
                auto allSchemes = g_backend.GetAvailableSchemes();
                for (const auto& scheme : allSchemes) {
                    if (scheme != "chord" && scheme != "chord2" && g_backend.IsSchemeEnabled(scheme)) {
                        g_savedEnabledSchemes.push_back(scheme);
                    }
                }
                
                // 2. Disable all schemes except chord and chord2
                for (const auto& scheme : allSchemes) {
                    if (scheme != "chord" && scheme != "chord2") {
                        g_backend.DisableScheme(scheme);
                    }
                }
                
                // 3. Enable chord and chord2 schemes
                g_backend.EnableScheme("chord");
                g_backend.EnableScheme("chord2");
                g_backend.ReloadSchemes();
                
            } else if (!g_ui.chordMode && wasChordMode) {
                // Exiting chord mode
                // 1. Disable chord and chord2 schemes
                g_backend.DisableScheme("chord");
                g_backend.DisableScheme("chord2");
                
                // 2. Restore previously saved schemes
                for (const auto& scheme : g_savedEnabledSchemes) {
                    g_backend.EnableScheme(scheme);
                }
                g_savedEnabledSchemes.clear();
                
                g_backend.ReloadSchemes();
            }
            
            // Clear chord sequence if disabling
            if (!g_ui.chordMode) {
                g_ui.chordSequence.clear();
                g_ui.chordResult.clear();
            }
            
            // Adjust window size based on chord mode
            if (g_hwndParentMain) {
                RECT rcWindow;
                GetWindowRect(g_hwndParentMain, &rcWindow);
                int width = rcWindow.right - rcWindow.left;
                int baseHeight = 170;  // Original height
                int newHeight = g_ui.chordMode ? 320 : baseHeight;
                
                SetWindowPos(g_hwndParentMain, NULL, rcWindow.left, rcWindow.top, 
                            width, newHeight, SWP_NOZORDER);
            }
            
            // Save configuration
            SaveConfig();
            
            // Refresh main window
            if (g_hwndParentMain) {
                InvalidateRect(g_hwndParentMain, NULL, TRUE);
            }
            
            DestroyWindow(hwndDlg);
            return 0;
        }
        
        if (id == IDCANCEL) {
            DestroyWindow(hwndDlg);
            return 0;
        }
        
        break;
        }
    }
    
    case WM_CLOSE:
        DestroyWindow(hwndDlg);
        return 0;
    }
    
    return DefWindowProcW(hwndDlg, msg, wParam, lParam);
}

void ShowOtherSettingsDialog(HWND hwndParent)
{
    static bool registered = false;
    if (!registered) {
        WNDCLASSW wc = {};
        wc.lpfnWndProc = OtherSettingsDlgProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.lpszClassName = L"OtherSettingsDialog";
        wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
        wc.hCursor = LoadCursor(NULL, IDC_ARROW);
        RegisterClassW(&wc);
        registered = true;
    }
    
    HWND hwndDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        L"OtherSettingsDialog",
        L"Other Settings",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 380, 210,
        hwndParent,
        NULL,
        GetModuleHandle(NULL),
        NULL
    );
    
    if (!hwndDlg) return;
    
    // Center on parent
    RECT rcParent, rcDlg;
    GetWindowRect(hwndParent, &rcParent);
    GetWindowRect(hwndDlg, &rcDlg);
    int x = rcParent.left + (rcParent.right - rcParent.left - (rcDlg.right - rcDlg.left)) / 2;
    int y = rcParent.top + (rcParent.bottom - rcParent.top - (rcDlg.bottom - rcDlg.top)) / 2;
    SetWindowPos(hwndDlg, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
    
    // Trigger WM_INITDIALOG
    SendMessageW(hwndDlg, WM_INITDIALOG, 0, 0);
}

LRESULT CALLBACK SchemeSelectionDlgProc(HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_INITDIALOG:
    {
        // Get all available schemes
        auto allSchemes = g_backend.GetAvailableSchemes();
        
        // Clear and rebuild groups
        g_schemeGroups.clear();
        g_standaloneSchemes.clear();
        
        // Define groups
        SchemeGroup defaultGroup;
        defaultGroup.name = "default";
        
        SchemeGroup specializedGroup;
        specializedGroup.name = "specialized";
        
        // Categorize schemes
        for (const auto& scheme : allSchemes) {
            // Skip chord and chord2 schemes - they're managed by chord mode setting
            if (scheme == "chord" || scheme == "chord2") continue;
            
            if (scheme == "default" || scheme == "simple" || scheme == "tones") {
                defaultGroup.schemes.push_back(scheme);
            } else if (scheme == "chinese") {
                specializedGroup.schemes.push_back(scheme);
            } else if (scheme == "custom") {
                g_standaloneSchemes.push_back(scheme);
            } else {
                // Unknown schemes go to specialized
                specializedGroup.schemes.push_back(scheme);
            }
        }
        
        if (!defaultGroup.schemes.empty()) g_schemeGroups.push_back(defaultGroup);
        if (!specializedGroup.schemes.empty()) g_schemeGroups.push_back(specializedGroup);
        
        // Create UI
        int yPos = 15;
        int xStart = 20;
        int groupCheckW = 320;
        int itemCheckW = 300;
        int btnW = 30;
        
        // Create group controls
        for (size_t gi = 0; gi < g_schemeGroups.size(); ++gi) {
            auto& group = g_schemeGroups[gi];
            
            // Group checkbox
            std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
            std::wstring groupName = conv.from_bytes(group.name);
            
            group.checkboxGroup = CreateWindowW(
                L"BUTTON", groupName.c_str(),
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                xStart, yPos, groupCheckW, 20,
                hwndDlg, (HMENU)(INT_PTR)(2000 + gi),
                GetModuleHandle(NULL), NULL
            );
            
            // Expand/Collapse button
            group.buttonExpand = CreateWindowW(
                L"BUTTON", group.expanded ? L"-" : L"+",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                xStart + groupCheckW + 5, yPos - 2, btnW, 24,
                hwndDlg, (HMENU)(INT_PTR)(3000 + gi),
                GetModuleHandle(NULL), NULL
            );
            
            // Check if all schemes in group are enabled
            bool allEnabled = true;
            for (const auto& scheme : group.schemes) {
                if (!g_backend.IsSchemeEnabled(scheme)) {
                    allEnabled = false;
                    break;
                }
            }
            SendMessage(group.checkboxGroup, BM_SETCHECK, allEnabled ? BST_CHECKED : BST_UNCHECKED, 0);
            
            yPos += 25;
            
            // Child scheme checkboxes - always create them, but control visibility
            for (size_t i = 0; i < group.schemes.size(); ++i) {
                bool enabled = g_backend.IsSchemeEnabled(group.schemes[i]);
                std::wstring schemeName = conv.from_bytes(group.schemes[i]);
                
                HWND hwndCheck = CreateWindowW(
                    L"BUTTON", schemeName.c_str(),
                    WS_CHILD | BS_AUTOCHECKBOX | (group.expanded ? WS_VISIBLE : 0),
                    xStart + 20, yPos, itemCheckW, 20,
                    hwndDlg, (HMENU)(INT_PTR)(1000 + gi * 100 + i),
                    GetModuleHandle(NULL), NULL
                );
                
                SendMessage(hwndCheck, BM_SETCHECK, enabled ? BST_CHECKED : BST_UNCHECKED, 0);
                group.checkboxes.push_back(hwndCheck);
                
                // Only increment yPos if expanded (for dialog sizing)
                if (group.expanded) {
                    yPos += 25;
                }
            }
        }
        
        // Standalone schemes (e.g., custom)
        for (size_t i = 0; i < g_standaloneSchemes.size(); ++i) {
            bool enabled = g_backend.IsSchemeEnabled(g_standaloneSchemes[i]);
            std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
            std::wstring schemeName = conv.from_bytes(g_standaloneSchemes[i]);
            
            HWND hwndCheck = CreateWindowW(
                L"BUTTON", schemeName.c_str(),
                WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                xStart, yPos, groupCheckW, 20,
                hwndDlg, (HMENU)(INT_PTR)(4000 + i),
                GetModuleHandle(NULL), NULL
            );
            
            SendMessage(hwndCheck, BM_SETCHECK, enabled ? BST_CHECKED : BST_UNCHECKED, 0);
            yPos += 25;
        }
        
        // Apply and Cancel buttons
        HWND hBtnApply = CreateWindowW(
            L"BUTTON", L"Apply",
            WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            80, yPos + 15, 80, 30,
            hwndDlg, (HMENU)IDOK,
            GetModuleHandle(NULL), NULL
        );
        
        HWND hBtnCancel = CreateWindowW(
            L"BUTTON", L"Cancel",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            180, yPos + 15, 80, 30,
            hwndDlg, (HMENU)IDCANCEL,
            GetModuleHandle(NULL), NULL
        );
        
        // Resize dialog 
        int dialogHeight = yPos + 90;
        SetWindowPos(hwndDlg, NULL, 0, 0, 400, dialogHeight, SWP_NOMOVE | SWP_NOZORDER);
        
        return 0;
    }
    
    case WM_COMMAND:
    {
        int id = LOWORD(wParam);
        
        // Group checkbox clicked
        if (id >= 2000 && id < 3000) {
            size_t groupIdx = id - 2000;
            if (groupIdx < g_schemeGroups.size()) {
                LRESULT checked = SendMessage(g_schemeGroups[groupIdx].checkboxGroup, BM_GETCHECK, 0, 0);
                bool enable = (checked == BST_CHECKED);
                
                // Update all child checkboxes
                for (HWND hwndCheck : g_schemeGroups[groupIdx].checkboxes) {
                    SendMessage(hwndCheck, BM_SETCHECK, enable ? BST_CHECKED : BST_UNCHECKED, 0);
                }
            }
            return 0;
        }
        
        // Expand/Collapse button clicked
        if (id >= 3000 && id < 4000) {
            size_t groupIdx = id - 3000;
            if (groupIdx < g_schemeGroups.size()) {
                auto& group = g_schemeGroups[groupIdx];
                group.expanded = !group.expanded;
                
                int heightChange = (int)group.checkboxes.size() * 25;
                
                // Toggle visibility and reposition child checkboxes
                RECT groupRect;
                GetWindowRect(group.checkboxGroup, &groupRect);
                POINT groupPt = {groupRect.left, groupRect.bottom};
                ScreenToClient(hwndDlg, &groupPt);
                
                for (size_t i = 0; i < group.checkboxes.size(); ++i) {
                    HWND hwndCheck = group.checkboxes[i];
                    if (group.expanded) {
                        // Position and show
                        int yPos = groupPt.y + 5 + (int)i * 25;
                        RECT rc;
                        GetWindowRect(hwndCheck, &rc);
                        int width = rc.right - rc.left;
                        int height = rc.bottom - rc.top;
                        POINT pt = {rc.left, rc.top};
                        ScreenToClient(hwndDlg, &pt);
                        SetWindowPos(hwndCheck, NULL, pt.x, yPos, width, height, SWP_NOZORDER);
                        ShowWindow(hwndCheck, SW_SHOW);
                    } else {
                        // Just hide
                        ShowWindow(hwndCheck, SW_HIDE);
                    }
                }
                
                // Update button text
                SetWindowTextW(group.buttonExpand, group.expanded ? L"-" : L"+");
                
                // Move all controls below this group
                
                // Move subsequent groups
                for (size_t gi = groupIdx + 1; gi < g_schemeGroups.size(); ++gi) {
                    RECT rc;
                    GetWindowRect(g_schemeGroups[gi].checkboxGroup, &rc);
                    POINT pt = {rc.left, rc.top};
                    ScreenToClient(hwndDlg, &pt);
                    
                    int newY = group.expanded ? pt.y + heightChange : pt.y - heightChange;
                    SetWindowPos(g_schemeGroups[gi].checkboxGroup, NULL, pt.x, newY, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
                    SetWindowPos(g_schemeGroups[gi].buttonExpand, NULL, pt.x + 325, newY - 2, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
                    
                    // Move child checkboxes
                    for (HWND hwndCheck : g_schemeGroups[gi].checkboxes) {
                        GetWindowRect(hwndCheck, &rc);
                        pt = {rc.left, rc.top};
                        ScreenToClient(hwndDlg, &pt);
                        SetWindowPos(hwndCheck, NULL, pt.x, group.expanded ? pt.y + heightChange : pt.y - heightChange, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
                    }
                }
                
                // Move standalone schemes
                HWND hwndChild = GetWindow(hwndDlg, GW_CHILD);
                while (hwndChild) {
                    int ctrlId = GetDlgCtrlID(hwndChild);
                    if (ctrlId >= 4000 && ctrlId < 5000) {
                        RECT rc;
                        GetWindowRect(hwndChild, &rc);
                        POINT pt = {rc.left, rc.top};
                        ScreenToClient(hwndDlg, &pt);
                        SetWindowPos(hwndChild, NULL, pt.x, group.expanded ? pt.y + heightChange : pt.y - heightChange, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
                    }
                    hwndChild = GetWindow(hwndChild, GW_HWNDNEXT);
                }
                
                // Move Apply and Cancel buttons
                HWND hBtnApply = GetDlgItem(hwndDlg, IDOK);
                HWND hBtnCancel = GetDlgItem(hwndDlg, IDCANCEL);
                if (hBtnApply) {
                    RECT rc;
                    GetWindowRect(hBtnApply, &rc);
                    POINT pt = {rc.left, rc.top};
                    ScreenToClient(hwndDlg, &pt);
                    SetWindowPos(hBtnApply, NULL, pt.x, group.expanded ? pt.y + heightChange : pt.y - heightChange, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
                }
                if (hBtnCancel) {
                    RECT rc;
                    GetWindowRect(hBtnCancel, &rc);
                    POINT pt = {rc.left, rc.top};
                    ScreenToClient(hwndDlg, &pt);
                    SetWindowPos(hBtnCancel, NULL, pt.x, group.expanded ? pt.y + heightChange : pt.y - heightChange, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
                }
                
                // Adjust dialog height
                RECT rc;
                GetWindowRect(hwndDlg, &rc);
                if (group.expanded) {
                    SetWindowPos(hwndDlg, NULL, 0, 0, rc.right - rc.left, rc.bottom - rc.top + heightChange, SWP_NOMOVE | SWP_NOZORDER);
                } else {
                    SetWindowPos(hwndDlg, NULL, 0, 0, rc.right - rc.left, rc.bottom - rc.top - heightChange, SWP_NOMOVE | SWP_NOZORDER);
                }
            }
            return 0;
        }
        
        // Apply button
        if (id == IDOK) {
            // Apply group schemes
            for (const auto& group : g_schemeGroups) {
                for (size_t i = 0; i < group.schemes.size(); ++i) {
                    if (i < group.checkboxes.size() && IsWindow(group.checkboxes[i])) {
                        LRESULT checked = SendMessage(group.checkboxes[i], BM_GETCHECK, 0, 0);
                        bool shouldEnable = (checked == BST_CHECKED);
                        bool currentlyEnabled = g_backend.IsSchemeEnabled(group.schemes[i]);
                        
                        if (shouldEnable && !currentlyEnabled) {
                            g_backend.EnableScheme(group.schemes[i]);
                        } else if (!shouldEnable && currentlyEnabled) {
                            g_backend.DisableScheme(group.schemes[i]);
                        }
                    }
                }
            }
            
            // Apply standalone schemes - need to find their checkboxes
            HWND hwndChild = GetWindow(hwndDlg, GW_CHILD);
            while (hwndChild) {
                int ctrlId = GetDlgCtrlID(hwndChild);
                if (ctrlId >= 4000 && ctrlId < 5000) {
                    size_t idx = ctrlId - 4000;
                    if (idx < g_standaloneSchemes.size()) {
                        LRESULT checked = SendMessage(hwndChild, BM_GETCHECK, 0, 0);
                        bool shouldEnable = (checked == BST_CHECKED);
                        bool currentlyEnabled = g_backend.IsSchemeEnabled(g_standaloneSchemes[idx]);
                        
                        if (shouldEnable && !currentlyEnabled) {
                            g_backend.EnableScheme(g_standaloneSchemes[idx]);
                        } else if (!shouldEnable && currentlyEnabled) {
                            g_backend.DisableScheme(g_standaloneSchemes[idx]);
                        }
                    }
                }
                hwndChild = GetWindow(hwndChild, GW_HWNDNEXT);
            }
            
            // Reload dictionary
            g_backend.ReloadSchemes();
            
            // Save configuration
            SaveConfig();
            
            g_schemeGroups.clear();
            DestroyWindow(hwndDlg);
            return 0;
        }
        
        // Cancel button
        if (id == IDCANCEL) {
            g_schemeGroups.clear();
            DestroyWindow(hwndDlg);
            return 0;
        }
        
        break;
    }
    
    case WM_CLOSE:
        g_schemeGroups.clear();
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

// About Dialog - Author Information
LRESULT CALLBACK AboutDlgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static Bitmap* githubIcon = NULL;
    static RECT githubBtnRect = {0, 0, 0, 0};
    
    switch (msg) {
    case WM_CREATE:
    {
        // Load GitHub icon
        githubIcon = new Bitmap(L"..\\uicon\\github.png");
        
        // Calculate button position (center bottom)
        RECT rc;
        GetClientRect(hwnd, &rc);
        int btnW = 48;
        int btnH = 48;
        githubBtnRect.left = (rc.right - btnW) / 2;
        githubBtnRect.top = rc.bottom - btnH - 20;
        githubBtnRect.right = githubBtnRect.left + btnW;
        githubBtnRect.bottom = githubBtnRect.top + btnH;
        
        return 0;
    }
    
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        
        Graphics g(hdc);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        
        // Background
        SolidBrush bgBrush(Color(255, 250, 250, 250));
        g.FillRectangle(&bgBrush, 0, 0, rc.right, rc.bottom);
        
        // Title
        FontFamily fontFamily(L"Segoe UI");
        Font titleFont(&fontFamily, 18, FontStyleBold, UnitPixel);
        SolidBrush textBrush(Color(255, 40, 40, 40));
        StringFormat format;
        format.SetAlignment(StringAlignmentCenter);
        format.SetLineAlignment(StringAlignmentCenter);
        
        RectF titleRect(0, 20, (REAL)rc.right, 30);
        g.DrawString(L"ScrIPA IPA Input Method", -1, &titleFont, titleRect, &format, &textBrush);
        
        // Author info
        Font normalFont(&fontFamily, 12, FontStyleRegular, UnitPixel);
        RectF infoRect(0, 60, (REAL)rc.right, 80);
        g.DrawString(L"Author: ShioLilia\nVersion: 0.0.5.2\nVersion Nickname: Nocturne\nLicense: GPLv3 and MIT", -1, &normalFont, infoRect, &format, &textBrush);
        
        // GitHub button
        if (githubIcon && githubIcon->GetLastStatus() == Ok) {
            g.DrawImage(githubIcon, githubBtnRect.left, githubBtnRect.top, 
                        githubBtnRect.right - githubBtnRect.left, 
                        githubBtnRect.bottom - githubBtnRect.top);
        } else {
            SolidBrush btnBrush(Color(255, 200, 200, 200));
            g.FillRectangle(&btnBrush, githubBtnRect.left, githubBtnRect.top,
                           githubBtnRect.right - githubBtnRect.left,
                           githubBtnRect.bottom - githubBtnRect.top);
        }
        
        // Border for GitHub button
        Pen btnPen(Color(255, 100, 100, 100), 2);
        g.DrawRectangle(&btnPen, githubBtnRect.left, githubBtnRect.top,
                       githubBtnRect.right - githubBtnRect.left,
                       githubBtnRect.bottom - githubBtnRect.top);
        
        EndPaint(hwnd, &ps);
        return 0;
    }
    
    case WM_LBUTTONDOWN:
    {
        int x = LOWORD(lParam);
        int y = HIWORD(lParam);
        POINT pt = {x, y};
        
        if (PtInRect(&githubBtnRect, pt)) {
            ShellExecuteW(NULL, L"open", L"https://github.com/ShioLilia/", NULL, NULL, SW_SHOW);
        }
        return 0;
    }
    
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE || wParam == VK_RETURN) {
            ShowWindow(hwnd, SW_HIDE);
        }
        return 0;
    
    case WM_CLOSE:
        ShowWindow(hwnd, SW_HIDE);
        return 0;
    
    case WM_DESTROY:
        if (githubIcon) {
            delete githubIcon;
            githubIcon = NULL;
        }
        g_hwndAboutDialog = NULL;
        return 0;
    }
    
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

void ShowAboutDialog(HWND hwndParent)
{
    // Create window on first use
    if (!g_hwndAboutDialog) {
        static bool registered = false;
        if (!registered) {
            WNDCLASSW wc = {};
            wc.lpfnWndProc = AboutDlgProc;
            wc.hInstance = GetModuleHandle(NULL);
            wc.lpszClassName = L"AboutDialog";
            wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
            wc.hCursor = LoadCursor(NULL, IDC_ARROW);
            RegisterClassW(&wc);
            registered = true;
        }
        
        g_hwndAboutDialog = CreateWindowExW(
            WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
            L"AboutDialog",
            L"About Scripa",
            WS_POPUP | WS_CAPTION | WS_SYSMENU,
            CW_USEDEFAULT, CW_USEDEFAULT, 350, 250,
            NULL,  // No parent, independent window
            NULL,
            GetModuleHandle(NULL),
            NULL
        );
        
        if (!g_hwndAboutDialog) return;
    }
    
    // Toggle visibility
    if (IsWindowVisible(g_hwndAboutDialog)) {
        ShowWindow(g_hwndAboutDialog, SW_HIDE);
    } else {
        // Center on parent
        RECT rcParent, rcDlg;
        GetWindowRect(hwndParent, &rcParent);
        GetWindowRect(g_hwndAboutDialog, &rcDlg);
        int x = rcParent.left + (rcParent.right - rcParent.left - (rcDlg.right - rcDlg.left)) / 2;
        int y = rcParent.top + (rcParent.bottom - rcParent.top - (rcDlg.bottom - rcDlg.top)) / 2;
        SetWindowPos(g_hwndAboutDialog, HWND_TOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
    }
}

// Scheme Reference popup window procedure
LRESULT CALLBACK SchemeReferenceWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    static HWND hBtnC1 = NULL, hBtnC2 = NULL, hBtnV = NULL, hBtnElse = NULL;
    
    switch (msg) {
    case WM_CREATE:
    {
        // Create 4 buttons (removed C3, C2 shows both cons2 and cons3)
        int btnY = 10;
        int btnW = 70;
        int btnH = 30;
        int spacing = 10;
        int startX = 10;
        
        hBtnC1 = CreateWindowW(L"BUTTON", L"C1", 
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            startX, btnY, btnW, btnH, hwnd, (HMENU)1001, GetModuleHandle(NULL), NULL);
        
        hBtnC2 = CreateWindowW(L"BUTTON", L"C2",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            startX + (btnW + spacing), btnY, btnW, btnH, hwnd, (HMENU)1002, GetModuleHandle(NULL), NULL);
        
        hBtnV = CreateWindowW(L"BUTTON", L"V",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            startX + 2 * (btnW + spacing), btnY, btnW, btnH, hwnd, (HMENU)1003, GetModuleHandle(NULL), NULL);
        
        hBtnElse = CreateWindowW(L"BUTTON", L"Else",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            startX + 3 * (btnW + spacing), btnY, btnW, btnH, hwnd, (HMENU)1004, GetModuleHandle(NULL), NULL);
        
        return 0;
    }
    
    case WM_COMMAND:
    {
        int id = LOWORD(wParam);
        if (id >= 1001 && id <= 1004) {
            g_schemeRefData.currentView = id - 1001;  // 0=C1, 1=C2, 2=V, 3=Else
            InvalidateRect(hwnd, NULL, TRUE);
        }
        return 0;
    }
    
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);
        
        Graphics g(hdc);
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        
        // Background - dark mode support
        Color bgColor = g_ui.darkMode ? Color(255, 45, 45, 48) : Color(255, 255, 255, 255);
        SolidBrush bgBrush(bgColor);
        g.FillRectangle(&bgBrush, 0, 0, rc.right, rc.bottom);
        
        // Draw current image(s)
        int yPos = 50;
        switch (g_schemeRefData.currentView) {
        case 0:  // C1
            if (g_schemeRefData.cons1 && g_schemeRefData.cons1->GetLastStatus() == Ok) {
                g.DrawImage(g_schemeRefData.cons1, 10, yPos, 
                    g_schemeRefData.cons1->GetWidth(), g_schemeRefData.cons1->GetHeight());
            }
            break;
        case 1:  // C2 - show both cons2 and cons3
            if (g_schemeRefData.cons2 && g_schemeRefData.cons2->GetLastStatus() == Ok) {
                int h2 = g_schemeRefData.cons2->GetHeight();
                g.DrawImage(g_schemeRefData.cons2, 10, yPos,
                    g_schemeRefData.cons2->GetWidth(), h2);
                yPos += h2 + 10;
            }
            if (g_schemeRefData.cons3 && g_schemeRefData.cons3->GetLastStatus() == Ok) {
                g.DrawImage(g_schemeRefData.cons3, 10, yPos,
                    g_schemeRefData.cons3->GetWidth(), g_schemeRefData.cons3->GetHeight());
            }
            break;
        case 2:  // V
            if (g_schemeRefData.vowel && g_schemeRefData.vowel->GetLastStatus() == Ok) {
                g.DrawImage(g_schemeRefData.vowel, 10, yPos,
                    g_schemeRefData.vowel->GetWidth(), g_schemeRefData.vowel->GetHeight());
            }
            break;
        case 3:  // Else
            if (g_schemeRefData.elseAndTones && g_schemeRefData.elseAndTones->GetLastStatus() == Ok) {
                g.DrawImage(g_schemeRefData.elseAndTones, 10, yPos,
                    g_schemeRefData.elseAndTones->GetWidth(), g_schemeRefData.elseAndTones->GetHeight());
            }
            break;
        }
        
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
        
        // Background - dark mode support
        Color bgColor = g_ui.darkMode ? Color(255, 45, 45, 48) : Color(255, 245, 245, 245);
        Color textColor = g_ui.darkMode ? Color(255, 245, 240, 230) : Color(255, 50, 50, 50);
        Color hoverColor = g_ui.darkMode ? Color(255, 60, 60, 65) : Color(255, 220, 220, 220);
        
        SolidBrush bgBrush(bgColor);
        g.FillRectangle(&bgBrush, 0, 0, 200, (int)g_menuItems.size() * 32);
        
        // Draw menu items
        SolidBrush textBrush(textColor);
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
                SolidBrush hoverBrush(hoverColor);
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
                ShowOtherSettingsDialog(g_hwndParentMain);
                break;
            case 2: // Change Theme
                g_ui.darkMode = !g_ui.darkMode;
                SaveConfig();  // Save theme preference
                InvalidateRect(g_hwndParentMain, NULL, TRUE);
                ShowWindow(hwnd, SW_HIDE);  // 关闭设置菜单
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
            
            // Load configuration (must be after backend init)
            LoadConfig();
            
            // Reload schemes with saved configuration
            g_backend.ReloadSchemes();
            
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
            
            // Initialize piano keyboard for chord mode
            g_pianoKeys.clear();
            // Will be positioned dynamically in WM_PAINT
        }
        return 0;

    case WM_SIZE:
        // Force redraw when window size changes to update button positions
        InvalidateRect(hwnd, NULL, TRUE);
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
                if (g_ui.chordMode) {
                    g_ui.chordSequence.clear();
                    g_ui.chordResult.clear();
                }
                g_backend.clearBuffer();
                g_ui.composition = L"";
                g_ui.items = { L"" };
                g_ui.selected = 0;
                g_ui.pageIndex = 0;
                InvalidateRect(hwnd, NULL, FALSE);
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
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        if (wParam == VK_RIGHT) {
            g_ui.selected = (g_ui.selected + 1) % (int)g_ui.items.size();
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;
        }
        if (wParam == VK_LEFT) {
            g_ui.selected = (g_ui.selected - 1 + (int)g_ui.items.size()) % (int)g_ui.items.size();
            InvalidateRect(hwnd, NULL, FALSE);
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
                
                // In chord mode, clear chord sequence; in normal mode, clear buffer
                if (g_ui.chordMode) {
                    g_ui.chordSequence.clear();
                    g_ui.chordResult.clear();
                }
                g_backend.clearBuffer();
                g_ui.composition = L"";
                g_ui.items = { L"" };
                g_ui.selected = 0;
                g_ui.pageIndex = 0;
                InvalidateRect(hwnd, NULL, FALSE);
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
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            if (ch == L']') {
                int totalPages = (int)((g_ui.items.size() + g_ui.itemsPerPage - 1) / g_ui.itemsPerPage);
                if (g_ui.pageIndex + 1 < totalPages) g_ui.pageIndex++;
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }

            // 小键盘数字会到达这里，作为普通字符输入
            // 大键盘数字已在 WM_KEYDOWN 中被拦截，不会到达这里

            // Special handling for chord mode
            if (g_ui.chordMode) {
                // Space or Enter key: confirm current note and add to sequence
                if (ch == L' ' || ch == L'\r') {
                    auto candidates = g_backend.GetCandidates();
                    
                    // If we have a candidate, add it to the chord sequence
                    if (!candidates.empty() && !candidates[0].empty()) {
                        std::wstring noteName = candidates[0];
                        g_ui.chordSequence.push_back(noteName);
                        
                        // Update chord results and candidates
                        UpdateChordResults(hwnd);
                        
                        // Clear composition buffer for next note
                        g_backend.clearBuffer();
                        g_ui.composition = L"";
                    }
                    return 0;
                }
                
                // Regular character: try to add to current composition
                // First, save the previous composition and candidates
                std::wstring prevComposition = g_ui.composition;
                auto prevCandidates = g_backend.GetCandidates();
                
                // Try adding the new character
                bool handled = g_backend.OnKeyDown(ch);
                g_ui.composition = g_backend.GetComposition();
                auto newCandidates = g_backend.GetCandidates();
                
                // Check if the new composition has valid candidates
                if (newCandidates.empty() || newCandidates[0].empty()) {
                    // No valid candidates with new character
                    // This means we should confirm the previous note and start fresh
                    if (!prevComposition.empty() && !prevCandidates.empty() && !prevCandidates[0].empty()) {
                        // Confirm the previous note
                        g_ui.chordSequence.push_back(prevCandidates[0]);
                        
                        // Clear and restart with the new character
                        g_backend.clearBuffer();
                        g_backend.OnKeyDown(ch);
                        g_ui.composition = g_backend.GetComposition();
                        newCandidates = g_backend.GetCandidates();
                    }
                }
                
                // Update chord results with current sequence
                UpdateChordResults(hwnd);
                
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            
            // Normal mode: forward character to backend and refresh UI
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
            InvalidateRect(hwnd, NULL, FALSE);
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
                    
                    // Load scheme reference images
                    g_schemeRefData.LoadAll();
                    
                    // Create window (larger to fit images)
                    g_hwndSchemeReference = CreateWindowExW(
                        WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
                        L"SchemeReferenceWindow",
                        L"Scheme Reference",
                        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME,
                        CW_USEDEFAULT, CW_USEDEFAULT, 900, 700,
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
                ShowAboutDialog(hwnd);
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
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            if (PtInRect(&g_btnLeft, POINT{x, y})) {
                if (g_ui.pageIndex > 0) g_ui.pageIndex--;
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            if (PtInRect(&g_btnRight, POINT{x, y})) {
                int totalPages = (int)((g_ui.items.size() + g_ui.itemsPerPage - 1) / g_ui.itemsPerPage);
                if (g_ui.pageIndex + 1 < totalPages) g_ui.pageIndex++;
                InvalidateRect(hwnd, NULL, FALSE);
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
                        
                        // In chord mode, clear chord sequence; in normal mode, clear buffer
                        if (g_ui.chordMode) {
                            g_ui.chordSequence.clear();
                            g_ui.chordResult.clear();
                        }
                        g_backend.clearBuffer();
                        g_ui.composition = L"";
                        g_ui.items = { L"" };
                        g_ui.selected = 0;
                        g_ui.pageIndex = 0;
                        InvalidateRect(hwnd, NULL, FALSE);
                        return 0;
                    }
                }
            }
            
            // Check piano keyboard keys (only when Chord mode enabled)
            // Check BLACK keys first (they are on top of white keys)
            if (g_ui.chordMode) {
                bool keyClicked = false;
                
                // First check black keys
                for (const auto& key : g_pianoKeys) {
                    if (key.isBlack && PtInRect(&key.rect, POINT{x, y})) {
                        // Add note to sequence
                        g_ui.chordSequence.push_back(key.note);
                        
                        // Update chord results and candidates
                        UpdateChordResults(hwnd);
                        
                        keyClicked = true;
                        break;
                    }
                }
                
                // Then check white keys if no black key was clicked
                if (!keyClicked) {
                    for (const auto& key : g_pianoKeys) {
                        if (!key.isBlack && PtInRect(&key.rect, POINT{x, y})) {
                            // Add note to sequence
                            g_ui.chordSequence.push_back(key.note);
                            
                            // Update chord results and candidates
                            UpdateChordResults(hwnd);
                            
                            return 0;
                        }
                    }
                }
            }
        }
        return 0;

    case WM_RBUTTONDOWN:
        // Right-click to clear chord sequence in Chord mode
        if (g_ui.chordMode) {
            g_ui.chordSequence.clear();
            g_ui.chordResult.clear();
            g_ui.items = {L""};
            g_ui.selected = 0;
            g_ui.pageIndex = 0;
            g_backend.clearBuffer();
            g_ui.composition = L"";
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;

    case WM_ERASEBKGND:
        // Prevent default erase to reduce flicker (we'll paint everything in WM_PAINT)
        return 1;

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            
            // Create memory DC for double buffering
            HDC hdcMem = CreateCompatibleDC(hdc);
            HBITMAP hbmMem = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
            HBITMAP hbmOld = (HBITMAP)SelectObject(hdcMem, hbmMem);
            
            // Now draw to hdcMem instead of hdc
            Graphics g(hdcMem);
            g.SetSmoothingMode(SmoothingModeAntiAlias);

            // Background - dark mode support
            Color bgColor = g_ui.darkMode ? Color(255, 45, 45, 48) : Color(255, 255, 255);
            Color textColor = g_ui.darkMode ? Color(255, 245, 240, 230) : Color(255, 0, 0, 0);
            Color borderColor = g_ui.darkMode ? Color(255, 80, 80, 80) : Color(255, 0, 0, 0);
            Color btnBgColor = g_ui.darkMode ? Color(255, 245, 240, 230) : Color(255, 255, 255, 255);
            
            SolidBrush bgBrush(bgColor);
            g.FillRectangle(&bgBrush, 0, 0, rc.right, rc.bottom);

            // draw toolbar (top 40 pixels)
            int toolbarY = 8;
            int btnX = 10;
            int btnSize = 30;
            int spacing = 8;

            // Button background brush for dark mode
            SolidBrush buttonBgBrush(btnBgColor);

            // default button
            g_btnDefault = {btnX, toolbarY, btnX + btnSize, toolbarY + btnSize};
            if (g_ui.darkMode) {
                g.FillRectangle(&buttonBgBrush, btnX, toolbarY, btnSize, btnSize);
            }
            if (g_icons.default_icon && g_icons.default_icon->GetLastStatus() == Ok) {
                g.DrawImage(g_icons.default_icon, btnX, toolbarY, btnSize, btnSize);
            } else {
                SolidBrush btnBrush(Color(200, 200, 200));
                g.FillRectangle(&btnBrush, btnX, toolbarY, btnSize, btnSize);
            }
            Pen btnPen(borderColor, 1);
            g.DrawRectangle(&btnPen, btnX, toolbarY, btnSize, btnSize);

            btnX += btnSize + spacing;

            // user button
            g_btnUser = {btnX, toolbarY, btnX + btnSize, toolbarY + btnSize};
            if (g_ui.darkMode) {
                g.FillRectangle(&buttonBgBrush, btnX, toolbarY, btnSize, btnSize);
            }
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
            if (g_ui.darkMode) {
                g.FillRectangle(&buttonBgBrush, btnX, toolbarY, btnSize, btnSize);
            }
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
            if (g_ui.darkMode) {
                g.FillRectangle(&buttonBgBrush, btnX, toolbarY, btnSize, btnSize);
            }
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
            if (g_ui.darkMode) {
                g.FillRectangle(&buttonBgBrush, btnX, toolbarY, btnSize, btnSize);
            }
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
            if (g_ui.darkMode) {
                g.FillRectangle(&buttonBgBrush, btnX, toolbarY, btnSize, btnSize);
            }
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
                HFONT hOld = (HFONT)SelectObject(hdcMem, hFont);
                
                // Text color based on dark mode
                COLORREF textColorWin = g_ui.darkMode ? RGB(245, 240, 230) : RGB(0, 0, 0);
                SetTextColor(hdcMem, textColorWin);
                SetBkMode(hdcMem, TRANSPARENT);

                // composition buffer display
                RECT compRc = {10, 45, rc.right - 10, 58};
                std::wstring comp = L"Buffer: " + g_ui.composition;
                DrawTextW(hdcMem, comp.c_str(), (int)comp.size(), &compRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);

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

                    // draw candidate background and border - dark mode support
                    COLORREF bgColorCandidate = (idx == g_ui.selected) 
                        ? RGB(0, 120, 215) 
                        : (g_ui.darkMode ? RGB(60, 60, 65) : RGB(255, 255, 255));
                    HBRUSH hbr = CreateSolidBrush(bgColorCandidate);
                    FillRect(hdcMem, &it, hbr);
                    
                    COLORREF borderColorCandidate = g_ui.darkMode ? RGB(80, 80, 80) : RGB(0, 0, 0);
                    HBRUSH borderBrush = CreateSolidBrush(borderColorCandidate);
                    FrameRect(hdcMem, &it, borderBrush);
                    DeleteObject(borderBrush);
                    DeleteObject(hbr);

                    // draw text
                    COLORREF candTextColor = (idx == g_ui.selected) 
                        ? RGB(255, 255, 255) 
                        : (g_ui.darkMode ? RGB(245, 240, 230) : RGB(0, 0, 0));
                    SetTextColor(hdcMem, candTextColor);
                    DrawTextW(hdcMem, g_ui.items[idx].c_str(), (int)g_ui.items[idx].size(), &it, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                    // draw index number
                    RECT numRc = { left + 4, top + 4, left + 24, top + 20 };
                    std::wstringstream ss;
                    ss << (i + 1);
                    COLORREF numColor = g_ui.darkMode ? RGB(150, 150, 150) : RGB(100, 100, 100);
                    SetTextColor(hdcMem, numColor);
                    DrawTextW(hdcMem, ss.str().c_str(), (int)ss.str().size(), &numRc, DT_LEFT | DT_TOP | DT_SINGLELINE);
                }

                // draw page indicator
                int totalPages = (int)((itemCount + per - 1) / per);
                std::wstringstream pss;
                pss << L"Page " << (g_ui.pageIndex + 1) << L"/" << (std::max)(1, totalPages);
                RECT pageRc = { rc.left + 10, 44 + itemH + 8 + 8, rc.right - 10, 44 + itemH + 8 + 24 };
                COLORREF pageColor = g_ui.darkMode ? RGB(180, 180, 180) : RGB(80, 80, 80);
                SetTextColor(hdcMem, pageColor);
                DrawTextW(hdcMem, pss.str().c_str(), (int)pss.str().size(), &pageRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

                SelectObject(hdcMem, hOld);
                DeleteObject(hFont);
            }
            
            // Draw piano keyboard for Chord mode
            if (g_ui.chordMode) {
                // Use GDI+ for piano keyboard
                Graphics gPiano(hdcMem);
                gPiano.SetSmoothingMode(SmoothingModeAntiAlias);
                
                // Piano keyboard position (below candidates area)
                int pianoY = 140;
                int whiteKeyW = 50;
                int whiteKeyH = 80;
                int blackKeyW = 30;
                int blackKeyH = 50;
                int startX = 20;
                
                // Clear and rebuild piano keys
                g_pianoKeys.clear();
                
                // White keys: C D E F G A B
                const wchar_t* whiteNotes[] = {L"C", L"D", L"E", L"F", L"G", L"A", L"B"};
                for (int i = 0; i < 7; ++i) {
                    int x = startX + i * whiteKeyW;
                    RECT keyRect = {x, pianoY, x + whiteKeyW, pianoY + whiteKeyH};
                    
                    PianoKey key;
                    key.note = whiteNotes[i];
                    key.rect = keyRect;
                    key.isBlack = false;
                    g_pianoKeys.push_back(key);
                    
                    // Draw white key
                    SolidBrush whiteBrush(Color(255, 255, 255, 255));
                    gPiano.FillRectangle(&whiteBrush, x, pianoY, whiteKeyW - 1, whiteKeyH);
                    
                    Pen blackPen(Color(255, 0, 0, 0), 2);
                    gPiano.DrawRectangle(&blackPen, x, pianoY, whiteKeyW - 1, whiteKeyH);
                    
                    // Draw note label
                    FontFamily fontFamily(L"Segoe UI");
                    Font noteFont(&fontFamily, 12, FontStyleBold, UnitPixel);
                    SolidBrush textBrush(Color(255, 0, 0, 0));
                    StringFormat format;
                    format.SetAlignment(StringAlignmentCenter);
                    format.SetLineAlignment(StringAlignmentFar);
                    RectF labelRect((REAL)x, (REAL)(pianoY + whiteKeyH - 25), (REAL)whiteKeyW, 20.0f);
                    gPiano.DrawString(whiteNotes[i], -1, &noteFont, labelRect, &format, &textBrush);
                }
                
                // Black keys: C# D# (skip) F# G# A#
                const wchar_t* blackNotes[] = {L"C#", L"D#", nullptr, L"F#", L"G#", L"A#"};
                const wchar_t* blackEnharmonic[] = {L"Db", L"Eb", nullptr, L"Gb", L"Ab", L"Bb"};
                for (int i = 0; i < 6; ++i) {
                    if (blackNotes[i] == nullptr) continue;  // No black key between E-F
                    
                    int x = startX + i * whiteKeyW + whiteKeyW - blackKeyW / 2;
                    RECT keyRect = {x, pianoY, x + blackKeyW, pianoY + blackKeyH};
                    
                    PianoKey key;
                    key.note = blackNotes[i];
                    key.rect = keyRect;
                    key.isBlack = true;
                    g_pianoKeys.push_back(key);
                    
                    // Draw black key
                    SolidBrush blackBrush(Color(255, 30, 30, 30));
                    gPiano.FillRectangle(&blackBrush, x, pianoY, blackKeyW, blackKeyH);
                    
                    Pen borderPen(Color(255, 0, 0, 0), 1);
                    gPiano.DrawRectangle(&borderPen, x, pianoY, blackKeyW, blackKeyH);
                    
                    // Draw enharmonic label (Db, Eb, etc.) at top
                    FontFamily fontFamily(L"Segoe UI");
                    Font enharmonicFont(&fontFamily, 8, FontStyleRegular, UnitPixel);
                    SolidBrush textBrush(Color(255, 180, 180, 180));
                    StringFormat format;
                    format.SetAlignment(StringAlignmentCenter);
                    format.SetLineAlignment(StringAlignmentNear);
                    RectF enharmonicRect((REAL)x, (REAL)(pianoY + 5), (REAL)blackKeyW, 12.0f);
                    gPiano.DrawString(blackEnharmonic[i], -1, &enharmonicFont, enharmonicRect, &format, &textBrush);
                    
                    // Draw sharp label (C#, D#, etc.) at bottom
                    Font noteFont(&fontFamily, 9, FontStyleBold, UnitPixel);
                    SolidBrush whiteBrush(Color(255, 255, 255, 255));
                    format.SetLineAlignment(StringAlignmentFar);
                    RectF labelRect((REAL)x, (REAL)(pianoY + blackKeyH - 18), (REAL)blackKeyW, 15.0f);
                    gPiano.DrawString(blackNotes[i], -1, &noteFont, labelRect, &format, &whiteBrush);
                }
                
                // Draw chord sequence and result
                int resultY = pianoY + whiteKeyH + 15;
                HFONT hFont2 = CreateFontW(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                    OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI");
                HFONT hOld2 = (HFONT)SelectObject(hdcMem, hFont2);
                
                COLORREF chordTextColor = g_ui.darkMode ? RGB(245, 240, 230) : RGB(0, 0, 0);
                SetTextColor(hdcMem, chordTextColor);
                SetBkMode(hdcMem, TRANSPARENT);
                
                std::wstring chordDisplay = L"Notes: " + (g_ui.chordResult.empty() ? L"(press keys or click piano)" : g_ui.chordResult);
                RECT chordRc = {startX, resultY, rc.right - 20, resultY + 25};
                DrawTextW(hdcMem, chordDisplay.c_str(), (int)chordDisplay.size(), &chordRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
                
                SelectObject(hdcMem, hOld2);
                DeleteObject(hFont2);
            }
            
            // Copy from memory DC to screen DC (double buffer)
            BitBlt(hdc, 0, 0, rc.right, rc.bottom, hdcMem, 0, 0, SRCCOPY);
            
            // Clean up memory DC
            SelectObject(hdcMem, hbmOld);
            DeleteObject(hbmMem);
            DeleteDC(hdcMem);

            EndPaint(hwnd, &ps);
        }
        return 0;

    case WM_DESTROY:
        // Save configuration before exit
        SaveConfig();
        
        if (g_hwndSettingsMenu) {
            DestroyWindow(g_hwndSettingsMenu);
            g_hwndSettingsMenu = NULL;
        }
        if (g_hwndSchemeReference) {
            DestroyWindow(g_hwndSchemeReference);
            g_hwndSchemeReference = NULL;
        }
        if (g_hwndAboutDialog) {
            DestroyWindow(g_hwndAboutDialog);
            g_hwndAboutDialog = NULL;
        }
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR pCmdLine, int nCmdShow)
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
        CW_USEDEFAULT, CW_USEDEFAULT, 600, 170,
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
    g_schemeRefData.UnloadAll();
    GdiplusShutdown(g_gdiplusToken);

    return 0;
}
