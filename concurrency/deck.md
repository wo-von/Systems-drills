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

**A:** _(your words first)_

**What I saw:**
strace -f -c ./01-mutex/mutex 8 100000 2>&1 | grep -E "futex|calls"
== 1
counter is 100000

real	0m0.054s
user	0m0.027s
sys	0m0.027s
== 4
counter is 400000

real	0m0.065s
user	0m0.161s
sys	0m0.085s
== 8
counter is 800000

real	0m0.161s
user	0m0.469s
sys	0m0.565s
% time     seconds  usecs/call     calls    errors syscall
100.00   13.987126          17    800055        51 futex

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
