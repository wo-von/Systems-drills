# Design — live migration of a running VM (mock loop round 3, Wed 28 Oct)

**The question:** a fleet of hosts runs customer VMs. A host has to be emptied for maintenance (a
firmware update, a failing part) and the guests must move to another host without the customer
noticing. Design live migration of a running VM from host A to host B.

**The one-paragraph answer:** start a VMM on B that waits for an incoming stream. Turn on dirty logging
for the guest on A and copy all of guest RAM while the guest keeps running (**pre-copy**). Then repeat:
fetch and clear the dirty bitmap and send only the pages written since the last round. Once the
remaining dirty set fits inside the pause budget, **pause** the guest. Send the last dirty pages, the
vCPU state and the device state, then move storage and the network to B and **resume** on B. A keeps
its paused copy until B confirms that the guest is running, so a failure before that point means
"resume on A". If the dirty set won't shrink, throttle the vCPUs or switch to **post-copy**.

---

## 1. Open with questions (3–5 min), then state the numbers

| Question | Assumption to state |
|---|---|
| Is the guest busy the whole time? Is there a quiet window? | No window. Assume a live database, 24/7 |
| Pause budget? | Guest fully stopped for at most a few hundred ms; TCP connections survive |
| Guest size and write rate? | 2 vCPU / 8 GiB up to 64 vCPU / 512 GiB; some write 1–2 GB/s |
| Storage? | Network-attached (EBS-like), reachable from both hosts, so no disk copy |
| Hosts? | Same AZ, same CPU model and hypervisor version, 25 Gbps between them |
| Pass-through devices (SR-IOV NIC, NVMe)? | Out of scope at first; name them as the hard case |
| Deadline? | The host must be empty by a set time; we can't wait forever |

**Write the numbers on the board, with units said out loud:**

```
512 GiB × 8 bit/B ≈ 4 400 Gbit
4 400 Gbit ÷ 25 Gbit/s ≈ 176 s     ≈ 3 min for the first full copy (more in practice)

Pause budget 300 ms × 25 Gbit/s ≈ 7.5 Gbit ≈ 0.9 GB
→ the final dirty set must be under ~0.9 GB, minus vCPU + device state and resume time
```

That last number drives the whole design: **you can't pause until the dirty set is under about 1 GB.**

## 2. The parts

```
            control plane (orchestrator: picks B, owns "who runs this VM")
                 │ start migration            │ commit / abort
                 ▼                            ▼
  ┌──────────── host A ────────────┐   ┌──────────── host B ────────────┐
  │  guest VM (running)            │   │  guest VM (not running yet,     │
  │   vCPUs ─ guest RAM ─ virtio   │   │   memory filled as it arrives)  │
  │       │        ▲               │   │                                 │
  │  VMM (QEMU / Firecracker)      │   │  VMM, started first, waiting    │
  │   - migration thread  ─────────┼───┼─► receiver: writes pages to     │
  │   - reads dirty bitmap         │   │   guest RAM, loads state        │
  │        ▲                       │   │        │                        │
  │  KVM: dirty logging (EPT WP    │   │  KVM: empty EPT, filled lazily   │
  │       or EPT D-bits + PML)     │   │       after resume              │
  └────────────────────────────────┘   └─────────────────────────────────┘
          │  encrypted, authenticated migration stream, 25 Gbps  │
          └──────────────────────────────────────────────────────┘

  network: VPC mapping  VM IP/MAC → host A  ──(at cutover)──►  → host B
  storage: volume attached to A  ──(drain, detach, attach)──►  attached to B
```

Encrypt the stream: guest RAM contains customer secrets (keys, passwords).

## 3. How you know which pages changed: dirty logging

The guest writes RAM directly through the EPT. No exit happens, so the hypervisor needs a trap or a
hardware log.

**Option A: write-protection (works everywhere)**

```
VMM: KVM_SET_USER_MEMORY_REGION(flags |= KVM_MEM_LOG_DIRTY_PAGES)
KVM: clear the W bit in every EPT entry of that memslot

guest writes page P
  → EPT violation (write to a read-only page) → VM exit
  → KVM: set bit P in the dirty bitmap, make P writable again
  → VM entry, the write completes        (one exit per page per round, not per write)

each round, the VMM:
  KVM_GET_DIRTY_LOG  (or KVM_CLEAR_DIRTY_LOG)  → bitmap, then reset
  KVM write-protects the pages that were dirty again
  VMM sends the CURRENT contents of every page whose bit is set
```

