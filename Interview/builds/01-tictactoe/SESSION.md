# Build 1 · Tic-tac-toe in C++ — session state

Amazon loop prep, Job hunt 2026 → Daily plan, Thu 15 Oct, Deep 3 slot (build 1 moved here because
Deep 1 didn't run). Logical-and-maintainable round, mock format: Claude is the interviewer, I write the code.
Claude writes no code unless I say "show me".

Build: `g++ -std=c++20 -Wall -Wextra -Wpedantic -fsanitize=address,undefined -g -o ttt tictactoe.cpp`

## Briefing (short)

- Scored on: split of state / rules / I/O, naming and invariants, testing aloud, refactor (not patch)
  when an extension arrives.
- Don't build abstract hierarchies up front; small clean base first.
- "Constant-time win check" = incremental bookkeeping per move, not a faster scan. The classic trick has
  preconditions: when each extension arrives, ask whether it assumes a full line, and whether the
  player encoding can cancel out.
- Extensions come one at a time after the base is tested: N×N, k in a row, M players, constant-time win check.

## Spec agreed with the interviewer

- Two humans, one keyboard, stdin/stdout; the computer only judges. 3×3 for now ("keep in mind it may change").
- Rows and cols 0–2. Input: two comma-separated ints, whitespace allowed (`1, 2` and `1,2`).
- Not two ints / not comma-separated → error message, same player tries again.
- Square taken or out of bounds → rejected, same player tries again.
- Board printed after every valid move. Player 1 = X, player 2 = O.
- Three in a row, column or diagonal → announce the winner. Board full with no win → draw.
- Input ends (EOF) → game aborted, no winner.

## Design so far (mine)

- Board: `vector<vector<char>>` 3×3, `'-'` for empty (chosen over numbers because it prints directly).
- Players: name + char.
- Board owns turns, applies moves, and after each valid move decides win (for the mover) or draw.
- Parsing separate from the board; the board only receives `(int, int)`.
- Win check: only the row, column and diagonals through the last move, O(N). For generality: walk
  outward from the new mark in each direction and count.
- Draw: a counter of free squares.

## Where we stopped — the interviewer's three questions

1. Diagonal cells: answered (0,0) (0,2) (1,1) (2,0) (2,2) — corners and center, correct.
   **Still open:** the test for each diagonal in terms of `r`, `c`, N.
2. Draw: answered — free-square counter.
3. **Open:** Board stores cells, tracks turns, owns players and judges wins. To unit-test
   "X wins on the anti-diagonal" without typing a game through stdin, what would the test call?
   Does that change the split or the names?

Next: answer 1's test and 3 briefly, then write the 3×3 base, then test aloud (normal, edge, bad input)
and state time and space. Then the interviewer releases extension 1.

To resume: `git pull`, start Claude in `Systems-drills/`, say "resume build 1 from
Interview/builds/01-tictactoe/SESSION.md".
