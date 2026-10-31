#!/usr/bin/env python3
"""Local Tumbleweed referee — pits two engines against each other over the
CodeCup protocol (one process per engine, stdin/stdout line dialogue).

Usage:
  python3 scripts/match.py --red /tmp/tw-v0 --white /tmp/tw-v2 --pairs 10

Implements the judge's rules closely enough for engine development:
  * random initial positions (inset from border, stacks non-adjacent)
  * placement legality: see >= 1 friend, no suicide, strict-greater capture
  * game end + scoring via the judge-style territory fill simulation
  * 30s cumulative clock per engine (measured on its own thinking time);
    exceeding it = default loss, as on CodeCup
"""

from __future__ import annotations

import argparse
import os
import random
import subprocess
import sys
import time

K = 11
STEPS = [(1, 0), (-1, 0), (0, 1), (0, -1), (1, 1), (-1, -1)]
RED, WHITE, NEUTRAL, EMPTY = 1, 2, 3, 0


def valid(c: int, r: int) -> bool:
    return 1 <= c <= K and 1 <= r <= K and abs(c - r) <= K // 2


def all_cells() -> list[tuple[int, int]]:
    return [(c, r) for c in range(1, K + 1) for r in range(1, K + 1) if valid(c, r)]


def neighbors(c: int, r: int) -> list[tuple[int, int]]:
    return [(c + dc, r + dr) for dc, dr in STEPS if valid(c + dc, r + dr)]


def on_border(c: int, r: int) -> bool:
    return any(not valid(nc, nr) for nc, nr in neighbors(c, r))


def hex_dist(a: tuple[int, int], b: tuple[int, int]) -> int:
    # map (c, r) -> axial (q=c, t=r-c), standard axial distance
    da = b[0] - a[0]
    db = (b[1] - b[0]) - (a[1] - a[0])
    return (abs(da) + abs(db) + abs(da + db)) // 2


def label(c: int, r: int) -> str:
    return f"{chr(ord('A') + c - 1)}{r}"


class Board:
    def __init__(self) -> None:
        self.cell: dict[tuple[int, int], tuple[int, int]] = {}  # (c,r) -> (color, value)
        self.cell[(6, 6)] = (NEUTRAL, 2)

    def seen(self, color: int, c: int, r: int) -> int:
        count = 0
        for dc, dr in STEPS:
            cc, rr = c + dc, r + dr
            while valid(cc, rr) and (cc, rr) not in self.cell:
                cc, rr = cc + dc, rr + dr
            if valid(cc, rr) and self.cell.get((cc, rr), (EMPTY, 0))[0] == color:
                count += 1
        return count

    def legal(self, color: int, c: int, r: int) -> bool:
        if not valid(c, r):
            return False
        occupant = self.cell.get((c, r))
        v = self.seen(color, c, r)
        if v < 1:
            return False
        opp = WHITE if color == RED else RED
        if v < self.seen(opp, c, r):
            return False
        if occupant is not None and v <= occupant[1]:
            return False
        return True

    def apply(self, color: int, c: int, r: int) -> None:
        self.cell[(c, r)] = (color, self.seen(color, c, r))

    def fill(self, color: int) -> None:
        while True:
            move = next(
                ((c, r) for c in range(1, K + 1) for r in range(1, K + 1) if self.legal(color, c, r)),
                None,
            )
            if move is None:
                return
            self.apply(color, *move)

    def territory(self, target: int) -> int:
        b = Board()
        b.cell = dict(self.cell)
        b.fill(WHITE if target == RED else RED)
        b.fill(target)
        return sum(1 for v in b.cell.values() if v[0] == target)


class Engine:
    def __init__(self, cmd: str, name: str = "engine") -> None:
        self.p = subprocess.Popen(
            [cmd], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1
        )
        self.used_ms = 0
        self.log = (
            open(f"/tmp/tw-{name}.log", "w")
            if os.environ.get("TW_DEBUG")
            else None
        )

    def send(self, line: str) -> None:
        if self.log:
            self.log.write(f"> {line}\n")
            self.log.flush()
        self.p.stdin.write(line + "\n")
        self.p.stdin.flush()

    def recv_move(self) -> str:
        t0 = time.monotonic()
        line = self.p.stdout.readline().strip()
        self.used_ms += int((time.monotonic() - t0) * 1000)
        if self.log:
            self.log.write(f"< {line}\n")
            self.log.flush()
        return line

    def close(self) -> None:
        try:
            self.p.kill()
        except OSError:
            pass


