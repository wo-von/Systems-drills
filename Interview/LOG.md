## C gotchas found in problems

- Returning a local array (lc 1). `int res[2]; return res;` gives the caller a
  dangling pointer: the array's lifetime ends at return (C11 6.2.4p2). Return memory
  from `malloc`, and say in a comment that the caller frees it.
- `-1` as a `size_t` is `SIZE_MAX` (lc 1, lc 20). Returning `-1` for "not found" from a
  `size_t` function doesn't work, and `top - 1` with `top == 0` wraps the same way.
- `malloc` memory is uninitialized (lc 1). Use `calloc` for flag arrays like `used[]`;
  reading an uninitialized `bool` is UB (UBSan: "not a valid value for type 'bool'").
- Freeing twice is UB (lc 1, C11 7.22.3.3). If cleanup can run twice (init failed, then
  `free_map`), set pointers to NULL after `free`; `free(NULL)` does nothing.
- Arrays can't be assigned, only initialized (lc 20). `stk.s = {0};` is an error: an
  array isn't a modifiable lvalue (C11 6.3.2.1p1), and a `{...}` list is only valid as
  an initializer in a declaration (6.7.9). Fix: `struct Stack stk = {0};` at the
  declaration, or `memset`/`memcpy` later. A struct holding an array *can* be assigned.

## Problems

date | problem | minutes | compile errors | logic bugs | found by me | categories
2026-10-22 | lc 1 two-sum | bell; rewrite ~150 | 6 | 13 | 0 | hash map (open addressing), arrays
2026-10-22 | lc 20 valid-parentheses | 44 | 2 | 3 | 0 | stack, strings
2026-10-23 | lc 200 number-of-islands | bell; fixed by 113 | 14 | 5 | 0 | BFS, grid graph, queue
2026-10-23 | lc 994 rotting-oranges | bell; fixed by 77 | 4 | 4 | 1 | multi-source BFS, grid graph, two queues
2026-10-23 | lc 1 two-sum (cold) | bell; fixed by 64 | 2 | 6 | 0 | hash map (open addressing), arrays
2026-10-23 | lc 20 valid-parentheses (cold) | 23 | 0 | 1 | 0 | stack, strings
2026-10-24 | lc 200 number-of-islands (cold) | bell; fixed by 65 | 0 | 5 | 0 | BFS, grid graph, queue
2026-10-24 | lc 994 rotting-oranges (cold) | bell; fixed by 60 | 0 | 3 | 0 | multi-source BFS, grid graph, two queues
2026-10-26 | lc 206 reverse-linked-list | bell; fixed by 56 | 5 | 2 | 1 | linked list, pointer rewiring
2026-10-27 | lc 21 merge-two-sorted-lists | 40 | 2 | 2 | 0 | linked list, dummy node, two pointers
2026-10-27 | lc 206 reverse-linked-list (cold) | 11 | 0 | 0 | 0 | linked list, pointer rewiring
2026-10-28 | lc 56 merge-intervals (C++) | bell; fixed by 55 | 2 | 4 | 1 | sorting, interval sweep, greedy
