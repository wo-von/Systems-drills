<!-- Per-repo file: save as CLAUDE.md in the repo root (or CLAUDE.local.md + .gitignore if you
     don't want it public). Keep it short; the workflow lives in ~/.claude/CLAUDE.md. -->

# System-drills

<One line: what this is and why it exists.>

- Default mode: learn
- Language / standard: <C11 | C++20>
- Build: `<cmd>`
- Test: `<cmd>`
- Sanitizers: `<cmd>`
- Run: `<cmd>`

## Hand-written zone (learn rules apply here whatever the mode)
- <paths I write myself; review only>

## AI-OK zone
- <Makefile, tests/, scripts/, README ...>

## Repo truths
<!-- One line each. Add an entry every time you got something wrong in this repo. -->
-

## Current focus
- Start Free-list / Pool-allocator


<!-- ===================== EXAMPLE: KVM VMM ===================== -->
<!-- TODO(Sina): fix repo name, paths and commands; I have not seen the tree. -->

# kvm-vmm

Minimal KVM-based VMM in C, written to learn the virtualization stack end to end.

- Default mode: learn
- Language / standard: C11, Linux x86-64 only
- Build: `make`            <!-- TODO verify -->
- Test: `make test`        <!-- TODO verify -->
- Sanitizers: `make asan`  <!-- TODO add target if missing -->

## Hand-written zone
- Everything that touches /dev/kvm: VM and vCPU setup, guest memory, register state,
  mode transitions, page tables, the VM-exit loop.

## AI-OK zone
- Makefile, guest test payload build scripts, test harness, README wording.

## Repo truths
- Stage 1 was written by hand. Keep it that way; suggest, don't rewrite.
- KVM struct layouts and ioctl semantics come from <linux/kvm.h> and Documentation/virt/kvm/api.rst,
  never from memory. Cite the section when you claim something about the API.

## Current focus
- Stage 2: long mode entry, paging, EPT.
