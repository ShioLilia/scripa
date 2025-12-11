#include "Dic.hpp"
#include "Engine.hpp"
#include <iostream>

int main() {
    Dictionary dict;
    dict.load("scheme/kunyomi.txt");

    Engine engine(&dict);

    while (true) {
        char c;
        std::cin >> c;
        engine.inputChar(c);

        auto cand = engine.getCandidates();
        std::cout << "Candidates: ";
        for (auto &u : cand) {
            std::cout << "(U32:" << u.size() << ") ";
        }
        std::cout << "\n";
    }
}