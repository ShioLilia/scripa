#include "ScripaTSF.h"
#include <locale>
#include <codecvt>
#include <filesystem>
#include <iostream>

using namespace std;

ScripaTSF::ScripaTSF()
{
}

ScripaTSF::~ScripaTSF()
{
}

bool ScripaTSF::Init()
{
    // Try to load all files under the repository-level schemes/ directory.
    // In a real build you'd use an install path or resource; this is a development convenience.
    try {
        for (auto &p : std::filesystem::directory_iterator(std::filesystem::path("schemes"))) {
            if (p.is_regular_file()) {
                dict_.load(p.path().string());
            }
        }
    } catch (...) {
        // directory may not exist; still continue
    }
    return true;
}

void ScripaTSF::Uninit()
{
    // nothing to free for now
}

static std::wstring utf8_to_wstring(const std::string &s)
{
    // simple converter for common cases; for production use a proper UTF-8->UTF-16 conversion
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
    return conv.from_bytes(s);
}

bool ScripaTSF::OnKeyDown(wchar_t ch)
{
    // Only handle basic ASCII input here for the skeleton
    if (ch >= 0 && ch <= 127) {
        char c = static_cast<char>(ch);
        bool handled = engine_.inputChar(c);
        return handled;
    }
    // Not handled: let TSF or application process it
    return false;
}

void ScripaTSF::ToggleMode()
{
    engine_.toggleMode();
}

bool ScripaTSF::GetMode() const
{
    // true = IPA, false = ENG
    return engine_.getMode() == Engine::Mode::IPA;
}

std::wstring ScripaTSF::GetComposition() const
{
    // Engine::getBuffer() returns an ASCII-friendly std::string for now
    return utf8_to_wstring(engine_.getBuffer());
}

std::vector<std::wstring> ScripaTSF::GetCandidates() const
{
    auto cands = engine_.getCandidates();
    std::vector<std::wstring> out;
    out.reserve(cands.size());
    for (auto &u32 : cands) {
        // convert UTF-32 -> UTF-8 -> UTF-16
        std::string utf8 = utf32_to_utf8(u32);
        out.push_back(utf8_to_wstring(utf8));
    }
    return out;
}
