# Tumbleweed — CodeCup 2027

Game: [Tumbleweed](https://www.codecup.nl/tumbleweed/rules.php) (Mike Zapawa, 2020) on a
91-cell hex board. Test competitions every ~3 weeks (first: **Oct 31, 2026**), final
spring 2027. Local match software: [Caia](https://www.codecup.nl/download_caia.php).

## CodeCup variant — differs from the official rules

| Aspect | CodeCup 2027 |
|---|---|
| Placement value | **Auto-determined** = friendly stacks seen. You output only the cell |
| Suicidal moves | **Forbidden** (official allows them) — your sightline count must be ≥ opponent's |
| Game end | Judge runs two territory-fill simulations after each move; ends when all cells are claimed |
| Start | Neutral v2 at F6 + judge-given v1 stacks (same per round; play both colors) |
| Clock | **30s total** per player per game |
| Score | Territory cells + 200/100 win/loss bonus; invalid move = default loss (`200 + winner territory + 0.9 × neutral`) |

## Protocol

```
red:   B2 / I8 / Start   → print move
white: B2 / I8 / <red move> → print move
loop:  read opponent move → print move; "Quit" ⇒ exit
```

Cells are `A`-`K` (columns, NE→SW) × `1`-`11` (rows, W→E). Cell exists iff `|col − row| ≤ 5`.

## Board model (encoded in `tumbleweed.cpp` (this repo))

- Hex steps in (col,row) space: `(±1,0) (0,±1) (+1,+1) (−1,−1)` — third axis preserves `c−r` (validated: official sample red `B2`→`E5` is a sightline)
- Sightline: ray along a step, blocked by the first stack; *seen* = friendly stacks hit
- Legality: `seen(me) ≥ 1` ∧ `seen(me) ≥ seen(opp)` ∧ (`target empty` ∨ `seen(me) > target.value`)

## Engine ladder

| v | What | State |
|---|------|-------|
| v0 | Greedy material: value + capture bonuses | ✅ skeleton |
| v1 | Prefer max `seen(me) − seen(opp)`; penalize recapturable placements | TODO |
| v2 | **Judge-style territory simulation** (fill-for-white → fill-for-red) as eval — this *is* the score function | TODO |
| v3 | Alpha-beta / MCTS over territory eval with 30s budget split | TODO |
| v4 | Caia self-play opening book (initial positions repeat within a round) | TODO |

## Strategy notes from the field

- The neutral F6 v2 stack is the biggest early target — but contesting it early
  can strand low-value stacks (v1 eval should model this).
- Captures reset a stack to *your* value = sightlines at capture time — capturing
  through dense friendly nets yields high-value stacks that are hard to retake.
- Endgame: the judge's fill simulation means "all my legal moves exhausted" is the
  real clock — maximize your remaining mobility while strangling theirs.
- Community: tw.antontobi.com has a live bot; the Tumbleweed Discord/Go crowd is
  where opening theory accumulates.
