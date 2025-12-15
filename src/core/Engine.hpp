#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <cstddef>
#include <algorithm>
#include "Dic.hpp"

class Engine {
public:
    enum class Mode {
        ENG,
        IPA
    };
    
    explicit Engine(Dictionary* dict);
    bool inputChar(char c); //在输入时是否直接一比一输出
    void toggleMode(); //按capslock来切换模式@call
    std::vector<std::u32string> getCandidates() const; //获取当前候选栏内容
    std::u32string chooseCandidate(size_t index); //选择候选的某个
    std::string getBuffer() const { return buffer_; } //debug用的，返回buffer值
    void clearBuffer() { buffer_.clear(); } //然后清空buffer
    Mode getMode() const { return mode_; } //debug用的，返回mode值


private:
    Dictionary* dict_;
    std::string buffer_;
    Mode mode_;
};
// 执行层
Engine::Engine(Dictionary* dict)
    : dict_(dict), mode_(Mode::IPA)
{
}

void Engine::toggleMode() {
    if (mode_ == Mode::IPA)
        mode_ = Mode::ENG;
    else
        mode_ = Mode::IPA;
    buffer_.clear();
}

bool Engine::inputChar(char c)
{
    if (mode_ == Mode::ENG) {
        buffer_.push_back(c);
        return true; // 那还说什么了，直接给了
    }
    if (c == ' ') {
        // 空格键即为直接输入第一个候选内容
        return true;
    }
    buffer_.push_back(c);
    return false; //其他情况
}

std::vector<std::u32string> Engine::getCandidates() const
{
    if (mode_ == Mode::ENG)
        return {};

    if (!dict_)
        return {};

    // We'll collect candidates along with their transformation count
    // transformation count = number of segments where mapped output != original segment
    std::vector<std::pair<std::u32string, int>> collected;

    // exact matches: compute transform count (0 or 1)
    auto exact = dict_->Lookup(buffer_);
    for (const auto &e : exact) {
        int t = (utf32_to_utf8(e) != buffer_) ? 1 : 0;
        collected.emplace_back(e, t);
    }

    // try all possible segmentations and collect combinations with transform counts
    size_t n = buffer_.size();
    if (n >= 2) {
        std::vector<std::string> parts;
        std::function<void(size_t)> dfs = [&](size_t pos) {
            if (pos == n) {
                // for this segmentation parts, lookup candidates for each part
                std::vector<std::pair<std::u32string,int>> accum;
                accum.push_back({U"", 0});
                for (const auto& p : parts) {
                    auto v = dict_->Lookup(p);
                    if (v.empty()) return; // this segmentation yields nothing
                    std::vector<std::pair<std::u32string,int>> next;
                    for (const auto& a : accum) {
                        for (const auto& vv : v) {
                            int add = (utf32_to_utf8(vv) != p) ? 1 : 0;
                            next.emplace_back(a.first + vv, a.second + add);
                        }
                    }
                    accum.swap(next);
                }
                // push accum results with their transform counts
                for (const auto &x : accum) collected.emplace_back(x.first, x.second);
                return;
            }
            for (size_t len = 1; pos + len <= n; ++len) {
                parts.push_back(buffer_.substr(pos, len));
                dfs(pos + len);
                parts.pop_back();
            }
        };
        dfs(0);
    }

    // consolidate: for identical UTF-8 strings keep the maximum transform count
    std::unordered_map<std::string, std::pair<std::u32string,int>> best; // key utf8 -> (u32, transformCount)
    for (auto &pr : collected) {
        std::string key = utf32_to_utf8(pr.first);
        auto it = best.find(key);
        if (it == best.end() || pr.second > it->second.second) {
            best[key] = {pr.first, pr.second};
        }
    }

    // move into vector and sort by transformCount desc, then by key
    std::vector<std::pair<std::u32string,int>> out;
    out.reserve(best.size());
    for (auto &kv : best) out.push_back(kv.second);
    std::sort(out.begin(), out.end(), [](auto &a, auto &b){
        if (a.second != b.second) return a.second > b.second; // more transforms first
        return utf32_to_utf8(a.first) < utf32_to_utf8(b.first);
    });

    // build result vector
    std::vector<std::u32string> result;
    result.reserve(out.size());
    for (auto &p : out) result.push_back(p.first);
    // limit to at most 100 candidates to avoid explosion
    if (result.size() > 100) result.resize(100);
    return result;
}

std::u32string Engine::chooseCandidate(size_t index)
{
    if (!dict_)
        return U"";

    auto cand = getCandidates();
    if (index >= cand.size())
        return U"";

    std::u32string result = cand[index];
    buffer_.clear();
    return result;
}