// Conformance bridge — tests/conformance.cpp
//
// Property tests mirroring the Lean 4 formal model of the Tumbleweed rules
// (WyattAu/game-spec, src/GameSpec/Tumbleweed.lean, iterations 1-7). Each
// test is named after the Lean theorem it mirrors. The bridge compiles the
// EXACT dev engine source (tumbleweed.cpp — the amalgamation pipeline
// verifies the submission artifact is derived from it), so it can never
// drift from the shipped artifact.
//
//   #define main engine_main + #include "../tumbleweed.cpp"
//
// Lean lemma -> C++ property map:
//   seen_place_self        -> placing never changes what the placed cell sees
//   seen_le_six            -> sightline counts are in [0, 6]
//   legalPlacement         -> legal() iff the four CodeCup conditions hold
//   totalValue_place_gt    -> a legal placement strictly increases the total
//   place_zero_ok          -> empty cells carry value 0 after a placement
//   place_bound_ok         -> every stack value stays in [0, 6]
//   fill_mono              -> the judge fill never decreases the total
//   fillStep_increase      -> every effective fill step adds >= 1
//   fill_stable_of_big     -> the fill loop terminates in <= 547 iterations
//   fill_const_of_none     -> a saturated fill is a fixpoint (no legal cells)
//   fill_547_empty         -> an empty board is already the fill fixpoint
//   (Iteration-1 sample)   -> B2/E5 sightline on the col-row axis, k=0 excluded

#define main engine_main
#include "../tumbleweed.cpp"
#undef main

#include <cstdio>
#include <cstdlib>
#include <random>
#include <utility>
#include <vector>

