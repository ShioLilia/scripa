#pragma once
#include "../core/Dic.hpp"
#include "../core/Engine.hpp"
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

    // Get the current composition (UTF-16) to be shown in the TSF composition
    std::wstring GetComposition() const;

    // Get candidates (UTF-16) for UI display
    std::vector<std::wstring> GetCandidates() const;

private:
    Dictionary dict_;
    Engine engine_ { &dict_ };
};
