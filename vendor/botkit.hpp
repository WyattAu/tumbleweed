// botkit.hpp — header-only shared utilities for judge-protocol C++ bots.
//
// The shared layer of the WyattAu competition estate for C++ entries. Entry
// repos (e.g. WyattAu/tumbleweed) vendor this file at a pinned bot-kit commit
// and amalgamate it into a single source file for judges that require one
// (CodeCup, AHC). Stdlib only; no external dependencies by design.
//
//   bk::Timer          — wall-clock budget (contest clocks are per-game total)
//   bk::hex            — CodeCup-style labeled hex grid (columns A-K x rows 1-N)
//   bk::read_line      — line protocol input with CRLF stripping
//   bk::emit           — line protocol output with mandatory flush
//
// Judge-compile compatibility: GCC 15.2, `-std=c++26 -O2 -march=native`
// (CodeCup 2027); keep this header clean under `-Wall -Wextra -Wpedantic`.

#pragma once

#include <array>
#include <chrono>
#include <iostream>
#include <string>
#include <utility>

namespace bk {

// ---------------------------------------------------------------------------
// Wall-clock budget. Contest clocks (CodeCup: 30s per GAME total, AHC:
// per-testcase wall time) measure cumulative own-time — budget explicitly.
// ---------------------------------------------------------------------------

struct Timer {
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

    [[nodiscard]] long long elapsed_ms() const {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::steady_clock::now() - start)
            .count();
    }
    [[nodiscard]] bool over(long long budget_ms) const { return elapsed_ms() >= budget_ms; }
};

// ---------------------------------------------------------------------------
// Labeled hex grid: columns A.. run NE->SW, rows 1.. run W->E, cells exist
// iff |col - row| <= k. This is the CodeCup labeling (Tumbleweed 2027: A-K x
// 1-11, 91 cells, center F6). Third axis preserves col-row — validated
// against the official Tumbleweed sample game (red B2 -> E5 is a sightline).
// ---------------------------------------------------------------------------

template <int K>
struct HexLabelGrid {
    static constexpr int kMax = K;  // label space is K x K

    static constexpr std::array<std::pair<int, int>, 6> kSteps{{
        {1, 0}, {-1, 0},   // rows (W->E)
        {0, 1}, {0, -1},   // columns (NE->SW, constant letter)
        {1, 1}, {-1, -1},  // NW->SE diagonals (constant col-row)
    }};

    [[nodiscard]] static bool valid(int c, int r) {
        return c >= 1 && c <= K && r >= 1 && r <= K && (c - r >= -K / 2) && (c - r <= K / 2);
    }

    /// First stack hit walking each of the six rays from (c, r), or 0 if none.
    /// `occupied(c2, r2)` must report whether a cell holds a stack.
    template <typename Occupied>
    static int first_hit_count(int c, int r, const Occupied& occupied) {
        int count = 0;
        for (auto [dc, dr] : kSteps) {
            int cc = c + dc, rr = r + dr;
            while (valid(cc, rr) && !occupied(cc, rr)) {
                cc += dc;
                rr += dr;
            }
            if (valid(cc, rr)) ++count;
        }
        return count;
    }

    [[nodiscard]] static std::string label(int c, int r) {
        std::string s;
        s += static_cast<char>('A' + c - 1);
        s += std::to_string(r);
        return s;
    }

    /// Parse "E5" -> (5, 5). Returns false on garbage.
    [[nodiscard]] static bool parse(const std::string& s, int& c, int& r) {
        if (s.size() < 2) return false;
        c = s[0] - 'A' + 1;
        try {
            r = std::stoi(s.substr(1));
        } catch (...) {
            return false;
        }
        return valid(c, r);
    }
};

// ---------------------------------------------------------------------------
// Line protocol: judges speak strict newline dialogue; flush every move.
// ---------------------------------------------------------------------------

[[nodiscard]] inline std::string read_line() {
    std::string line;
    if (!std::getline(std::cin, line)) line = "Quit";
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
    return line;
}

inline void emit(const std::string& line) {
    std::cout << line << '\n' << std::flush;
}

}  // namespace bk
