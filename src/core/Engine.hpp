#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
// #include Dic.hpp

class Dictionary;

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
}

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

    return dict_->lookup(buffer_);
}

std::u32string Engine::chooseCandidate(size_t index)
{
    if (!dict_)
        return U"";

    auto cand = dict_->lookup(buffer_);
    if (index >= cand.size())
        return U"";

    std::u32string result = cand[index];

    buffer_.clear();

    return result;
}