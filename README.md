# tumbleweed

WyattAu's entry for **CodeCup 2027 — Tumbleweed** (codecup.nl). Hex board, 91
cells, stack placement via sightline majority; CodeCup variant rules in the
header of `tumbleweed.cpp`.

Estate architecture: shared C++ utilities live in
[`WyattAu/bot-kit`](https://github.com/WyattAu/bot-kit) (`cpp/botkit/botkit.hpp`,
header-only). CodeCup submissions must be a **single source file**, so this
repo vendors the header at a pinned commit (`BOTKIT_PIN` → `vendor/botkit.hpp`)
and amalgamates it into the submission artifact.

## Commands

```sh
g++ -std=c++23 -O2 -Wall -Wextra -Wpedantic -o /tmp/tw tumbleweed.cpp   # dev build
printf 'B2\nI8\nStart\nQuit\n' | /tmp/tw                                 # protocol smoke
./scripts/amalgamate.sh          # -> submission/tumbleweed.cpp (upload this)
./scripts/update-botkit.sh       # refresh vendored botkit.hpp, re-pin
```

CI compiles both the dev form and the amalgamated artifact, protocol-smokes
both, and uploads the artifact — the artifact CI green-lights is the one you
upload to codecup.nl → "My submissions".

## Submission mechanics (CodeCup 2027)

- Single source file, ≤ 1,474,560 bytes; judge: GCC 15.2 `-std=c++26 -O2 -march=native` (our C++23-clean code is a subset — verify locally with `-std=c++26` before submitting)
- One program per game run; stdin/stdout line dialogue; **flush every move**
- 30s total per game (own thinking time only; no per-move limit; no threads/files/network/thinking-on-opponent-time)
- stderr (≤ ~10k chars) is visible to you on the game result page — log sparsely
- Test competitions every ~3 weeks (first: Oct 31, 2026); local matching via [Caia](https://www.codecup.nl/download_caia.php)

## Engine ladder

See `tumbleweed.cpp` header + `docs/` in `WyattAu/bot-kit`: v0 greedy material ✅
→ v1 sightline-difference eval → v2 judge-style territory simulation (the real
score function) → v3 alpha-beta on territory → v4 Caia self-play opening book.

## Related

- `WyattAu/bot-kit` — shared bot infrastructure (this repo's vendored header)
- `WyattAu/competitions` — the registry
- `WyattAu/estate.yml` — registered as `research / support`
