# Concurrency deck

One card per rung. Front: the interview question. Back: the sentence, plus
what I saw when I built it. Re-read before the Friday cold re-build.

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

**A:** _(fill in after building)_

**What I saw:**

---

## 2 · Atomic counter

**Q: How is a lock done in the CPU?**

**A:**

**What I saw:**

---

## 3 · Spinlock from CAS

**Q: What is compare-and-swap? Why weak vs strong?**

**A:**

**What I saw:**

---

## 4 · Ticket lock

**Q: Why are spinlocks dangerous inside a guest?**

**A:**

**What I saw:**

---

## 5 · Bounded queue, mutex + condvar

**Q: Write a producer-consumer queue.**

**A:**

**What I saw:**

---

## 6 · Semaphore

**Q: Mutex vs semaphore?**

**A:**

**What I saw:**

---

## 7 · The SAP logger

**Q: Many CPUs append to one log and the copy is slow. How?**

**A:**

**What I saw:**

---

## 8 · SPSC ring buffer

**Q: Lock-free single-producer single-consumer queue?**

**A:**

**What I saw:**

---

## 9 · Seqlock

**Q: How does the kernel publish the time without a lock?**

**A:**

**What I saw:**

---

## 10 · Reference counting

**Q: Why release on decrement and acquire before free, not seq_cst?**

**A:**

**What I saw:**

---

## 11 · Reader-writer lock

**Q: When would you not use an rwlock?**

**A:**

**What I saw:**

---

## 12 · Treiber stack and ABA

**Q: What is ABA?**

**A:**

**What I saw:**

---

## 13 · MPSC queue

**Q: What does a real multi-producer log end up being?**

**A:**

**What I saw:**

---

## 14 · Memory-model litmus tests

**Q: ARM vs x86 memory model, what is the difference?**

**A:**

**What I saw:**

---

## 15 · Deadlock lab

**Q: How do you avoid deadlock in a large codebase?**

**A:**

**What I saw:**

---

## 16 · The kernel's view

**Q: Which locking primitive in which kernel context, and why?**

**A:**

**What I saw:**
