#include "ScripaTSF.h"
#include "../core/Loader.hpp"
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
    // 使用 SchemeLoader 加载所有已启用的字库
    int count = loader_.loadSchemes(schemes_path_, dict_);
    std::cout << "[ScripaTSF] Loaded " << count << " scheme file(s)\n";
    return count > 0;
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

// 字库管理接口实现
void ScripaTSF::EnableScheme(const std::string& schemeName)
{
    loader_.enableScheme(schemeName);
}

void ScripaTSF::DisableScheme(const std::string& schemeName)
{
    loader_.disableScheme(schemeName);
}

bool ScripaTSF::IsSchemeEnabled(const std::string& schemeName) const
{
    return loader_.isSchemeEnabled(schemeName);
}

std::vector<std::string> ScripaTSF::GetAvailableSchemes() const
{
    return loader_.getAvailableSchemes(schemes_path_);
}

std::vector<std::string> ScripaTSF::GetEnabledSchemes() const
{
    return loader_.getEnabledSchemes();
}

bool ScripaTSF::ReloadSchemes()
{
    // 清空当前字典
    dict_.clear();
    
    // 重新加载所有启用的字库
    int count = loader_.loadSchemes(schemes_path_, dict_);
    std::cout << "[ScripaTSF] Reloaded " << count << " scheme file(s)\n";
    
    // 清空当前输入缓冲
    engine_.clearBuffer();
    
    return count > 0;
}
