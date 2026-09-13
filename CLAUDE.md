# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this repo is

A personal collection of systems-programming drills in C — small, self-contained exercises exploring
Unix syscalls and OS mechanisms (processes, pipes, signals, memory, etc.). Each drill lives in its own
directory (currently just `ping-pong/`). There is no build system (no Makefile/CMake) and no test suite;
drills are compiled and run by hand.

## Learning mode — do not write code unless explicitly asked

This is a learning project. The user's goal is to learn systems programming by writing the code
themselves, not to receive finished implementations.

- Default to hints: relevant syscalls/man pages, structural pointers (e.g. "you'll need two pipes, one
  per direction"), common pitfalls, and at most skeleton/pseudocode.
- Only write actual implementation code when the user explicitly says to write/implement it.
- Reviewing, debugging, or critiquing code the user already wrote is fine — that is not "writing it for
  them."
- If it's ambiguous whether they want hints or code, ask rather than assume.

A drill directory may contain more than one attempt at the same exercise (e.g. `main.c` vs `main2.c` in
`ping-pong/`) — these are successive attempts/comparisons by the user, not a "reference" vs. "real"
implementation to imitate wholesale.

## Common commands

There's no build script; compile drills directly with gcc/clang, e.g.:

```sh
gcc -Wall -Wextra -O2 -o pingpong ping-pong/main.c
./pingpong [num_exchanges]
```

Formatting is via clang-format (config in `.clang-format`, LLVM-based style, 4-space indent). A Claude
Code hook (`.claude/settings.json`) already runs `clang-format -i` automatically on any `.c`/`.h` file
after Write/Edit, and VS Code is configured to format C files on save — no need to invoke clang-format
manually after edits made through the editing tools.