def play_game(red_cmd: str, white_cmd: str, rng: random.Random) -> tuple[int, str, int, int]:
    """Returns (winner, note, red_territory, white_territory). winner: 1 red, 2 white, 0 draw."""
    cells = [c for c in all_cells() if not on_border(*c)]
    while True:
        (rc, rr), (wc, wr) = rng.sample(cells, 2)
        if hex_dist((rc, rr), (wc, wr)) >= 2:
            break

    red, white = Engine(red_cmd, "red"), Engine(white_cmd, "white")
    board = Board()
    board.cell[(rc, rr)] = (RED, 1)
    board.cell[(wc, wr)] = (WHITE, 1)

    for e, line in ((red, label(rc, rr)), (red, label(wc, wr)), (red, "Start")):
        e.send(line)
    for e, line in ((white, label(rc, rr)), (white, label(wc, wr))):
        e.send(line)

    winner, note = 0, ""
    turn_is_red = True
    tr, tw = 0, 0
    try:
        for _ in range(300):
            turn, other = (red, white) if turn_is_red else (white, red)
            color = RED if turn_is_red else WHITE
            move = turn.recv_move()
            if turn.used_ms > 30_000:
                winner, note = (2 if turn_is_red else 1), "timeout"
                break
            if move == "Quit" or not move:
                winner, note = (2 if turn_is_red else 1), "quit/eof"
                break
            try:
                c, r = parse(move)
            except (ValueError, IndexError):
                winner, note = (2 if turn_is_red else 1), "unparseable output"
                break
            if not board.legal(color, c, r):
                if os.environ.get("TW_DEBUG"):
                    bad = label(c, r)
                    print(f"DEBUG ply: {'red' if turn_is_red else 'white'} played {bad} — "
                          f"seen={board.seen(color, c, r)} occupant={board.cell.get((c, r))}",
                          file=sys.stderr)
                winner, note = (2 if turn_is_red else 1), "illegal move"
                break
            board.apply(color, c, r)
            other.send(move)
            turn_is_red = not turn_is_red

            # Judge rule: the game ends when every cell belongs to either
            # territory after a move — this happens BEFORE the board is
            # physically full. Check after every move; if closed, send Quit
            # and score from the territory counts.
            tr, tw = board.territory(RED), board.territory(WHITE)
            if tr + tw >= 91:
                red.send("Quit")
                white.send("Quit")
                if tr == tw:
                    winner, note = 0, "territory draw"
                else:
                    winner, note = (1 if tr > tw else 2), "territory closure"
                break
            if len(board.cell) >= 91:
                break
    finally:
        red.close()
        white.close()

    if winner == 0:
        tr, tw = board.territory(RED), board.territory(WHITE)
        if tr == tw:
            winner, note = 0, "draw"
        else:
            winner = 1 if tr > tw else 2
            note = "territory"
    return winner, note, tr, tw


def parse(move: str) -> tuple[int, int]:
    return ord(move[0]) - ord("A") + 1, int(move[1:])


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--red", required=True)
    ap.add_argument("--white", required=True)
    ap.add_argument("--pairs", type=int, default=10, help="games per color pairing")
    ap.add_argument("--seed", type=int, default=7)
    args = ap.parse_args()
    rng = random.Random(args.seed)

    score = {1: 0, 2: 0, 0: 0}
    margins: list[int] = []
    notes: dict[str, int] = {}
    for game in range(args.pairs * 2):
        red_cmd = args.red if game % 2 == 0 else args.white
        white_cmd = args.white if game % 2 == 0 else args.red
        winner, note, tr, tw = play_game(red_cmd, white_cmd, rng)
        # normalize to the --red engine's perspective
        if winner != 0:
            winner = winner if game % 2 == 0 else (1 if winner == 2 else 2)
        score[winner] += 1
        notes[note] = notes.get(note, 0) + 1
        red_margin = (tr - tw) if game % 2 == 0 else (tw - tr)
        margins.append(red_margin)
        who = "RED " if winner == 1 else ("WHITE" if winner == 2 else "DRAW")
        print(f"game {game + 1:3}: {who} ({note}, red-view margin {red_margin:+d})", flush=True)

    w, l, d = score[1], score[2], score[0]
    print(f"\n== --red engine: {w}W / {l}L / {d}D · avg margin {sum(margins) / len(margins):+.1f} ==")
    print("   ", dict(sorted(notes.items())))
    return 0


if __name__ == "__main__":
    sys.exit(main())
