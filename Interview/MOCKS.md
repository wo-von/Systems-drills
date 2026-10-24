# Coding mocks

Claude plays the interviewer; I write the code. Run with the `mock` skill ("mock", "cold redo").
One session: cold redo of the task that's due → a new task → my test walk → compile → review → my fix → one log line.

Build: `gcc -std=c11 -Wall -Wextra -Wpedantic -fsanitize=address,undefined -g -o <name> <name>.c`

## What I'm training (the log's categories)

1. **Model:** data model before code. Metadata, not the memory itself; what is a mapping and what is bookkeeping.
2. **Bounds:** `>` vs `>=`, half-open ranges, first/last index, unsigned under/overflow.
3. **Walk:** hand-walk 2–3 cases (edges included) *before* saying done.
4. **C cold:** includes first; `.` vs `->`; keywords; enum/struct syntax; array parameters.
5. **Finish:** I make the last fix myself. Claude writes code only on "show me".

## Task bank

Status: `todo`, `open` (started, not finished), `done`, `cold ✓` (redo passed), `cold ✗` (redo again in 7 days).

| # | Task | File | Status | Cold redo due |
|---|---|---|---|---|
| 1 | Guest physical → host virtual lookup (region table, overlap check) | `mock1.c` | done (fix written by Claude) | 2026-09-29 |
| 2 | Physical frame allocator (bitmap, ctz, next-fit, double free) | `mock2.c` | done | 2026-09-30 |
| 2b | Frame allocator, part 2: allocate n contiguous frames | — | todo | |
| 3 | Pool allocator over a fixed byte array (alignment, embedded free list) | — | todo | |
| 4 | Binary header parser (length field, bounds before every read, `offset + len` overflow) | `mock4.c` | done | 2026-10-27 |
| 5 | Page-table walker, 4 levels, then nested (gva → gpa → hpa) | — | todo | |
| 6 | LRU cache (hash map + intrusive doubly linked list, O(1) both ways) | — | todo | |
| 7 | Dirty-page bitmap: set, clear, iterate set bits fast | — | todo | |
| 8 | `/proc`-style text parser into structs, malformed lines handled | — | todo | |
| 9 | Linked-list surgery (reverse sublist, cycle, merge, splice) | — | todo | |
| 10 | Slab-style fixed-size object cache on top of #3 | — | todo | |

Tasks 3, 4, 5 and 6 repay a second pass the most. Each finished task gets a cold redo 2–3 days later.
Tasks 2 and 10 are in the ladder's orbit but stay single-threaded here.

## Log

One line per task or redo. "Found by me" = logic bugs I caught in my own walk before compiling.

| Date | Task | Min | Compile errors | Logic bugs | Found by me | Categories |
|---|---|---|---|---|---|---|
| 2026-09-27 | 1 · gpa → hva | ~50 | 13 | 5 | 0 | model, bounds, walk, C cold, finish |
| 2026-09-27 | 2 · frame allocator | | | | | model (bitmap ≠ page tables) |
| 2026-10-24 | 4 · header parser | 113 | 3 | 7 | 0 | model, bounds, walk, C cold, finish |
