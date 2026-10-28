# Design 7 — snapshot and restore for microVMs, Firecracker-style (review sheet, not yet mocked)

**The question:** functions run in microVMs. Booting a fresh VM and initializing the runtime takes
seconds. Design snapshot and restore so that a new VM resumes an already-initialized one in
milliseconds, possibly many clones from one snapshot.

**The one-paragraph answer:** to snapshot, **pause** the vCPUs, then write three things: the **vCPU
state** (the same list as for live migration), the **device state** (virtio queue indices, MMIO
devices, timers), and **guest memory** as a file (full, or a diff of the pages dirtied since the last
snapshot, from dirty logging). To restore, the VMM loads the state and **maps the memory file
`MAP_PRIVATE`** instead of reading it. Pages load lazily on first touch from the page cache,
copy-on-write, so many clones share one file. For memory on another machine, a **userfaultfd** handler
fetches pages on demand. The hard part isn't the copy but the **clones**: they share RNG seeds, IDs, TCP
state and a clock that jumped. Signal the guest through **VMGenID** to reseed and regenerate identity,
and fix the clock.

---

## 1. Open with questions

| Question | Assumption to state |
|---|---|
| Purpose | Fast start: resume a VM whose runtime is already warm |
| How many clones per snapshot? | Many, concurrently, on many hosts |
| VM size? | 128 MiB – 10 GiB |
| Restore target | Tens of ms to first instruction; working set loaded quickly after |
| Same host? | Not necessarily: same CPU template and VMM version required |

## 2. The parts

```
 SNAPSHOT (once)                                   RESTORE (many times)
 ┌────────────────────────┐                        ┌──────────────────────────────────┐
 │ running microVM        │                        │ new VMM process                   │
 │  pause vCPUs           │                        │  KVM_CREATE_VM, vCPUs             │
 │  save vCPU state ──────┼──► state file ────────►│  load vCPU + device state         │
 │  save device state ────┼──►  (versioned)        │  mmap(mem file, MAP_PRIVATE)      │
 │  write guest memory ───┼──► memory file ───────►│   → memslot (no copy at restore)  │
 │  (full or diff)        │    (page cache / S3)   │  VMGenID bump, clock fix          │
 └────────────────────────┘                        │  resume vCPUs                     │
                                                   └──────────────────────────────────┘
 first touch of a page after resume:
   EPT violation → host fault on HVA → page cache hit (or disk read) → private COW copy only on write
```

## 3. Memory loading: three options

| Option | How | Trade-off |
|---|---|---|
| **Read the whole file** at restore | `read()` into anonymous memory | Simple; restore time grows with VM size |
| **`mmap` MAP_PRIVATE** (Firecracker's default) | Pages fault in from the page cache; writes go to private copies | Fast restore; clones on one host share the clean pages; first touches cost a fault each |
| **userfaultfd** | Register guest RAM; a handler thread gets each missing-page fault and supplies the page (`UFFDIO_COPY`) | Pages can come from anywhere (network, a compressed store); you control prefetching |

**Prefetch the working set:** record which pages were touched in the first N ms after a test restore,
and load those first. This turns hundreds of serial faults into one batched read.

**Diff snapshots:** keep dirty logging on; a diff snapshot writes only the pages dirtied since the
base snapshot. Restore = base file + diff layered over it.

## 4. Clones: the uniqueness problem

| What's duplicated | Why it hurts | Fix |
|---|---|---|
| RNG state (kernel CRNG, userspace PRNGs) | Two clones produce the same "random" keys, nonces, TLS session IDs | **VMGenID** device: a 128-bit generation ID the guest kernel watches; Linux reseeds its RNG when it changes (since 5.18). Userspace libraries need to reseed too |
| Machine IDs, UUIDs, hostnames | Collisions in logs, leases, distributed systems | Regenerate on the generation change; inject through metadata |
| Network identity, open TCP connections | Same MAC/IP; connections with the same sequence numbers | Fresh tap device/MAC per clone; drop connections at snapshot time; the guest re-runs DHCP or gets config |
| Clock | Guest time is the snapshot time, maybe hours old | Update kvmclock/TSC offset on restore; guest steps time on the generation change |

Say: **"a snapshot is safe to resume once; cloning needs guest cooperation."**

## 5. What else to save, and compatibility

- vCPU: registers, sregs, MSRs, XSAVE, LAPIC, pending events, TSC (same list as live migration).
- Devices: virtio feature bits, queue addresses and indices, in-flight requests drained before the
  pause; serial, RTC, i8042 state.
- **Format versioning:** the state file carries a version; a newer VMM reads older versions. Restore on
  a host with the **same CPU template** (CPUID filtered to a common set) or the guest crashes on a
  missing instruction.
- **Integrity and confidentiality:** a snapshot contains everything in guest memory (secrets).
  Encrypt it at rest, sign it, and verify before restore. A tampered state file is an attack on the VMM's
  parser, so validate every field like guest input.

## 6. Implement: the userfaultfd handler loop

```c
/* Guest RAM region [base, base+size) registered with UFFDIO_REGISTER (MISSING mode). */
for (;;) {
    struct pollfd p = { .fd = uffd, .events = POLLIN };
    if (poll(&p, 1, -1) < 0) { if (errno == EINTR) continue; die("poll"); }

    struct uffd_msg msg;
    ssize_t n = read(uffd, &msg, sizeof msg);
    if (n != sizeof msg) { if (n < 0 && errno == EAGAIN) continue; die("read uffd"); }
    if (msg.event != UFFD_EVENT_PAGEFAULT) continue;          /* also handle REMOVE/UNMAP */

    uint64_t addr = msg.arg.pagefault.address & ~(PAGE_SIZE - 1);
    uint64_t off  = addr - base;                              /* offset into the memory file */
    if (off >= size) die("fault outside guest RAM");

    fetch_page(store, off, buf);                              /* local file, network, cache */
    struct uffdio_copy c = { .dst = addr, .src = (uint64_t)buf, .len = PAGE_SIZE, .mode = 0 };
    if (ioctl(uffd, UFFDIO_COPY, &c) < 0 && errno != EEXIST)  /* EEXIST: another thread won */
        die("UFFDIO_COPY");
}
```

## 7. Scale and defend

- Metrics: restore time to first instruction, time to working set loaded, faults per restore,
  page-cache hit rate, number of clones per snapshot, restore failures by reason (version, CPU template).
- What fails first: disk/network bandwidth for cold snapshots, then page-cache memory on the host.
- Test: restore 1,000 clones and check that their RNG outputs, MACs and machine IDs all differ; restore
  on hosts with different microcode/CPU steppings in the same template; corrupt a state file and check
  the VMM rejects it cleanly; snapshot during heavy I/O.

## Traps

| Trap | Correct |
|---|---|
| "Restore = copy memory back" | Map the file and fault pages lazily; copying is the slow option |
| Clones are just copies, so they're fine | Same RNG, IDs and network identity: a security bug |
| Snapshot is a backup of the disk | It's memory + CPU + device state; the disk is separate (and must be consistent with it) |
| Restore anywhere | Same CPU template and compatible VMM version only |
| Trust the snapshot file | Validate it like guest input; encrypt and sign it |

## Check you can explain these cold

- [ ] The three things a snapshot contains
- [ ] mmap MAP_PRIVATE vs userfaultfd for restore
- [ ] Four uniqueness problems of clones and the fix for each, naming VMGenID
- [ ] Why the CPU template matters
- [ ] The userfaultfd loop
