# Concurrency ladder

Next: rung 4 (read-modify-write by hand): new `04-cas/`, your own atomic add and atomic max
using only `atomic_compare_exchange_weak`, with a retry counter. Cold rebuilds due: rung 0 from
2026-09-30, rung 1 from 10-01, rung 2 from 10-02, rung 3 from 10-02.

Target: interviews for systems, hypervisor and kernel teams (SAP, Amazon). Session: 90 min.
Quiz (2026-09-24):
- Solid: an atomic RMW is indivisible; what spinning costs.
- Partly: release pairs with acquire; condvar waits; the logger (built 2026-09-22, not recalled).
- Gaps: what acquire does *not* do; CAS; the condvar failure; when spinning pays off; interrupts.
- Corrected: relaxed orders nothing; a mutex waiter doesn't poll; the kernel does sleep.

One rung per directory `NN-name/`. A rung is done when it builds, its card in `deck.md` is
in your own words, and the break-it result is recorded. The `rung` skill schedules cold
rebuilds and recall interviews. Older versions of this file, which contain the answers, are in
git history. Don't read them.

Shape: facets, each block ending in a rung that combines them (7 logger, 15 kernel module).

## Foundations

- [x] **0 · A visible race.** Four pthreads increment a shared counter with no lock. *New:* ThreadSanitizer.
  *Try:* predict the total, then run at `-O2`, under TSan, and with `volatile`.
  *Answers:* what is a data race?
  done: 2026-09-23 · cold: —
- [x] **1 · Mutex.** Fix the race with `pthread_mutex` (done). Then build your own mutex: an
  `int` in your memory, `atomic_exchange`, and the futex syscall. Swap it in for `pthread_mutex`.
  *New:* a blocking lock, futex.
  *Try:* time and `strace -f -c` your mutex against pthread's at 1, 4 and 8 threads.
  *Answers:* what does a mutex actually do when it's taken?
  done: 2026-09-24 · cold: —
- [x] **2 · Atomic counter, and what the CPU does.** Quick rung: the C part is solid. Replace
  the mutex with a C11 atomic increment. *New:* `objdump -d`, and cross-compiling for aarch64
  twice: plain, and with `-march=armv8.1-a`.
  *Try:* before disassembling, predict what the increment becomes on x86 and on
  both ARM builds. Time it against rung 1.
  *Answers:* how is a lock done in the CPU? (the SAP question, exactly)
  done: 2026-09-25 · cold: —
- [x] **3 · Hand-off.** One thread fills a struct, then sets a flag. Another thread waits for
  the flag, then reads the struct. No lock. *New:* release and acquire.
  *Try:* make the flag relaxed; predict what TSan says, then run it. Then have the
  reader check the flag once instead of waiting for it, and predict what it can see.
  *Answers:* what does an acquire load promise, and what does it not do?
  done: 2026-09-25 · cold: —
- [ ] **4 · Read-modify-write by hand.** Write your own atomic add and atomic max, using only
  `atomic_compare_exchange_weak`. Count the retries. *New:* compare-and-swap.
  *Try:* predict the retry count with 1, 4 and 8 threads, then measure. Swap weak
  for strong: predict whether anything changes on x86, and think about ARM.
  *Answers:* what is compare-and-swap? Why weak vs strong?
  done: — · cold: —
- [ ] **5 · Spinlock.** Build `lock`/`unlock` from atomics, with the weakest memory orders that
  are still correct. *New:* busy-waiting.
  *Try:* predict throughput against rung 1 at 2 threads and at more threads than
  cores. Watch the lock holder's speed as the number of waiters grows, find out why it
  changes, and fix the waiters.
  *Answers:* how do you build a lock from atomics, and what do the waiters cost?
  done: — · cold: —
- [ ] **6 · Fair spinlock.** Threads must get the lock in the order they arrived. *New:* fairness.
  *Try:* count acquisitions per thread for rung 5 vs this lock. On paper: what
  happens to the waiters when the hypervisor deschedules the lock holder's vCPU? What could the
  guest or the hypervisor do about it?
  *Answers:* when does spinning pay off, and why are spinlocks dangerous inside a guest?
  done: — · cold: —

## Blocking primitives

