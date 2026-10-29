# OS 4 — processes, threads, syscalls, scheduling, and the tools (review sheet, Thu 29 Oct)

## 1. Shell runs `ls -l`: the model answer (3 minutes)

```
shell (pid 100)                                   kernel
 read line (readline), parse, not a builtin, search PATH
 fork() ── glibc → clone(SIGCHLD) ─────────────► copy task_struct, new PID 101,
   │                                               copy page tables, writable pages → read-only (COW)
   ├─ returns 101 in the parent                    both runnable
   │   wait4(101) ──────────────────────────────► parent sleeps: state S
   └─ returns 0 in the child (pid 101)
       [redirections here: dup2(fd, 1)]
       execve("/usr/bin/ls", argv, envp) ───────► drop the old mm, read the ELF header,
                                                   map segments + stack (argv, envp, auxv),
                                                   start the dynamic loader → ld maps libc → main()
       ls: ioctl(1, TCGETS) is stdout a tty?
           statx / openat + getdents64 for directories
           /etc/passwd, /etc/group for names; /etc/localtime for dates
           write(1, ...) → tty/pty driver → terminal
       exit_group(0) ───────────────────────────► free mm, files; state Z (zombie);
                                                   SIGCHLD to the parent; wake the parent
 wait4 returns 101, status 0 ◄──────────────────── zombie reaped (task_struct freed)
 print prompt
```

Seen on this machine with `strace -f bash -c 'ls -l /etc/hostname; true'`: `clone(...SIGCHLD) = 31829`,
`wait4(-1, <unfinished>`, `31829 execve("/usr/bin/ls"...)`, `exit_group(0)`, `wait4 resumed ... = 31829`.

**fork:** returns the child's PID to the parent, 0 to the child, -1 on failure. The child gets only the
calling thread, no pending signals, no file locks. Memory is shared copy-on-write.

**execve keeps:** PID, PPID, fds without `O_CLOEXEC`, cwd, uid/gid, rlimits, signal mask, ignored signals.
**execve loses:** the whole address space, handled signal handlers (reset to default), other threads,
mappings, `atexit` handlers.

