#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <iostream>

static std::u32string utf8_to_utf32(const std::string& utf8) //把8位变成32位
{
    std::u32string out;
    size_t i = 0, n = utf8.size();

    while (i < n) {
        unsigned char c = utf8[i];
        char32_t code = 0;
        size_t extra = 0;

        if (c < 0x80) {
            code = c;
            extra = 0;
        }
        else if ((c >> 5) == 0x6) { // 110xxxxx
            code = c & 0x1F;
            extra = 1;
        }
        else if ((c >> 4) == 0xE) { // 1110xxxx
            code = c & 0x0F;
            extra = 2;
        }
        else if ((c >> 3) == 0x1E) { // 11110xxx
            code = c & 0x07;
            extra = 3;
        }
        else {
            // invalid byte
            ++i;
            continue;
        }

        if (i + extra >= n) break;

        for (size_t j = 1; j <= extra; ++j) {
            unsigned char cc = utf8[i + j];
            code = (code << 6) | (cc & 0x3F);
        }

        out.push_back(code);
        i += extra + 1;
    }
    return out;
}

// 字典类 Dictionary class
class Dictionary {
public:
    bool load(const std::string& path); // 加载 scheme 文件
    std::vector<std::u32string> Lookup(const std::string& key) const; // 返回当前 key 的所有候选
    std::vector<std::u32string> LookupByPrefix(const std::string& prefix) const;
    void debugPrint() const;// 调试：打印整个字典
    private:
    std::unordered_map<std::string, std::vector<std::u32string>> dict_;
    // key: 输入法编码（如 "th", "aa", "ts"）
    // value: IPA 字符（UTF-32 形式） schemes
};


//执行层
bool Dictionary::load(const std::string& path)
{
    std::ifstream fin(path);
    if (!fin.is_open()) {
        std::cerr << "Failed to open scheme file: " << path << "\n";
        return false;
    }

    std::string line;
    size_t lineno = 0;

    while (std::getline(fin, line)) {
        ++lineno;   
        if (line.size() == 0 || line[0] == '#')
            continue; // 去掉注释（# 开头）
        std::istringstream iss(line);
        std::string key, value_utf8;

        if (!(iss >> key >> value_utf8)) {
            continue;    // 空行或格式错误
        }
        std::u32string value = utf8_to_utf32(value_utf8); // UTF-32 转码（例如 θ → U+03B8）
        dict_[key].push_back(value); // 插入到字典中
    }

    return true;
}

std::vector<std::u32string> Dictionary::Lookup(const std::string& key) const
{
    auto it = dict_.find(key);
    if (it == dict_.end())
        return {};

    return it->second;  
}

std::vector<std::u32string> Dictionary::LookupByPrefix(const std::string& prefix) const {
    std::vector<std::u32string> result;

    for (const auto& [key, values] : dict_) {
        if (key == prefix || key.rfind(prefix, 0) == 0) {
            result.insert(result.end(), values.begin(), values.end());
        }
    }

    // 去重
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());

    return result;
}
void Dictionary::debugPrint() const
{
    for (auto& [key, vec] : dict_) {
        std::cout << key << " : ";
        for (auto &u32 : vec)
            std::cout << "[UTF32 size=" << u32.size() << "] ";
        std::cout << "\n";
    }
}