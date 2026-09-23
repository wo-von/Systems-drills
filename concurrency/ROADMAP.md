# Concurrency ladder

One rung per directory: `NN-name/name.c`. Tick a rung when its code builds, the
sentence is on a card in `deck.md`, and the break-it experiment is recorded.

## Foundations

- [x] **0 · A visible race.** Four pthreads increment a shared counter with no lock. Watch the total come out wrong; run under TSan and read the report. *Sentence:* a race is a bug, contention is a cost. *Answers:* what is a data race?
- [ ] **1 · Mutex.** Fix it with `pthread_mutex`. Time it with 1, 4, 8 threads and see contended multi-thread run slower than single-thread. *Sentence:* the loser sleeps in a futex, it doesn't spin. *Answers:* what does a mutex actually do when it's taken?
- [ ] **2 · Atomic counter, and what the CPU does.** `atomic_fetch_add`. Then `objdump -d` and find `lock xadd`. Then cross-compile for aarch64 twice, plain and with `-march=armv8.1-a`, and see the `ldxr`/`stxr` loop become a single `ldadd`. *Sentence:* the core takes exclusive ownership of the cache line through the coherence protocol; nothing locks the bus. *Answers:* how is a lock done in the CPU? The SAP question, exactly.
- [ ] **3 · Spinlock from CAS.** `atomic_compare_exchange_weak` in a loop, acquire on lock, release on unlock, test-and-test-and-set so waiters spin on a read, `pause` in the loop. *Sentence:* CAS writes the new value only if memory still holds the expected one, and tells you which happened. *Answers:* what is compare-and-swap? Why weak vs strong?
- [ ] **4 · Ticket lock.** `fetch_add` on next-ticket, spin until now-serving matches. Fair by construction. Then say what happens to a spinning vCPU when the lock holder's vCPU has been descheduled (lock-holder preemption) and what paravirtual spinlocks do about it. *Answers:* why are spinlocks dangerous inside a guest?

## Blocking primitives

- [ ] **5 · Bounded queue, mutex + condition variable.** Producer/consumer. Demonstrate the lost wakeup by using `if` instead of `while`; demonstrate a spurious wakeup surviving the `while`. `signal` vs `broadcast`. *Sentence:* the predicate is always re-checked in a loop because the wakeup is a hint, not a guarantee. *Answers:* write a producer-consumer queue.
- [ ] **6 · Semaphore.** Same queue on `sem_t`. Then implement a counting semaphore from a mutex and a condvar. *Sentence:* a mutex has an owner and must be released by it; a semaphore is a count and anyone may post. *Answers:* mutex vs semaphore. The email's word.
- [ ] **7 · The SAP logger.** Multiple CPUs append (timestamp, cpu id, payload) to one log; the copy is slow. Version A: `fetch_add` reserves a slot, copy with no lock held, release-store a sequence number to publish; reader acquire-loads and checks `seq == i + 1`. Version B: `trylock`, and a CPU that loses keeps preparing its next records locally and flushes them in a batch when it wins. *Sentence:* the only serialised step is one atomic instruction; everything slow happens in private. *Answers:* the question you had yesterday. Re-build both cold in one week.

## Lock-free structures

- [ ] **8 · SPSC ring buffer.** Power-of-two capacity, head and tail atomics, acquire/release, `_Alignas(64)` so the two indices never share a line. Then make everything relaxed and explain why it still passes on x86 and would not on ARM. *Sentence:* release on the producer's tail store pairs with acquire on the consumer's tail load; that pairing is the whole correctness argument. *Answers:* the most likely lock-free ask on a hypervisor team.
- [ ] **9 · Seqlock.** Writer bumps an odd/even sequence around the write; readers retry if the sequence changed or was odd. *Sentence:* readers never block writers, at the cost of retries; right for small, read-mostly data like a clock. *Answers:* how does the kernel publish the time without a lock?
- [ ] **10 · Reference counting.** `fetch_sub` with release; `atomic_thread_fence(acquire)` in the thread that sees zero, before the free. *Sentence:* release on every decrement so that the last one, after its acquire, sees every other thread's writes before it frees. *Answers:* why those two orderings and not seq_cst?
- [ ] **11 · Reader-writer lock and writer starvation.** Build the naive one, starve the writer, fix it with a writer-preferring variant. Then say when RCU is the better answer. *Answers:* when would you not use an rwlock?
- [ ] **12 · Treiber stack and ABA.** Lock-free push/pop with a CAS loop on the head. Construct the ABA interleaving on paper and, if you can, in code. Do not implement safe reclamation; be able to say why it is hard and name the three answers (hazard pointers, epochs, RCU). *Answers:* what is ABA?

## Depth

- [ ] **13 · MPSC queue.** Vyukov's intrusive multi-producer single-consumer queue: one `exchange` on push, no CAS loop. Optional; it is what a real log ends up being.
- [ ] **14 · Memory-model litmus tests.** Store buffering (Dekker) with relaxed atomics on x86: run it a few million times and watch `r1 == r2 == 0` appear, which TSO permits. Then message passing on an ARM machine (a Graviton instance for an hour, or a Pi) and watch it break without acquire/release where x86 never would. *Sentence:* x86 only reorders stores after later loads; ARM reorders almost anything not ordered by a fence or an acquire/release pair. *Answers:* the ARM vs x86 differentiator you planned to volunteer, now backed by something you saw.
- [ ] **15 · Deadlock lab.** Two locks taken in opposite orders by two threads. Get the TSan/helgrind report, fix by ordering, then fix again with `trylock` and back-off. *Sentence:* a global lock order is the only cure; lockdep enforces it in the kernel. *Answers:* how do you avoid deadlock in a large codebase?
- [ ] **16 · The kernel's view: reading, then a weekend module.** `Documentation/locking/spinlocks.rst`, `mutex-design.rst`, `Documentation/RCU/whatisRCU.rst`. Write a one-page cheat sheet: which primitive in which context (process, softirq, hardirq), why nothing sleeps under a spinlock, when `spin_lock_irqsave` is required, what a grace period is. *Answers:* half of block 1 of the Amazon screen. Then, one October weekend: a small out-of-tree module, a char device backed by a `kfifo`, a producer kthread, a wait queue for readers, a spinlock taken with `irqsave` from an hrtimer callback, a mutex on the ioctl path. Load it, race it from userspace, read it under lockdep. That turns the cheat sheet into hands, and it is the kernel-team question you can then answer from something you built this month.

## After rung 16

Every Friday, pick one rung from a die roll and re-build it in 30 minutes with no notes. If it takes longer than 30, it goes back into the weekly rotation.
