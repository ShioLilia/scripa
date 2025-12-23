#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <cstddef>
#include <algorithm>
#include "Dic.hpp"
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif
//maxmin宏污染

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
    void deleteLastChar() { if (!buffer_.empty()) buffer_.pop_back(); } //删除最后一个字符
    Mode getMode() const { return mode_; } //debug用的，返回mode值


private:
    Dictionary* dict_;
    std::string buffer_;
    Mode mode_;
};
// 执行层
inline Engine::Engine(Dictionary* dict)
    : dict_(dict), mode_(Mode::IPA)
{
}

inline void Engine::toggleMode() {
    if (mode_ == Mode::IPA)
        mode_ = Mode::ENG;
    else
        mode_ = Mode::IPA;
    buffer_.clear();
}

inline bool Engine::inputChar(char c)
{
    if (mode_ == Mode::ENG) {
        buffer_.push_back(c);
        return true; // 那还说什么了，直接给了
    }
    if (c == ' ') {
        // 空格键：提交第一个候选
        return true;
    }
    buffer_.push_back(c);
    // 返回是否有候选（总是返回 true 让 UI 刷新）
    return true;
}

inline std::vector<std::u32string> Engine::getCandidates() const
{
    if (mode_ == Mode::ENG)
        return {};

    if (!dict_)
        return {};

    // Helper: get digit count in T-pattern (e.g., T132 -> 3, T1 -> 1, not T-pattern -> 0)
    auto getT_digitCount = [](const std::string &buf) -> int {
        if (buf.empty() || buf[0] != 'T') return 0;
        size_t i = 1;
        int count = 0;
        while (i < buf.size() && buf[i] >= '1' && buf[i] <= '5') {
            count++;
            i++;
        }
        if (count == 0) return 0;  // no digits after T
        if (i == buf.size() || buf[i] == 'T') return count;  // valid T-pattern
        return 0;  // invalid
    };

    // Count T-patterns in buffer
    auto countT_patternsInBuffer = [&]() -> int {
        int count = 0;
        size_t i = 0;
        while (i < buffer_.size()) {
            if (buffer_[i] == 'T' && i + 1 < buffer_.size() && buffer_[i+1] >= '1' && buffer_[i+1] <= '5') {
                count++;
                i++;
                while (i < buffer_.size() && buffer_[i] >= '1' && buffer_[i] <= '5') i++;
                if (i < buffer_.size() && buffer_[i] == 'T') i++;
            } else {
                i++;
            }
        }
        return count;
    };

    int totalT_patterns = countT_patternsInBuffer();

    // Collect: (result_u32, num_T_converted, max_T_digits, num_total_converted, was_in_dict)
    // where num_T_converted = count of converted T-pattern segments
    // max_T_digits = maximum digit count in any T-pattern (e.g., T132 -> 3, T12 -> 2)
    // num_total_converted = total transformation count across all segments
    std::vector<std::tuple<std::u32string, int, int, int, bool>> collected;

    // exact match
    {
        auto exact = dict_->Lookup(buffer_);
        if (exact.empty() && !buffer_.empty()) {
            exact.push_back(utf8_to_utf32(buffer_));
        }
        
        for (const auto &e : exact) {
            bool was_in_dict = (utf32_to_utf8(e) != buffer_);
            int t_converted = (was_in_dict && getT_digitCount(buffer_) > 0) ? 1 : 0;
            int t_digits = getT_digitCount(buffer_);
            int total_converted = was_in_dict ? 1 : 0;
            collected.emplace_back(e, t_converted, t_digits, total_converted, was_in_dict);
        }
    }

    // segmentation
    size_t n = buffer_.size();
    if (n >= 2) {
        std::vector<std::string> parts;
        std::function<void(size_t)> dfs = [&](size_t pos) {
            if (pos == n) {
                // Accumulate all candidates from this segmentation
                std::vector<std::tuple<std::u32string, int, int, int, bool>> accum;
                accum.push_back({U"", 0, 0, 0, false});
                
                for (const auto& p : parts) {
                    auto v = dict_->Lookup(p);
                    if (v.empty() && !p.empty()) {
                        v.push_back(utf8_to_utf32(p));
                    }
                    if (v.empty()) return;
                    
                    std::vector<std::tuple<std::u32string, int, int, int, bool>> next;
                    for (const auto& a : accum) {
                        for (const auto& vv : v) {
                            std::string vv_utf8 = utf32_to_utf8(vv);
                            bool was_in_dict = (vv_utf8 != p);
                            // Track T-pattern conversions
                            int p_t_digits = getT_digitCount(p);
                            int new_t_converted = std::get<1>(a) + (was_in_dict && p_t_digits > 0 ? 1 : 0);
                            int new_max_t_digits = (std::get<2>(a) > p_t_digits) ? std::get<2>(a) : p_t_digits;
                            int new_total_converted = std::get<3>(a) + (was_in_dict ? 1 : 0);
                            bool new_in_dict = std::get<4>(a) || was_in_dict;
                            next.emplace_back(
                                std::get<0>(a) + vv,
                                new_t_converted,
                                new_max_t_digits,
                                new_total_converted,
                                new_in_dict
                            );
                        }
                    }
                    accum.swap(next);
                }
                
                for (const auto &x : accum) collected.push_back(x);
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

    // Consolidate by UTF-8 string, keeping best
    std::unordered_map<std::string, std::tuple<std::u32string, int, int, int, bool>> best;
    for (auto &item : collected) {
        std::string key = utf32_to_utf8(std::get<0>(item));
        auto it = best.find(key);
        if (it == best.end()) {
            best[key] = item;
        } else {
            int old_t = std::get<1>(it->second);
            int new_t = std::get<1>(item);
            int old_t_digits = std::get<2>(it->second);
            int new_t_digits = std::get<2>(item);
            int old_total = std::get<3>(it->second);
            int new_total = std::get<3>(item);
            
            // Keep if: more T converted, or same T but more T digits, or same T/digits but more total
            if (new_t > old_t || 
                (new_t == old_t && new_t_digits > old_t_digits) ||
                (new_t == old_t && new_t_digits == old_t_digits && new_total > old_total)) {
                best[key] = item;
            }
        }
    }

    // Sort: by num_T_converted desc, then by max_T_digits desc, then by total_converted desc, then by key
    std::vector<std::tuple<std::u32string, int, int, int, bool>> out;
    out.reserve(best.size());
    for (auto &kv : best) out.push_back(kv.second);
    
    std::sort(out.begin(), out.end(), [](const auto &a, const auto &b){
        int a_t = std::get<1>(a);
        int b_t = std::get<1>(b);
        if (a_t != b_t) return a_t > b_t;  // more T conversions first
        
        int a_t_digits = std::get<2>(a);
        int b_t_digits = std::get<2>(b);
        if (a_t_digits != b_t_digits) return a_t_digits > b_t_digits;  // more T digits first
        
        int a_total = std::get<3>(a);
        int b_total = std::get<3>(b);
        if (a_total != b_total) return a_total > b_total;  // more total conversions
        
        return utf32_to_utf8(std::get<0>(a)) < utf32_to_utf8(std::get<0>(b));
    });

    // build result vector
    std::vector<std::u32string> result;
    result.reserve(out.size());
    for (auto &p : out) result.push_back(std::get<0>(p));
    if (result.size() > 100) result.resize(100);
    return result;
}

inline std::u32string  Engine::chooseCandidate(size_t index)
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