namespace cf {

int g_checks = 0;
int g_fail = 0;

#define CHECK(cond, name, detail)                                                     \
    do {                                                                              \
        ++g_checks;                                                                   \
        if (!(cond)) {                                                                \
            ++g_fail;                                                                 \
            std::printf("[FAIL] %-24s %s (seed %u, board %d, %s)\n", name, detail,    \
                        g_seed, g_board_idx, #cond);                                  \
        }                                                                             \
    } while (0)

unsigned g_seed = 0;
int g_board_idx = 0;

long long total_value(const tw::Board& b) {
    long long s = 0;
    for (int c = 1; c <= tw::kLabels; ++c)
        for (int r = 1; r <= tw::kLabels; ++r) s += std::llabs(b.cell[c][r].value);
    return s;
}

bool boards_equal(const tw::Board& x, const tw::Board& y) {
    for (int c = 1; c <= tw::kLabels; ++c)
        for (int r = 1; r <= tw::kLabels; ++r)
            if (x.cell[c][r].color != y.cell[c][r].color ||
                x.cell[c][r].value != y.cell[c][r].value)
                return false;
    return true;
}

/// Bounded mirror of Engine::fill with per-step instrumentation.
struct FillReport {
    int steps = 0;          // scan iterations (placed or not) — the Lean fuel
    long long placed = 0;   // effective placements
    bool ok = true;
};

FillReport fill_bounded(tw::Board& b, tw::Color color) {
    FillReport rep;
    long long prev = total_value(b);
    for (;;) {
        bool placed = false;
        for (int c = 1; c <= tw::kLabels && !placed; ++c) {
            for (int r = 1; r <= tw::kLabels && !placed; ++r) {
                if (b.legal(color, c, r)) {
                    b.apply(color, c, r);
                    placed = true;
                }
            }
        }
        ++rep.steps;
        const long long now = total_value(b);
        if (now < prev) rep.ok = false;                                   // fill_mono
        if (placed) {
            if (now <= prev) rep.ok = false;                              // fillStep_increase
            ++rep.placed;
        }
        prev = now;
        if (!placed) break;
        if (rep.steps >= 547) { rep.ok = false; break; }                  // fill_stable_of_big
    }
    return rep;
}

tw::Board random_board(std::mt19937& rng, int density_pct) {
    tw::Board b;
    std::uniform_int_distribution<int> roll(0, 99), color(0, 5), value(0, 6);
    for (int c = 1; c <= tw::kLabels; ++c) {
        for (int r = 1; r <= tw::kLabels; ++r) {
            if (!tw::Grid::valid(c, r) || roll(rng) >= density_pct) continue;
            const int col = color(rng);  // 0 empty, 1 red, 2 white, 3 neutral, 4-5 empty
            if (col == 0 || col == 4 || col == 5) continue;
            b.cell[c][r] = {static_cast<tw::Color>(col), value(rng)};
        }
    }
    return b;
}

/// A position reachable in real play: initial setup + alternating legal moves.
tw::Board game_board(std::mt19937& rng, int nmoves) {
    tw::Board b;
    b.cell[6][6] = {tw::Neutral, 2};
    std::uniform_int_distribution<int> label(1, 11);
    for (tw::Color col : {tw::Red, tw::White}) {
        for (;;) {
            const int c = label(rng), r = label(rng);
            if (tw::Grid::valid(c, r) && b.cell[c][r].color == tw::Empty) {
                b.cell[c][r] = {col, 1};
                break;
            }
        }
    }
    tw::Color turn = tw::Red;
    for (int i = 0; i < nmoves; ++i) {
        std::vector<std::pair<int, int>> moves;
        for (int c = 1; c <= tw::kLabels; ++c)
            for (int r = 1; r <= tw::kLabels; ++r)
                if (b.legal(turn, c, r)) moves.push_back({c, r});
        if (moves.empty()) break;
        std::uniform_int_distribution<size_t> pick(0, moves.size() - 1);
        const auto [mc, mr] = moves[pick(rng)];
        b.apply(turn, mc, mr);
        turn = (turn == tw::Red) ? tw::White : tw::Red;
    }
    return b;
}

// -- properties ------------------------------------------------------------

/// Lean: seen_place_self + seen_le_six
static void sweep_seen(const tw::Board& b) {
    for (int c = 1; c <= tw::kLabels; ++c) {
        for (int r = 1; r <= tw::kLabels; ++r) {
            if (!tw::Grid::valid(c, r)) continue;
            const int sr = b.seen(tw::Red, c, r);
            const int sw = b.seen(tw::White, c, r);
            CHECK(sr >= 0 && sr <= 6, "seen_le_six", "red");
            CHECK(sw >= 0 && sw <= 6, "seen_le_six", "white");
            for (tw::Color pc : {tw::Red, tw::White, tw::Neutral}) {
                tw::Board t = b;
                const int before[3] = {t.seen(tw::Red, c, r), t.seen(tw::White, c, r),
                                       t.seen(tw::Neutral, c, r)};
                t.apply(pc, c, r);
                const int after[3] = {t.seen(tw::Red, c, r), t.seen(tw::White, c, r),
                                      t.seen(tw::Neutral, c, r)};
                CHECK(before[0] == after[0] && before[1] == after[1] && before[2] == after[2],
                      "seen_place_self", "keystone");
            }
        }
    }
}

/// Lean: legalPlacement (full iff) + placement invariant preservation
static void sweep_legal_and_place(const tw::Board& b) {
    const long long total0 = total_value(b);
    for (tw::Color me : {tw::Red, tw::White}) {
        const tw::Color opp = (me == tw::Red) ? tw::White : tw::Red;
        for (int c = 1; c <= tw::kLabels; ++c) {
            for (int r = 1; r <= tw::kLabels; ++r) {
                if (!tw::Grid::valid(c, r)) continue;
                const int v = b.seen(me, c, r);
                const int vo = b.seen(opp, c, r);
                const tw::Stack& s = b.cell[c][r];
                const bool expect =
                    v >= 1 && v >= vo && (s.color == tw::Empty || v > s.value);
                CHECK(b.legal(me, c, r) == expect, "legalPlacement", "iff");
                if (!b.legal(me, c, r)) continue;
                tw::Board t = b;
                t.apply(me, c, r);
                CHECK(total_value(t) > total0, "totalValue_place_gt", "strict");
                for (int cc = 1; cc <= tw::kLabels; ++cc) {
                    for (int rr = 1; rr <= tw::kLabels; ++rr) {
                        if (!tw::Grid::valid(cc, rr)) continue;
                        const tw::Stack& ts = t.cell[cc][rr];
                        CHECK(ts.value >= 0 && ts.value <= 6, "place_bound_ok", "range");
                        if (ts.color == tw::Empty) CHECK(ts.value == 0, "place_zero_ok", "zero");
                    }
                }
            }
        }
    }
}

/// Lean: fill_mono + fillStep_increase + fill_stable_of_big + fill_const_of_none
static void sweep_fill(const tw::Board& b) {
    for (tw::Color color : {tw::Red, tw::White}) {
        tw::Board m = b;
        const FillReport rep = fill_bounded(m, color);
        CHECK(rep.ok, "fill_mono/increase/stable", "instrumented");
        CHECK(rep.placed <= 546, "fill_stable_of_big", "placed <= 546");
        tw::Board a = b;                                    // the artifact's own fill
        tw::Engine::fill(a, color);
        CHECK(boards_equal(a, m), "fill_conformance", "artifact == mirror");
        // fixpoint: no legal cell remains; re-running fill is a no-op
        bool any_legal = false;
        for (int c = 1; c <= tw::kLabels && !any_legal; ++c)
            for (int r = 1; r <= tw::kLabels && !any_legal; ++r)
                if (a.legal(color, c, r)) any_legal = true;
        CHECK(!any_legal, "fill_const_of_none", "no legal cells at fixpoint");
        tw::Board a2 = a;
        tw::Engine::fill(a2, color);
        CHECK(boards_equal(a2, a), "fill_const_of_none", "idempotent");
    }
}

/// Lean: fill_547_empty (empty board is the fill fixpoint)
static void test_empty_board() {
    tw::Board b;
    for (tw::Color color : {tw::Red, tw::White}) {
        tw::Board m = b;
        const FillReport rep = fill_bounded(m, color);
        CHECK(rep.ok && rep.placed == 0 && rep.steps == 1, "fill_547_empty", "immediate none");
        CHECK(boards_equal(m, b), "fill_547_empty", "unchanged");
        CHECK(tw::Engine::territory(b, color) == 0, "fill_547_empty", "territory 0");
    }
}

/// Iteration-1 sample regression: B2 and E5 share the col-row axis (c - r = 0);
/// a stack never sees itself (k = 0 excluded).
static void test_sample_sightline() {
    tw::Board b;
    b.cell[2][2] = {tw::Red, 1};  // B2
    b.cell[5][5] = {tw::Red, 1};  // E5
    CHECK(b.seen(tw::Red, 3, 3) == 2, "sample_B2_E5", "C3 sees both");
    CHECK(b.seen(tw::Red, 4, 4) == 2, "sample_B2_E5", "D4 sees both");
    CHECK(b.seen(tw::Red, 2, 2) == 1, "sample_B2_E5", "B2 sees E5, not itself");
    CHECK(b.seen(tw::Red, 5, 5) == 1, "sample_B2_E5", "E5 sees B2, not itself");
    // The {1,0}/{0,1} families must NOT connect B2/E5 (they are not on a
    // constant-column or constant-row ray).
    tw::Board c1;
    c1.cell[2][2] = {tw::Red, 1};
    c1.cell[5][2] = {tw::Red, 1};
    CHECK(c1.seen(tw::Red, 3, 2) == 2, "sample_axis_row", "B2/B5 row ray");
    tw::Board c2;
    c2.cell[2][2] = {tw::Red, 1};
    c2.cell[2][5] = {tw::Red, 1};
    CHECK(c2.seen(tw::Red, 2, 3) == 2, "sample_axis_col", "B2/B2-col ray");
}

}  // namespace cf

int main() {
    using namespace cf;

    std::printf("conformance bridge: game-spec Lean model <-> tumbleweed.cpp\n");

    test_empty_board();
    test_sample_sightline();

    std::mt19937 rng;
    const unsigned seeds[] = {1, 2, 3, 4, 5};
    for (unsigned seed : seeds) {
        for (int density : {10, 25, 50}) {
            rng.seed(seed * 1000 + density);
            for (int rep = 0; rep < 3; ++rep) {
                g_seed = seed;
                g_board_idx = rep;
                const tw::Board b = random_board(rng, density);
                sweep_seen(b);
                sweep_legal_and_place(b);
                sweep_fill(b);
            }
            for (int moves : {5, 20, 60}) {
                rng.seed(seed * 7777 + moves);
                g_seed = seed;
                g_board_idx = 100 + moves;
                const tw::Board b = game_board(rng, moves);
                sweep_seen(b);
                sweep_legal_and_place(b);
                sweep_fill(b);
            }
        }
    }

    std::printf("%d checks, %d failures\n", g_checks, g_fail);
    return g_fail == 0 ? 0 : 1;
}
