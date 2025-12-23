#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <filesystem>
#include <iostream>

#include "Dic.hpp"

class SchemeLoader {
public:
    SchemeLoader();
    
    // 加载所有启用的 scheme 文件
    int loadSchemes(const std::string& dirPath, Dictionary& dict);
    
    // 字库启用/禁用管理
    void enableScheme(const std::string& schemeName);
    void disableScheme(const std::string& schemeName);
    bool isSchemeEnabled(const std::string& schemeName) const;
    
    // 获取所有可用字库名称（从文件系统扫描）
    std::vector<std::string> getAvailableSchemes(const std::string& dirPath) const;
    
    // 获取所有已启用的字库名称
    std::vector<std::string> getEnabledSchemes() const;
    
private:
    bool isSchemeFile(const std::filesystem::path& p) const;
    std::string getSchemeNameFromPath(const std::filesystem::path& p) const;
    
    std::unordered_set<std::string> enabled_schemes_;  // 存储已启用的字库名（不含扩展名）
};
// 执行层
inline SchemeLoader::SchemeLoader() {
    // 默认全部启用（通过空集合表示"全部启用"，或显式添加）
    // 我们采用显式方式：默认添加所有已知字库
    enabled_schemes_.insert("custom");
    enabled_schemes_.insert("simple");
    enabled_schemes_.insert("default");
    enabled_schemes_.insert("tones");
    // 用户可以通过扫描目录动态添加
}

inline bool SchemeLoader::isSchemeFile(const std::filesystem::path& p) const
{
    return p.extension() == ".txt";
}

inline std::string SchemeLoader::getSchemeNameFromPath(const std::filesystem::path& p) const
{
    return p.stem().string();  // 返回不含扩展名的文件名
}

inline void SchemeLoader::enableScheme(const std::string& schemeName) {
    enabled_schemes_.insert(schemeName);
}

inline void SchemeLoader::disableScheme(const std::string& schemeName) {
    enabled_schemes_.erase(schemeName);
}

inline bool SchemeLoader::isSchemeEnabled(const std::string& schemeName) const {
    return enabled_schemes_.find(schemeName) != enabled_schemes_.end();
}

inline std::vector<std::string> SchemeLoader::getAvailableSchemes(const std::string& dirPath) const {
    namespace fs = std::filesystem;
    std::vector<std::string> schemes;
    
    try {
        for (const auto& entry : fs::directory_iterator(dirPath)) {
            if (!entry.is_regular_file()) continue;
            auto path = entry.path();
            if (!isSchemeFile(path)) continue;
            schemes.push_back(getSchemeNameFromPath(path));
        }
    } catch (std::exception& e) {
        std::cerr << "SchemeLoader::getAvailableSchemes error: " << e.what() << "\n";
    }
    
    return schemes;
}

inline std::vector<std::string> SchemeLoader::getEnabledSchemes() const {
    std::vector<std::string> result;
    for (const auto& name : enabled_schemes_) {
        result.push_back(name);
    }
    return result;
}

inline int SchemeLoader::loadSchemes(const std::string& dirPath, Dictionary& dict)
{
    namespace fs = std::filesystem;
    int count = 0;
    try {
        for (const auto& entry : fs::directory_iterator(dirPath)) {
            if (!entry.is_regular_file()) continue;
            auto path = entry.path();
            if (!isSchemeFile(path)) continue;
            
            std::string schemeName = getSchemeNameFromPath(path);
            
            // 只加载已启用的字库
            if (!isSchemeEnabled(schemeName)) {
                std::cout << "[SchemeLoader] Skipping (disabled): "
                          << path.string() << "\n";
                continue;
            }
            
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