**Option B: hardware (Intel EPT A/D bits + PML)**: the CPU sets the EPT **dirty bit** and appends
the GPA to a 512-entry **page modification log**. There's a VM exit only when the log is full, so
there are far fewer exits than with write-protection. The **dirty ring** (`KVM_CAP_DIRTY_LOG_RING`)
gives the VMM a per-vCPU ring of dirty GPAs instead of a whole bitmap, which is cheaper for huge guests.

**Cost to mention:** while logging is on, huge pages are split into 4 KiB entries so a dirty bit
covers 4 KiB, not 2 MiB. The guest runs slower during the migration.

**What you send** is the page, not a log of writes. If a page is written 10 times between two rounds,
it's sent once.

## 4. Pre-copy rounds and when to stop

```
round 0   ████████████████████████████████████  all 512 GiB, ~176 s, guest running
round 1   ██████████                             pages dirtied during round 0
round 2   ████                                   pages dirtied during round 1
round 3   ██                                     ...
          ▲ dirty set ≤ ~0.9 GB → PAUSE
stop+copy ▌  last dirty pages + vCPU + devices   guest paused (the downtime)
resume on B ──────────────────────────────────►
```

- Each round lasts (dirty set ÷ bandwidth). It **converges** while the rate of *new distinct* dirty
  pages stays below the link rate: at 16 Gbit/s of dirtying on a 25 Gbit/s link, each round is at
  most 0.64× the previous one.
- The dirty set is a **set of pages**, bounded by the guest's writable working set. A database that
  writes 2 GB/s into the same 20 GiB never has more than 20 GiB dirty, but 20 GiB is ~7 s to
  send, which is still far over budget.
- **Stop rule:** pause when `remaining_dirty ÷ measured_throughput + fixed_cost ≤ budget`, where
  `fixed_cost` is the vCPU and device state plus resume time. Also stop iterating when the dirty set
  hasn't shrunk for *k* rounds; that's the signal to use a lever.

## 5. Levers when it won't converge

| Lever | What it does | Cost |
|---|---|---|
| **Auto-converge** (throttle vCPUs) | Takes CPU time away from the guest, so it dirties fewer pages | Customer sees a slowdown for seconds to minutes |
| **Post-copy** | Pause briefly, move only the vCPU and device state, resume on B now; B fetches missing pages from A on demand (`userfaultfd`), A pushes the rest in the background | During post-copy, **neither host has the whole VM**: if A or B dies, the guest is lost. Each missing page costs a network round trip |
| **Hybrid** | A few pre-copy rounds, then switch to post-copy | Smaller window of the post-copy risk |
| **Less data per page** | Skip zero pages, compress, send deltas of hot pages (XBZRLE) | CPU on both hosts |
| **More bandwidth** | Several streams (multifd), a dedicated link | Network capacity |
| **Abort and reschedule** | Try another target or time | Only works if the deadline allows |

Say which one you'd pick: pre-copy by default; auto-converge for moderate writers; hybrid post-copy
for the largest write-heavy guests, with the risk stated.

## 6. The cutover (stop-and-copy)

```
A (VMM)                         B (VMM)                     control plane
  │ pause vCPUs                   │                             │
  │ drain in-flight virtio I/O    │                             │
  │ last dirty pages ────────────►│                             │
  │ vCPU state ──────────────────►│ load into KVM               │
  │ device state ────────────────►│ load into devices           │
  │                               │ "state complete" ──────────►│
  │                               │                 attach volume to B (fence A)
  │                               │                 VPC mapping: VM → B
  │                               │◄────────────── "commit, resume"
  │                               │ resume vCPUs; send GARP/RARP
  │                               │ "running" ─────────────────►│
  │◄────────────────────────────────────────────── "release"    │
  │ free guest RAM, destroy VM    │                             │
```

**What moves besides RAM:**

- **vCPU (per vCPU):** general registers (`KVM_GET_REGS`); segments and CR0/3/4, EFER
  (`KVM_GET_SREGS`); **MSRs**; **XSAVE** FPU/SSE/AVX state and XCR0; **local APIC**; pending
  exceptions, interrupts, NMIs (`KVM_GET_VCPU_EVENTS`); debug registers; nested state if the guest runs VMs.
- **Platform:** IOAPIC/PIC (`KVM_GET_IRQCHIP`), PIT, **kvmclock** (`KVM_GET_CLOCK`).
- **Time:** the guest TSC must not jump backwards or by a large amount. Set B's TSC offset so the guest
  continues from the value it had at the pause. Same CPU model means same TSC frequency; otherwise use
  TSC scaling.
- **Devices:** virtio feature bits, queue addresses, ring indices (`last_avail_idx`). Finish or drain
  in-flight I/O first so no request is half done.
