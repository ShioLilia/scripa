#include "Dic.hpp"
#include "Engine.hpp"
#include "Loader.hpp"
#include <iostream>

int main() {
    Dictionary dict;
    SchemeLoader loader;
    int count = loader.loadSchemes("schemes/", dict);
    std::cout << "Loaded scheme files: " << count << "\n";
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