- [ ] **7 · Bounded queue, mutex + condition variable.** Producers and consumers share a
  fixed-capacity queue. Producers block when it's full, consumers when it's empty.
  *New:* condition variables.
  *Try:* turn the wait's predicate check into an `if` and construct a run that
  breaks it (the quiz's Q4). Swap `signal` and `broadcast` and predict what changes.
  *Answers:* write a producer-consumer queue.
  done: — · cold: —
- [ ] **8 · Semaphore.** Rebuild the same queue on `sem_t`, then build a counting semaphore from
  a mutex and a condvar. *New:* semaphores.
  *Try:* find an operation that is legal on one primitive and not on the other;
  predict what each does if you try it anyway.
  *Answers:* mutex vs semaphore? (the email's word)
  done: — · cold: —
- [ ] **9 · Reader-writer lock and writer starvation.** Build a naive rwlock from a mutex and
  condvars, run a read-heavy load, starve the writer, then fix the starvation. *New:* starvation.
  *Try:* predict the writer's latency with 8 readers, before and after the fix.
  *Answers:* when would you not use an rwlock, and what would you use instead?
  done: — · cold: —
- [ ] **10 · The SAP logger.** Several CPUs append (timestamp, cpu id, payload) records to one
  log, and copying a record is slow. Constraints: the serialised part is as small as possible,
  and a reader never sees a half-written record. Version A takes no lock at all. In version B,
  a CPU that can't get the lock doesn't wait. You built this once (`../CPU_logger`) and couldn't
  recall it two days later: write the plan from scratch, build it, *then* compare.
  *New:* combining rungs 2-8.
  *Try:* predict throughput against a plain-mutex version at 1, 4 and 8 threads.
  Weaken A's publication step until a reader catches a torn record.
  *Answers:* many CPUs append to one log and the copy is slow; how? Rebuild both cold within a week.
  done: — · cold: —

## Lock-free structures

- [ ] **11 · SPSC ring buffer.** One producer thread and one consumer thread share a fixed-size
  ring, with no locks. Frame the ownership first (see `../SPSC_Ring_buffer` for the earlier attempt).
  *New:* false sharing.
  *Try:* predict throughput, then find out whether the placement of head and tail
  in memory matters, and why. Make every atomic relaxed: predict whether the tests fail on x86,
  and whether they would on ARM.
  *Answers:* a lock-free single-producer single-consumer queue?
  done: — · cold: —
- [ ] **12 · Read-mostly value.** One writer updates a small multi-word value (e.g. a time pair)
  often; many readers read it. First build it naively and catch a torn read. Then fix it under
  these constraints: readers take no lock, never block the writer, and never return a torn value.
  *New:* a read path with no lock.
  *Try:* predict the readers' retry rate as the write frequency goes up.
  *Answers:* how does the kernel publish the time without a lock?
  done: — · cold: —
- [ ] **13 · Reference counting.** A shared object is freed by whichever thread drops the last
  reference. Constraints: correct on ARM, and no `seq_cst`. *New:* choosing the weakest
  orderings that are still correct.
  *Try:* make everything relaxed, run under TSan, and argue what each operation
  needs and why.
  *Answers:* what orderings does a refcount need, and why not `seq_cst`?
  done: — · cold: —
- [ ] **14 · Lock-free stack.** Push and pop with a CAS loop on the head. On paper, construct
  an interleaving where a pop's CAS succeeds but corrupts the stack, and reproduce it in code if
  you can. Don't implement safe memory reclamation, but be able to say why it's hard.
  *New:* the ABA problem. *Answers:* what is ABA?
  done: — · cold: —

## Depth

- [ ] **15 · Memory-model litmus tests.** Store buffering with relaxed atomics on x86: predict
  whether `r1 == r2 == 0` can ever happen, then run it a few million times. Then message
  passing on ARM (a Graviton instance for an hour, or a Pi), relaxed and then acquire/release;
  predict each before running. *New:* litmus tests.
  *Answers:* ARM vs x86 memory model: what's the difference? (the differentiator to volunteer)
  done: — · cold: —
- [ ] **16 · Deadlock lab.** Two threads take two locks in opposite orders. Get the TSan and
  helgrind reports, then fix it in two different ways.
  *Try:* predict how long it takes to deadlock with 2 threads vs 8.
  *Answers:* how do you avoid deadlock in a large codebase?
  done: — · cold: —
- [ ] **17 · A lock and an interruption.** The main loop updates a counter under your rung-5
  spinlock; a `SIGALRM` handler (`setitimer`, every millisecond) updates the same counter.
  *New:* asynchronous interruption, the user-space stand-in for an interrupt.
  *Try:* predict what happens over a minute of running, then run it. Fix it without
  removing the handler.
  *Answers:* when must a spinlock holder also disable interrupts? (the quiz's Q7)
  done: — · cold: —
- [ ] **18 · The kernel's rules.** Read `Documentation/locking/spinlocks.rst`, `mutex-design.rst`
  and `Documentation/RCU/whatisRCU.rst`. Write a one-page cheat sheet in your own words: which
  primitive in which context (process, softirq, hardirq), and why. *New:* execution contexts.
  *Answers:* why may a spinlock holder not sleep? (half of block 1 of the Amazon screen)
  done: — · cold: —
- [ ] **19 · Kernel module (one October weekend).** An out-of-tree module: a char device backed
  by a `kfifo`, a producer kthread, a wait queue for readers, a spinlock shared with an hrtimer
  callback (work out which `spin_lock` variant it needs) and a mutex on the ioctl path. Load
  it, race it from userspace, and run it under lockdep. *New:* combining everything, in the kernel.
  *Answers:* which locking primitive in which kernel context, and why?
  done: — · cold: —

## Parking lot
- MPSC queue: many producers and one consumer, where a push never retries. Read the standard
  design only after your attempt. (It's what a real multi-producer log ends up being.)