- **Not sent:** the **EPT**. It maps GPA → host A's physical frames, which mean nothing on B. B starts
  with an empty EPT and fills it through EPT violations after resume (a warm-up cost). The guest's own
  page tables are ordinary guest RAM and were already copied.
- **Not booted:** B never boots the guest. It resumes it at the exact instruction where it stopped.

**Network:** the control plane updates the mapping (VM IP/MAC → host B). On plain L2, B sends a
gratuitous ARP/RARP so switches learn the new port. Packets sent during the pause are dropped and TCP
retransmits them, which is why connections survive a few hundred ms.

**Storage:** complete in-flight I/O on A, then detach and attach on B. **Fence A** first, so the two
hosts never write the same volume at once.

## 7. Failure: one owner at a time

```
       ┌────────── A owns the VM ──────────┐        ┌──── B owns the VM ────┐
start ─► pre-copy ─► paused on A ─► B: "state complete" ─► COMMIT ─► running on B ─► A frees
           │              │              │                   ▲
           └── any failure (B dies, link drops, timeout) ────┘ before COMMIT
                         → abort, resume on A (customer sees a slightly longer pause, or nothing)
```

- The **commit point** is the control plane switching ownership to B. Before it, A is authoritative.
  After it, B is.
- Never resume on both. Ownership is one record in the control plane, and the volume fence enforces it.
- Post-copy breaks this rule on purpose (state is split across A and B), which is why it's the
  risky lever.

## 8. How you know it worked

- Per migration: total time, **measured downtime** (pause → resume), bytes sent, rounds, dirty rate per
  round, which lever was used, abort reason.
- From the guest's side: a gap in a heartbeat or ping, steal time, and the TCP retransmit spike.
- Fleet: success rate, number of aborts, p99 downtime by instance size, and hosts emptied by their deadline.
- Before shipping it: migrate test guests in a loop while they run a memory-writing load and check a
  checksum of their memory and their clock.

---

## What you said and what to say instead

| You said | Say instead |
|---|---|
| Opened with a switch that buffers all client traffic during the copy | The guest keeps running and serving clients during the copy. Only the final pause (≤ ~300 ms) stops traffic, and TCP covers it |
| "Snapshot, copy, apply the changes" | Right idea. Call it **pre-copy with dirty logging**, iterated until the dirty set fits the budget |
| 512 GiB over 25 Gbps = 22.5 s | ~176 s. Bytes → bits (×8). Say units aloud |
| "2 GB/s × 3 min = 360 GB to resend" | The dirty set is a set of **pages**, bounded by the writable working set. Rewrites count once |
| "Diff the current state against the snapshot" | That reads 512 GiB every round. Use **dirty logging**: write-protect in the EPT, or EPT dirty bits + PML |
| "Transfer the logs" | Send the current **contents** of the pages in the dirty bitmap |
| "Wait for a quiet window for heavy users" | No window exists. Use **auto-converge**, **post-copy** or hybrid, with their costs |
| "At some point we have to pause" | Stop rule: `dirty ÷ throughput + fixed cost ≤ budget`, or no progress for *k* rounds |
| "Send RAM, the page tables and all the registers" | Guest page tables are in RAM already. Name the vCPU state: regs, sregs, MSRs, XSAVE, LAPIC, pending events, TSC |
| "The EPT should also be sent" | **Never.** It maps to A's frames. B rebuilds its own lazily |
| "4× the time plus the bootup of B" | B doesn't boot. Its VMM is started first and **resumes** the guest |
| "Packets go to B if the VM is marked migrated" | Control plane updates the IP/MAC → host mapping; GARP/RARP on L2; TCP retransmits what the pause dropped |
| B crashes → keep running on A | Right. Add the **commit point**: A frees only after B says it is running |
| Never mentioned devices, storage, time | virtio ring state, drain I/O, fence then re-attach the volume, TSC offset and kvmclock |

## Check you can explain these cold

- [ ] The two numbers: first copy of 512 GiB at 25 Gbps (~3 min) and the max final dirty set for a 300 ms pause (~0.9 GB)
- [ ] Dirty logging with write-protection, step by step, and what PML changes
- [ ] Why the dirty set is bounded by the working set, and when pre-copy fails to converge
- [ ] The stop rule as a formula
- [ ] Auto-converge vs post-copy: what each costs, and why post-copy is risky
- [ ] The vCPU state list (seven items) and why the EPT isn't sent
- [ ] How the TSC stays sane across the move
- [ ] The cutover sequence, including storage fencing and the network mapping
- [ ] The commit point, and what the customer sees when B dies before and after it
- [ ] Three metrics that show a migration went well
