# Mock loop debrief (rounds Wed 28 – Thu 29 Oct)

Raw notes per round: [mockloop_2026-10-28.md](mockloop_2026-10-28.md). Design sheets: [DESIGN_INDEX.md](DESIGN_INDEX.md).

## Results

| Round | LP story | Technical | Result |
|---|---|---|---|
| 1 Problem solving | Hire & Develop: Meteo-42 master student | Merge Intervals (C++) | Right approach fast, good clarifying questions; bell, fixed by 55; space complexity wrong twice |
| 2 OS | Dive Deep: C → Rust rewrite | malloc(32 KiB) trace + "inside a guest" | Right shape; **virtual vs physical bookkeeping merged into one table**; exit and EPT-violation path missing |
| 3 Design | Customer Obsession: South Pole datalogger | Live migration | Reinvented pre-copy; units wrong (×8); dirty logging needed a hint; "send the EPT"; no stop rule |
| 4 Logical & maintainable | Ownership: lidar product line | Parking lot in C | 20 of 40 min on API talk, nothing compiled; rewritten from reference |

## Five patterns, in order of how much they cost

1. **Code that runs comes late.** Merge Intervals: file empty at minute 20 (unsaved). Parking lot: nothing compiled at the bell.
   Fix: signatures + stub bodies + `main` within 8 minutes; fill in one function at a time, one test each.
2. **Physical vs virtual memory.** "One global table for all RAM, both PA and VA; brk assigns physical pages." That's
   the base for OS 2, the malloc trace, live migration and guest memory. Fix: VMAs (per process, virtual ranges) ·
   page tables (per process, VA → PA, filled lazily) · `struct page` + buddy (global, physical frames). Reread
   `os1_os2_review.md` section OS 2 and `design5_guest_memory.md` section 2.
3. **Stories change under follow-up.** Rust: root cause went from "float rounding" to "out-of-bounds read". Lidar:
   today's version differs from the one in the stories tab (see below). Interviewers compare notes; a story that
   shifts reads as unreliable. Fix: one written version per story, numbers included, and tell that one.
4. **No numbers, no cost.** No LP answer had a number in its first telling. Customer Obsession never said what it
   cost. Fix: every story needs a number in the Result and, where asked, the cost.
5. **Units and precise words.** 22.5 s instead of 176 s (bytes vs bits); `std::sort` "O(1)" then "O(n)"
   (it's O(log n) stack); "shadow pages" for EPT; "transfer the logs". Fix: say units aloud; when unsure of a
   term, describe the mechanism instead.

What went well: clarifying questions in rounds 1 and 3; finding the duplicate-push bug yourself by tracing;
self-correcting units; the right shape on the malloc trace and on pre-copy; strong concrete details once
prompted (the OEM workshop, Mathematica reference, timestamp check).

## The lidar story and Friday

The stories tab lists **"Lidar OEM integration"** as one of the four phone-screen stories told to Stanislav on 28 Sep.
Today's Ownership story is the same project. **Don't tell it on Friday.** It's fine for Monday, with a different
interviewer, if it matches the written version.

Today's telling also **contradicts the written one**:

| Written (stories tab) | Told today |
|---|---|
| "Our only hardware change is an external modem; everything else is software" | "It needed HW changes: communication stack, modem and wiring"; "the SW part was the easy part" |
| First OEM failed after months; you gave up; second OEM worked | One OEM; your apt/systemd workshop convinced them and they adapted |
| "About half a dozen lidars in the field. Don't inflate it" | "Now our second most important product" |

Pick what's true and make one version. If both OEMs and the workshop are true, the story is richer, but
it has to be the same story every time.

## Three stories to rewrite (today)

**1. Dive Deep — C → Rust rewrite.** Settle the one true root cause. Fill in:
- What exactly differed (which outputs, how big the difference was).
- The mechanism (an out-of-bounds read at the interval boundary? rounding? both?), stated in one sentence.
- How you proved it: Mathematica reference on x86-64; what it showed.
- Correct the Rust claim: indexing out of bounds **panics at runtime**; it isn't a compile error. If iterators
  avoided the index entirely, say that instead.
- Result with a number, and what changed afterwards (tests, tolerance policy).

**2. Hire and Develop — Meteo-42 student.** This principle carries the most weight in the loop.
- Add the 1:1 where you **asked** why he was struggling, and what he said. If that didn't happen, say so in
  "what I'd do differently" ("I guessed at the cause; now I ask first"), which is a strong answer.
- How you handled his reaction to the feedback.
- Numbers: thesis grade, time to his first merged change, what he owned by the end.
- Lead with one sentence: "I supervised a master's student who was struggling; within three months he finished
  the prototype and we hired him."

**3. Ownership — lidar (for Monday only).** Reconcile with the written version (table above), then add:
- That you volunteered after production and the HW team declined. Say it in the first answer.
- The OEM workshop as the concrete hard moment.
- Honest numbers: units in the field, time taken.
- **For Friday, use the planned Ownership story instead:** the orphaned project at your previous job (no
  documentation, everyone before you a freelancer).

Customer Obsession (South Pole) is good: just add the cost (days, what slipped) and two numbers
(compression ratio or data delivered, customers using the feature now).

## Questions to ask them (two per round type)

| Round | Question |
|---|---|
| Problem solving | "What does code review look like on the team: how big are changes, and how long until they land?" |
| Problem solving | "What's the hardest bug the team has chased recently, and how did you find it?" |
| OS | "How much of the team's work is upstream KVM and Linux, and how much is internal?" |
| OS | "How do you test hypervisor changes before they reach a host with customer instances?" |
| Design | "What's the hardest part of the hypervisor right now: performance, isolation, or something else?" |
| Design | "When a design like live update goes wrong in the fleet, how do you find out first?" |
| Logical & maintainable | "How does a new senior engineer get up to speed: what does the first month look like?" |
| Logical & maintainable | "What separates someone who does well on your team from someone who just does okay?" |

For Stanislav on Friday, one specific: "Your work on guest FPU and XSAVE state: what made that hard?"
(Only if you can follow up on the answer; see the Amazon Nitro reference tab.)

## Before Friday

- [ ] Rewrite the three stories above; tell each aloud once with a timer (2–3 min)
- [ ] Reread OS 2 (physical vs virtual) and the malloc trace; say it aloud in 3 minutes
- [ ] vCPU state list cold: regs, sregs, MSRs, XSAVE, LAPIC, events, TSC (Stanislav's area)
- [ ] Design sheets 5, 6, 4 in that order
- [ ] In every coding round: skeleton that compiles within 8 minutes, trace an example before saying done