**States:** R running/runnable · S interruptible sleep · D uninterruptible (I/O, counts in load) ·
T stopped · Z zombie (exited, not yet reaped; can't be killed; kill or fix the parent).

**Process vs thread in Linux:** both are tasks (`task_struct`). `clone()` flags decide what is shared
(`CLONE_VM`, `CLONE_FILES`, `CLONE_SIGHAND`, `CLONE_THREAD`). Userspace PID = kernel TGID; each thread
has its own TID.

## 2. Context switch

- Entering the kernel (syscall, interrupt) saves the **user** registers on the task's own kernel stack.
- `schedule()` picks the next task; `context_switch()`:
  - `switch_mm`: write CR3 for the new page tables (skipped between threads of one process; with
    **PCID** the TLB isn't flushed).
  - `__switch_to`: callee-saved registers, **RSP = the next task's kernel stack** (a pointer swap, no copy),
    FS/GS base (TLS), **FPU/SSE/AVX state via XSAVE/XRSTOR** (up to ~2.5 KiB).
- Triggers: **voluntary** (the task blocks, e.g. in `wait4`) or **involuntary** (preempted: a tick or a wakeup
  finds a task that should run first). Per-task counts: `/proc/<pid>/status` `*_ctxt_switches`.
- Cost: ~1–2 µs direct; the bigger cost is cold caches and TLB afterwards. Measured here: ~1,200
  switches/s on an idle 12-CPU machine (`/proc/stat` `ctxt`).
- Hypervisor link: KVM saves/restores guest FPU state around VM entry/exit the same way (XSAVE).

## 3. Scheduling

**Weights:** nice 0 = 1024; each nice step ≈ ×1.25 (≈10% CPU vs a competitor). nice 5 = 335, nice 10 = 110,
nice 19 = 15, nice −10 = 9548. Share = weight / sum of weights. Measured here, three loops on one CPU at
nice 0/5/10: **69.0 / 22.6 / 7.3 %**, predicted 69.7 / 22.8 / 7.5. Alone on a CPU, any nice gets 100%.

**CFS (2.6.23 – 6.5):** `vruntime += runtime × 1024 / weight`; runnable tasks in a red-black tree by
vruntime; run the leftmost. Fair shares, but latency only through heuristics.

**EEVDF (6.6 onwards):** same weights. **Lag** = owed − received; **eligible** if lag ≥ 0; **virtual
deadline** ≈ eligible time + slice / weight; run the eligible task with the earliest deadline. A shorter
requested slice (6.12: `sched_setattr`) means earlier deadlines: runs **sooner**, not **more**. Lag
survives short sleeps, so sleeping doesn't game it.

**Classes, strict order:** `SCHED_DEADLINE` > `SCHED_FIFO`/`SCHED_RR` (1–99) > normal (EEVDF) > `SCHED_IDLE`.
Per-CPU run queues, load balancing, affinity (`taskset`). 6.12 also has **sched_ext** (BPF schedulers).

**Hypervisor link:** a vCPU is a host thread. Descheduled vCPU = **steal time** in the guest (`st`);
descheduled while holding a lock = **lock-holder preemption** (OS 5).

## 4. "The service is slow": first 60 seconds

| Step | Command | Look for |
|---|---|---|
| 1 | `uptime` | Load vs number of CPUs; rising or falling (1/5/15 min) |
| 2 | `dmesg -T \| tail` | OOM kills, I/O errors, hung-task warnings |
| 3 | `vmstat 1` | `r` > CPUs (CPU saturated), `b` (I/O blocked), `si/so` ≠ 0 (swapping), `wa`, `st` (steal) |
| 4 | `ps -o pid,stat,ni,pcpu,rss,wchan:20,comm -p <pid>` | State (R/S/D), where it sleeps (WCHAN) |
| 5 | `/proc/<pid>/status` | Threads, VmRSS, voluntary vs involuntary switches |
| 6a | `perf top -p <pid>` / `perf record -g` | High CPU: which functions |
| 6b | `strace -c -f -p <pid>` (seconds only) / `perf trace` | Syscall counts and errors; **strace slows the target 10–100×** |
| 6c | `iostat -xz 1`, `/proc/<pid>/stack` | D state: which disk, where in the kernel |
| 6d | `pmap -x`, `/proc/<pid>/smaps_rollup` | Which mapping grows |
| 6e | `lsof -p`, `ls /proc/<pid>/fd \| wc -l` | fd leaks vs `ulimit -n` |
| 6f | `top -H -p`, `ps -L` | One hot thread (a vCPU of a VMM) |
| VMM | `perf kvm stat` | VM exits by reason |

## What you said and what to say instead

| You said | Say instead |
|---|---|
| "exec is run by COW" | COW is fork's; exec throws the address space away and builds a new one |
| "The child gets an exact copy of everything, only the PID differs" | Only the calling thread; no pending signals or locks; memory shared COW |
| (exec question answered for fork) | exec keeps PID, fds, cwd, ids; loses memory, handlers, threads |
| States not named | Shell S in `wait4`; ls R; then Z until reaped |
| "Saves registers and variables on the stack, and the stack itself" | User registers saved at kernel entry; the switch swaps RSP between per-task kernel stacks; nothing copied |
| "Sets page tables, file descriptors" | CR3 (with PCID), FS/GS base, XSAVE state; fds are just the task's pointer |
| (scheduler) "I don't know" | Weights; CFS vruntime + RB tree; EEVDF lag + eligible + earliest virtual deadline |
| "19/1043" | Weight, not nice value: 15 / 1039 ≈ 1.4% |
| "top, then strace" | System first (uptime, dmesg, vmstat), then the process (ps, /proc), then one tool; strace is heavy |

## Check you can explain these cold

- [ ] `ls -l` from the shell: fork, exec, wait, exit, with states
- [ ] What exec keeps and loses
- [ ] Process vs thread as `clone()` flags
- [ ] The context switch steps, and why the cost is mostly caches/TLB
- [ ] nice → weight → share, with the 69/23/7 example
- [ ] CFS vs EEVDF in four sentences
- [ ] Steal time and lock-holder preemption
- [ ] The first-60-seconds order and what each column means
