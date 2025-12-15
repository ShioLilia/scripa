#include "Dic.hpp"
#include "Engine.hpp"
#include "Loader.hpp"
#include <iostream>
#include <string>
#ifdef _WIN32
#include <windows.h>
#endif

// status-line helper: print a single-line status that overwrites previous status
static size_t g_last_status_len = 0;
static void printStatusLine(const std::string& s) {
    // determine terminal width to avoid wrapping
    size_t width = 80;
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi)) {
        width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    }
#else
    struct winsize w;
    if (ioctl(0, TIOCGWINSZ, &w) == 0 && w.ws_col > 0) {
        width = w.ws_col;
    }
#endif

    std::string out = s;
        if (out.size() > width && width > 1) {
            // keep the tail part which is likely more relevant; prefix with UTF-8 ellipsis
            size_t tail = width - 1;
            std::string tailstr = out.substr(out.size() - tail);
            // UTF-8 ellipsis bytes
            const std::string ell = "\xE2\x80\xA6";
            out = ell + tailstr;
            // if resulting byte length still exceeds width, keep the last `width` bytes
            if (out.size() > width) out = out.substr(out.size() - width);
    }

    std::cout << '\r' << out;
    if (g_last_status_len > out.size()) std::cout << std::string(g_last_status_len - out.size(), ' ');
    std::cout << std::flush;
    g_last_status_len = out.size();
}

int main() {
    // Ensure Windows console uses UTF-8 so IPA characters render correctly
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    // Keep input echo enabled so user sees typed ASCII buffer.
#endif
    Dictionary dict;
    SchemeLoader loader;
    int count = loader.loadSchemes("schemes/", dict);
    std::cout << "Loaded scheme files: " << count << "\n";
    Engine engine(&dict);

    std::cout << "Type characters; press Space to commit first candidate. Ctrl+C to exit.\n";

    while (true) {
        char c;
        if (!std::cin.get(c)) break; // read including spaces
        // Handle special commit keys (space / Enter) in IPA mode BEFORE feeding char to engine
        if (engine.getMode() == Engine::Mode::IPA && (c == ' ' || c == '\r' || c == '\n')) {
            auto cand = engine.getCandidates();
            if (!cand.empty()) {
                // erase any echoed ASCII input (buffer) from the console before printing candidate
                std::string buf = engine.getBuffer();
                size_t blen = buf.size();
                for (size_t i = 0; i < blen; ++i) {
                    // move back, overwrite with space, move back again
                    std::cout << '\b' << ' ' << '\b';
                }

                size_t chosen_index = 0;
                if (cand.size() == 1) {
                    chosen_index = 0;
                } else {
                    // present numbered candidate list below and wait for user selection
                    // clear status line so list appears below prompt
                    std::cout << '\r';
                    if (g_last_status_len > 0) std::cout << std::string(g_last_status_len, ' ');
                    std::cout << '\r' << std::flush;
                    // print numbered candidates
                    size_t ci = 1;
                    for (auto &u : cand) {
                        std::string s = utf32_to_utf8(u);
                        if (s.size() == 0) s = "(empty)";
                        std::cout << "[" << ci << "] " << s << std::endl;
                        ++ci;
                    }
                    std::cout << "Select [1-" << cand.size() << "] or press Enter/Space for first: " << std::flush;

                    // read selection (allow multi-digit, read until newline)
                    std::string selline;
                    if (!std::getline(std::cin, selline)) break;
                    if (selline.empty()) {
                        chosen_index = 0;
                    } else {
                        try {
                            int v = std::stoi(selline);
                            if (v >= 1 && static_cast<size_t>(v) <= cand.size()) chosen_index = static_cast<size_t>(v - 1);
                            else chosen_index = 0;
                        } catch (...) {
                            chosen_index = 0;
                        }
                    }
                }

                auto res = engine.chooseCandidate(chosen_index);
                std::string out = utf32_to_utf8(res);
                // clear status line fully, then print committed candidate on its own line
                std::cout << '\r';
                if (g_last_status_len > 0) std::cout << std::string(g_last_status_len, ' ');
                std::cout << '\r' << out << std::endl << std::flush;
                // reset status length so next status starts fresh
                g_last_status_len = 0;
            } else {
                // no candidate: output space or newline accordingly
                if (c == ' ') std::cout << ' ' << std::flush;
                else std::cout << std::endl;
            }
            continue;
        }

        bool direct = engine.inputChar(c);

        if (engine.getMode() == Engine::Mode::ENG) {
            if (direct) {
                if (c == '\r' || c == '\n') std::cout << std::endl;
                else std::cout << c << std::flush;
            }
            continue;
        }

        // otherwise, show candidate strings (converted to UTF-8) for debugging
        auto cand = engine.getCandidates();
        std::string status = "Candidates: ";
        size_t ci = 1;
        for (auto &u : cand) {
            std::string s = utf32_to_utf8(u);
            if (s.size() == 0) s = "(empty)";
            status += std::string("[U32:") + std::to_string(u.size()) + "] " + s + " [" + std::to_string(ci) + "] ";
            ++ci;
        }
        printStatusLine(status);
    }
// done
    return 0;
}