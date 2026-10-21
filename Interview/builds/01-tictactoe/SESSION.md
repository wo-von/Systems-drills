# Build 1 · Tic-tac-toe in C — session state

Amazon loop prep, Job hunt 2026 → Daily plan, build lane. Logical-and-maintainable round, mock format:
Claude is the interviewer, I write the code. Claude writes no code unless I say "show me".
Started in C++ on Thu 15 Oct; switched to C on Fri 16 Oct (the loop's coding rounds are in C).

Build: `gcc -std=c11 -Wall -Wextra -Wpedantic -fsanitize=address,undefined -g -o ttt tictactoe.c`
Run: `./ttt [boardsize [winsize]]` (n defaults to 3, k to n). `./ttt < /dev/null` runs the tests, then
aborts on EOF; no output from ASan/LeakSanitizer means clean.

## Spec

- Two humans, one keyboard, stdin/stdout; the computer only judges. Player 1 = X, player 2 = O.
- Input `row, col` (0-based), whitespace allowed. Bad input, out of bounds or taken → same player again.
- Board printed after every valid move. k in a row (row, col, any diagonal, contiguous, k or more) wins;
  board full → draw; EOF → game aborted, no winner.
- Board N×N from argv, 3 ≤ N ≤ 25 (25 = 200-column terminal / 8 columns per cell). 3 ≤ k ≤ N.

## Design (state / rules / game + I/O)

- `struct Board { n, k, counter, char *cells }`: cells on the heap (option B, struct stays on the stack),
  `counter` = free squares, kept only by `make_move`. `set_board(b, n, k)` validates and allocates,
  `del_board` frees and sets NULL (safe twice).
- Two accessors, `board_cell` (write) and `board_cell_const` (read), `r * n + c` in one place, bounds assert.
- Rules: `check_gamestate(const board *, const move *)` reads the mark from the cell (the caller can't pass
  a mismatched one). `is_win` walks outward from the move in 4 directions, `1 + count(+d) + count(-d) >= k`.
  O(k) time, O(1) extra. Correct because of the invariant: nobody had won before this move.
- Tests in `test_tictactoe()`, built only through `make_move` / `place()`, asserts never wrap calls.

## Progress

- [x] Base 3×3 (commit 06c4273)
- [x] Extension 1: N×N from argv (commit 3944933)
- [x] Extension 2: k in a row. **Fix written by Claude** (walk double-counted the move, `<= n` overflowed
      and wrapped rows, `== 0` missed runs longer than k, tests never set k, int narrowing in parsing)
- [ ] Extension 3: M players
- [ ] Extension 4: constant-time win check

## Lessons from this build (my mocks-log categories)

- Bounds: `<=` vs `<` again (anti-diagonal `N - c`, walk `x <= n`); `'\0'` compared to a pointer is NULL.
- Walk: tests with wrong fixtures passed (impossible boards, wrong expected value), so walk the fixture too.
- C cold: `strtol` takes `char **`; `free` doesn't NULL the pointer; `const` on a struct is shallow;
  `n * n` is computed in the operands' type, not the destination's; macro bodies need parentheses.

## Next session

1. Cold redo, 10 min, blank file: `count_dir` + `is_win`, then diff against `tictactoe.c`.
2. Extension 3 from the interviewer: "M players". Clarifying questions first. Expect to revisit where `k`
   and the players live (a `struct Game` owning board + rules + players).

To resume: `git pull`, start Claude in `Systems-drills/`, say "resume build 1 from
Interview/builds/01-tictactoe/SESSION.md".
