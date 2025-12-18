#pragma once
#include "../core/Dic.hpp"
#include "../core/Engine.hpp"
#include "../core/Loader.hpp"
#include <string>
#include <vector>

// Minimal non-COM skeleton that shows where TSF integration should call into the engine.
// This file is intentionally lightweight: it focuses on wiring `Engine` and `Dictionary`.

class ScripaTSF {
public:
    ScripaTSF();
    ~ScripaTSF();

    // Initialize dictionary and engine. Returns false on failure.
    bool Init();
    void Uninit();

    // Handle a key (wide char). Returns true if the key was handled by the composition engine.
    bool OnKeyDown(wchar_t ch);

    // Toggle between ENG and IPA mode
    void ToggleMode();

    // Get current mode (true = IPA, false = ENG)
    bool GetMode() const;

    // Get the current composition (UTF-16) to be shown in the TSF composition
    std::wstring GetComposition() const;

    // Get candidates (UTF-16) for UI display
    std::vector<std::wstring> GetCandidates() const;

    // 字库管理接口
    void EnableScheme(const std::string& schemeName);
    void DisableScheme(const std::string& schemeName);
    bool IsSchemeEnabled(const std::string& schemeName) const;
    std::vector<std::string> GetAvailableSchemes() const;
    std::vector<std::string> GetEnabledSchemes() const;
    
    // 重新加载字库（在更改字库设置后调用）
    bool ReloadSchemes();

private:
    Dictionary dict_;
    Engine engine_ { &dict_ };
    SchemeLoader loader_;
    std::string schemes_path_ = "schemes/";  // 默认路径
};
