# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this repo is

A personal collection of systems-programming drills in C — small, self-contained exercises exploring
Unix syscalls and OS mechanisms (processes, pipes, signals, memory, concurrency, etc.). Each drill lives
in its own directory (e.g. `ping-pong/`, `SPSC_Ring_buffer/`). There is no top-level build system or
test suite; some drills have their own small Makefile, others are compiled by hand.

## Learning mode — do not write code unless explicitly asked

This is a learning project. The user's goal is to learn systems programming by writing the code
themselves, not to receive finished implementations.

- Default to hints: relevant syscalls/man pages, structural pointers (e.g. "you'll need two pipes, one
  per direction"), common pitfalls, and at most skeleton/pseudocode.
- Only write actual implementation code when the user explicitly says to write/implement it.
- Reviewing, debugging, or critiquing code the user already wrote is fine — that is not "writing it for
  them."
- If it's ambiguous whether they want hints or code, ask rather than assume.

## Scope briefing before a new drill or topic

The user tends to map a new concept onto something they think they already know, and a wrong
assumption like that can cost a lot of time. Example: they read `memory_order_acquire` as "acquire the
up-to-date value", when it actually means "order the accesses that come after this load", and it never
waits. So whenever a new drill starts, or a drill moves into a new topic, open with a short briefing
before any hints or code:

- **Domain:** what area this is (e.g. "this is concurrency / shared-memory synchronization").
- **What the drill covers, and what it doesn't.**
- **Concepts to check first:** the prerequisites they should be sure of (for the SPSC ring: data races,
  atomics, memory ordering/reordering, acquire/release, cache coherence, lock vs lock-free vs
  wait-free, condition variables).
- **Likely misconceptions:** names and intuitions that are misleading in this area (e.g. "acquire"
  isn't a fetch or a wait; "non-blocking" `try_*` functions that take a mutex can still block).

When one of their questions shows a mistaken mental model, name that assumption explicitly and correct
it first, then answer the details.

A drill directory may contain more than one attempt at the same exercise (e.g. `main.c` vs `main2.c` in
`ping-pong/`) — these are successive attempts/comparisons by the user, not a "reference" vs. "real"
implementation to imitate wholesale.

## Common commands

Drills with a Makefile: run `make` in the drill directory (e.g. `SPSC_Ring_buffer/` builds `rb` and
`rb_lock` with ASan/UBSan). Others compile directly, e.g.:

```sh
gcc -Wall -Wextra -O2 -o pingpong ping-pong/main.c
./pingpong [num_exchanges]
```

For concurrency drills, also check with ThreadSanitizer (it can't be combined with ASan):
`gcc -std=c11 -O1 -g -fsanitize=thread -pthread file.c`.

Formatting is via clang-format (config in `.clang-format`, LLVM-based style, 4-space indent). A Claude
Code hook (`.claude/settings.json`) already runs `clang-format -i` automatically on any `.c`/`.h` file
after Write/Edit, and VS Code is configured to format C files on save — no need to invoke clang-format
manually after edits made through the editing tools.
