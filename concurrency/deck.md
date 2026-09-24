# Concurrency deck

One card per rung. Front: the interview question. Back: my answer in my own
words, written before Claude gives its sentence, plus what I saw when I built it.

---

## 0 · A visible race

**Q: What is a data race?**

**A:** Two threads access the same location, at least one writes, at least one
access is non-atomic, and nothing orders them. In C11 that is undefined
behaviour, not "a wrong number". A race is a bug; contention is a cost.

**What I saw:**
- Four threads, ten million `(*counter)++` each, `-O2`: total was exactly
  40 000 000 every run. `objdump` showed no loop, just `add %rax,(%rdx)`.
  The compiler folded the loop into one add because it may assume no other
  thread touches a non-atomic object.
- Same source under TSan: two reports on the increment line, and a total of
  222 009 instead of 400 000. TSan does not look at results; it tracks
  happens-before per address.
- With `volatile`, the loop came back (`mov`, `add`, `mov`) and the total
  dropped to ~10-15 million of 40. `volatile` did not fix anything, it only
  stopped the compiler from hiding the race. `volatile` is for hardware;
  atomics are for threads.
- `pthread_join` does not run the thread. The thread runs from `create`.
  Join only waits, and gives a happens-before edge plus a lifetime guarantee.
- Start breaking at around 100000, before that, the race cannot be seen, since the operation is fast (nano sec)
  and the pthread_creat needs microsecs.

---

## 1 · Mutex

**Q: What does a mutex actually do when it's taken?**

**A:** The lock is an int in my own memory. Taking it is one atomic exchange in user
space: if the old value was "free", I have it, and the kernel is never involved. If it
was taken, I mark the word "maybe waiters" and put myself to sleep with futex wait on
that address. The unlocker releases with one exchange and calls futex wake only if the
old value said someone may be waiting. So the kernel only does sleep and wake, never the
locking itself.

**What I saw:**
- My prediction was about 1 µs per lock (a syscall every time). pthread did 1 thread ×
  100 000 in 0.003 s with `sys` 0.000, so the uncontended path never enters the kernel.
- `strace -f -c`, 8 threads × 100 000 with pthread: only about 6 800 futex calls for
  800 000 lock/unlock pairs. futex isn't a lock; it's "sleep on / wake up an address".
- v0 (0/1 word, unlock always wakes everyone): 800 055 futex calls, and even 1 thread
  was about 18× slower than pthread (0.054 s against 0.003 s).
- Bugs on the way:
  - WAIT_PRIVATE with a plain WAKE hung: private and shared futexes are keyed
    differently, so the wake found no sleepers.
  - Load-then-store in unlock loses a wakeup.
  - A second store of 0 after the wake released someone else's lock, and the assert failed.
- v1 (3 states: 0 free, 1 locked, 2 maybe waiters): 1 futex call at 1 thread (join's),
  4 284 at 8 threads.
- 1 000 000 per thread, real time: pthread 0.018 / 0.253 / 0.706 s, mine 0.011 / 0.153 /
  0.454 s at 1 / 4 / 8 threads. Single runs. Mine skips glibc's mutex-type checks and
  lets newcomers barge.

**Later:**
- Why does `user` time grow about 70× from 1 thread to 8 when waiting threads are
  asleep? (rungs 2 and 5)
- Barging makes it unfair. (rung 6)

---

## 2 · Atomic counter

**Q: How is a lock done in the CPU?**

**A:** _(your words first)_

**What I saw:**

---

## 3 · Hand-off

**Q: What does an acquire load promise, and what does it not do?**

**A:** _(your words first)_

**What I saw:**

---

## 4 · Read-modify-write by hand

**Q: What is compare-and-swap? Why weak vs strong?**

**A:** _(your words first)_

**What I saw:**

---

## 5 · Spinlock

**Q: How do you build a lock from atomics, and what do the waiters cost?**

**A:** _(your words first)_

**What I saw:**

---

## 6 · Fair spinlock

**Q: When does spinning pay off, and why are spinlocks dangerous inside a guest?**

**A:** _(your words first)_

**What I saw:**

---

## 7 · Bounded queue, mutex + condvar

**Q: Write a producer-consumer queue.**

**A:** _(your words first)_

**What I saw:**

---

## 8 · Semaphore

**Q: Mutex vs semaphore?**

**A:** _(your words first)_

**What I saw:**

---

## 9 · Reader-writer lock

**Q: When would you not use an rwlock, and what would you use instead?**

**A:** _(your words first)_

**What I saw:**

---

## 10 · The SAP logger

**Q: Many CPUs append to one log and the copy is slow. How?**

**A:** _(your words first)_

**What I saw:**

---

## 11 · SPSC ring buffer

**Q: Lock-free single-producer single-consumer queue?**

**A:** _(your words first)_

**What I saw:**

---

## 12 · Read-mostly value

**Q: How does the kernel publish the time without a lock?**

**A:** _(your words first)_

**What I saw:**

---

## 13 · Reference counting

**Q: What orderings does a refcount need, and why not seq_cst?**

**A:** _(your words first)_

**What I saw:**

---

## 14 · Lock-free stack

**Q: What is ABA?**

**A:** _(your words first)_

**What I saw:**

---

## 15 · Memory-model litmus tests

**Q: ARM vs x86 memory model, what is the difference?**

**A:** _(your words first)_

**What I saw:**

---

## 16 · Deadlock lab

**Q: How do you avoid deadlock in a large codebase?**

**A:** _(your words first)_

**What I saw:**

---

## 17 · A lock and an interruption

**Q: When must a spinlock holder also disable interrupts?**

**A:** _(your words first)_

**What I saw:**

---

## 18 · The kernel's rules

**Q: Why may a spinlock holder not sleep?**

**A:** _(your words first)_

**What I saw:**

---

## 19 · Kernel module

**Q: Which locking primitive in which kernel context, and why?**

**A:** _(your words first)_

**What I saw:**
