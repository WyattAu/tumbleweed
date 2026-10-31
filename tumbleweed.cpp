// Tumbleweed — WyattAu CodeCup 2027 entry.
//
// CodeCup variant rules (codecup.nl/tumbleweed/rules.php — these DIFFER from
// the official game):
//   * Hex board, 6 per side, 91 cells. Columns A-K run NE->SW, rows 1-11 W->E.
//   * Start: neutral v2 stack at F6, red v1 and white v1 at judge-given cells.
//   * You output only the CELL; the placed value is auto-determined as the
//     number of friendly stacks the new stack sees (unobstructed straight line).
//   * Placing on an occupied cell (capture / friendly upgrade) requires
//     new value > existing value. Initial stacks must see >= 1 friendly.
//   * No suicide: your sightline count must be >= opponent's to that cell.
//   * Territory (judge-simulated) ends the game; score = territory cells
//     + 200/100 win/loss bonus. Invalid move or 30s TOTAL clock = default loss.
//
// Protocol (stdin/stdout, line-based, flush every move):
//   red:   read [red_init, white_init, "Start"]  -> print move
//   white: read [red_init, white_init, red_move] -> print move
//   then alternate: read opponent move, print move. "Quit" => exit.
//
// Submission: CodeCup requires ONE source file — `scripts/amalgamate.sh`
// inlines the pinned vendor/botkit.hpp into submission/tumbleweed.cpp.
//
// Engine ladder (docs/ENGINE.md):
//   v0 greedy material (this file) -> v1 sightline-difference eval ->
//   v2 judge-style territory simulation -> v3 alpha-beta on territory ->
//   v4 Caia self-play opening book.

#include "vendor/botkit.hpp"

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace tw {

constexpr int kLabels = 11;  // board label space: A-K x 1-11
using Grid = bk::HexLabelGrid<kLabels>;

enum Color : std::uint8_t { Empty = 0, Red = 1, White = 2, Neutral = 3 };

struct Stack {
    Color color;
    int value;
};

struct Board {
    std::array<std::array<Stack, kLabels + 1>, kLabels + 1> cell{};  // [c][r], 1-based

    [[nodiscard]] static bool valid(int c, int r) { return Grid::valid(c, r); }

    // Number of stacks of `color` visible from (c, r). Rays stop at the first
    // stack; the target cell's own occupant (if any) is never counted —
    // placement replaces it.
    [[nodiscard]] int seen(Color color, int c, int r) const {
        int count = 0;
        for (auto [dc, dr] : Grid::kSteps) {
            int cc = c + dc, rr = r + dr;
            while (Grid::valid(cc, rr) && cell[cc][rr].color == Empty) {
                cc += dc;
                rr += dr;
            }
            if (Grid::valid(cc, rr) && cell[cc][rr].color == color) ++count;
        }
        return count;
    }

    [[nodiscard]] int seen_opponent(Color me, int c, int r) const {
        return seen(me == Red ? White : Red, c, r);
    }

    [[nodiscard]] bool legal(Color me, int c, int r) const {
        if (!Grid::valid(c, r)) return false;
        const Stack& s = cell[c][r];
        const int v = seen(me, c, r);
        if (v < 1) return false;                            // must see a friend
        if (v < seen_opponent(me, c, r)) return false;      // no suicide
        if (s.color != Empty && v <= s.value) return false; // capture needs strictly greater
        return true;
    }

    // Apply a placement for `color` at (c, r). Value is auto-determined.
    void apply(Color color, int c, int r) { cell[c][r] = {color, seen(color, c, r)}; }
};

struct Engine {
    Board board;
    Color me = Red;
    Color opp = White;
    bk::Timer clock;  // 30s TOTAL per game — v0 spends microseconds; the
                      // search ladder may budget seconds per move from here.

    struct Move {
        int c = 0, r = 0;
        long long score = -1;
    };

    [[nodiscard]] std::vector<Move> legal_moves() const {
        std::vector<Move> moves;
        for (int c = 1; c <= kLabels; ++c) {
            for (int r = 1; r <= kLabels; ++r) {
                if (!board.legal(me, c, r)) continue;
                Move m{c, r, 0};
                const int v = board.seen(me, c, r);
                m.score = v;                                        // new material
                if (board.cell[c][r].color == opp) {
                    m.score += 3LL * board.cell[c][r].value;        // stripping enemy stacks
                } else if (board.cell[c][r].color == Neutral) {
                    m.score += 2LL * board.cell[c][r].value;        // claiming the big neutral
                }
                // TODO(v1): prefer placements where (seen(me) - seen(opp)) is
                // maximal, and penalize leaving cheap recaptures nearby.
                // TODO(v2): replicate the judge's territory simulation for the
                // true score. TODO(v3): alpha-beta on territory eval.
                moves.push_back(m);
            }
        }
        return moves;
    }

    [[nodiscard]] Move best() const {
        Move best;
        for (const Move& m : legal_moves()) {
            if (m.score > best.score) best = m;
        }
        return best;  // judge guarantees a legal move exists on your turn
    }
};

}  // namespace tw

int main() {
    using namespace tw;

    Engine engine;
    engine.me = Red;
    engine.opp = White;

    const std::string red_init = bk::read_line();
    const std::string white_init = bk::read_line();
    const std::string third = bk::read_line();

    int rc = 0, rr = 0, wc = 0, wr = 0;
    if (!Grid::parse(red_init, rc, rr) || !Grid::parse(white_init, wc, wr)) return 1;

    // Setup: neutral v2 at F6; initial red/white stacks are judge-given, v1.
    engine.board.cell[6][6] = {Neutral, 2};
    engine.board.cell[rc][rr] = {Red, 1};
    engine.board.cell[wc][wr] = {White, 1};

    // Play our best move: output it AND apply it — the board must reflect
    // our own stacks or we re-play the same cell forever.
    auto play = [&engine] {
        const auto m = engine.best();
        bk::emit(Grid::label(m.c, m.r));
        engine.board.apply(engine.me, m.c, m.r);
    };

    if (third == "Start") {
        // We are red and move first.
        play();
    } else {
        // We are white; `third` is red's first move.
        int c = 0, r = 0;
        if (!Grid::parse(third, c, r)) return 0;
        engine.board.apply(Red, c, r);
        play();
    }

    for (;;) {
        const std::string line = bk::read_line();
        if (line == "Quit") return 0;
        int c = 0, r = 0;
        if (!Grid::parse(line, c, r)) return 0;
        engine.board.apply(engine.opp, c, r);
        play();
    }
}
