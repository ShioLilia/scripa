#pragma once
#include <string>
#include <vector>
#include <filesystem>
#include <iostream>

#include "Dic.hpp"

class SchemeLoader {
    public:
    int loadSchemes(const std::string& dirPath, Dictionary& dict);
    private:
    bool isSchemeFile(const std::filesystem::path& p) const;
};
// 执行层
bool SchemeLoader::isSchemeFile(const std::filesystem::path& p) const
{
    return p.extension() == ".txt";
}
int SchemeLoader::loadSchemes(const std::string& dirPath, Dictionary& dict)
{
    namespace fs = std::filesystem;
    int count = 0;
    try {
        
        for (const auto& entry : fs::directory_iterator(dirPath)) {
            if (!entry.is_regular_file()) continue;
            auto path = entry.path();
            if (!isSchemeFile(path)) continue;
            std::cout << "[SchemeLoader] Loading: "
                      << path.string() << "\n";
            if (dict.load(path.string())) {
                count++;
            }
        }
    } 
    catch (std::exception& e) {
        std::cerr << "SchemeLoader error: " << e.what() << "\n";
    }
    return count;
}