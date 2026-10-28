# Design 5 — guest memory on a host: overcommit, ballooning, huge pages, NUMA (review sheet, not yet mocked)

**The question:** a host has a fixed amount of RAM and runs many guests. Design how guest memory is
provided: how it's backed, whether you overcommit, how you reclaim, and how you make it fast.

**The one-paragraph answer:** guest RAM is a region the VMM maps (anonymous memory, memfd or
hugetlbfs) and registers with KVM as **memslots**. The host backs it lazily through EPT violations, or
upfront with prefaulting when predictability matters. For a public cloud selling fixed memory sizes,
**don't overcommit**: back each guest with **huge pages** (fewer TLB misses, cheaper 2-D walks, smaller
page tables) on the **NUMA node** where its vCPUs run, and **zero every page** before it reaches a
guest. Where density matters more than predictability, overcommit with these reclaim tools in order:
untouched pages cost nothing; **ballooning** or **free page reporting** returns guest-free pages;
**KSM** dedupes, with a side-channel risk; host **swap** is the last resort because of double paging.

---

## 1. Open with questions

| Question | Assumption to state |
|---|---|
| Sold as fixed sizes, or best effort? | Fixed sizes: the customer paid for 64 GiB and expects 64 GiB |
| Host size? | 2 sockets, 1–2 TiB, guests of 2–512 GiB |
| Latency sensitive? | Yes: databases, HPC. Predictable memory latency matters |
| Can guests run our drivers (balloon)? | Not guaranteed; customers bring their own OS |
| Isolation | Guests are hostile to each other; no data may leak across |

## 2. Where guest memory lives

```
 guest virtual ──(guest page tables, guest-owned)──► guest physical (GPA)
                                                          │ memslot: GPA range → HVA range in the VMM
                                                          ▼
 VMM process: mmap(guest RAM)  (anon / memfd / hugetlbfs) = host virtual (HVA)
                                                          │ host page tables + EPT (GPA → HPA)
                                                          ▼
 host physical frames (struct page, buddy allocator, per NUMA node)
```

First guest touch of a GPA → EPT violation → KVM looks up the memslot → host fault on the HVA →
frame from the buddy allocator → EPT entry → resume. **Prefaulting** (`MAP_POPULATE`, or touching all
memory at launch) moves that cost to launch time and finds out upfront whether the memory exists.

## 3. Overcommit: should you?

| Mechanism | How it reclaims | Cost and risk |
|---|---|---|
| Lazy backing | Never-touched pages have no frame | Guests that touch everything (most do, eventually) reclaim nothing |
| **Ballooning** (virtio-balloon) | Guest driver allocates pages inside the guest and reports their GPAs; host drops them (`madvise(MADV_DONTNEED)`) | Needs a cooperating guest driver; guest may OOM or swap if inflated too far; splits huge pages |
| **Free page reporting** | Guest reports pages it has freed; host drops them | Needs the driver; only reclaims what's actually free |
| **virtio-mem** | Hot-unplug guest memory in blocks | Driver support; coarse |
| **KSM** | Merge identical pages copy-on-write | CPU to scan; **cross-VM side channels** (timing of COW faults); avoid between tenants |
| Host swap | Host pages guest memory to disk | **Double paging:** host swaps a page the guest is about to swap too; latency spikes the guest can't explain |

The answer to give for EC2-style fixed instances: **no memory overcommit**. The guest gets dedicated
memory, so its performance is predictable. Overcommit is for dense, cheaper tiers where the trade-off is
stated to the customer. Say the trade-off explicitly; that's what the interviewer wants to hear.

## 4. Huge pages

**Numbers to put on the board** (512 GiB guest):

```
4 KiB pages: 512 GiB / 4 KiB = 128 Mi leaf entries × 8 B = 1 GiB of leaf page tables
2 MiB pages: 256 Ki entries × 8 B                          = 2 MiB
1 GiB pages: 512 entries                                   = 4 KiB
```

- **TLB reach:** ~1,500 L2 TLB entries × 4 KiB ≈ 6 MiB; × 2 MiB ≈ 3 GiB.
- **2-D walk:** a TLB miss with 4-level guest and 4-level EPT tables can take up to 24 memory
  references. Huge EPT pages remove levels from the host side of every one of them.
- **hugetlbfs** (reserved at boot, never swapped, never split): deterministic, chosen for VMs. **THP**
  (transparent): flexible, but allocation can fail under fragmentation, and compaction adds jitter.
- What breaks huge pages: dirty logging for live migration (split to 4 KiB), ballooning at 4 KiB, KSM.
- The guest should use huge pages too; the gain multiplies.

## 5. NUMA

```
 socket 0                          socket 1
 ┌──────────────┐  interconnect   ┌──────────────┐
 │ cores 0–47   │◄───────────────►│ cores 48–95  │
 │ memory node 0│   remote access │ memory node 1│
 └──────────────┘   ~1.5–2× slower└──────────────┘   (order of magnitude; say "approximately")
```

- Pin the guest's vCPU threads to one node's cores, and allocate its memory from that node
  (`mbind` / `set_mempolicy` on the guest RAM region, or hugetlbfs pages from that node).
- A guest larger than one node: split it, and give it a **virtual NUMA topology** (ACPI SRAT) that
  matches the host, so the guest's own scheduler and allocator keep data local.
- Turn off automatic NUMA balancing for pinned guests: page migration adds jitter.
- Device locality: put the guest's passed-through NIC/NVMe on the same node.

## 6. Isolation and security

- **Zero every frame before it reaches a guest** (and scrub on teardown), or one tenant reads another's data.
- No KSM across tenants (side channel). Consider hardware memory encryption where available.
- The VMM's own memory and host kernel memory must never be mapped into a memslot.
- Host OOM: set the VMM processes' `oom_score_adj` so the host kills anything else first, and size
  host-reserved memory (page tables, VMM, device rings) explicitly.

## 7. Implement: the balloon inflate handler in the VMM

```c
/* Guest put an array of 32-bit PFNs (4 KiB units) in an inflate-queue buffer. */
static void balloon_inflate(struct vm *vm, const uint32_t *pfns_guest, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        uint64_t gpa = (uint64_t)pfns_guest[i] << 12;      /* read once; guest-controlled */
        void *hva = gpa_to_hva(vm, gpa, 4096);              /* bound: must be inside a RAM memslot */
        if (!hva) { vm->stats.balloon_bad_pfn++; continue; }
        if (madvise(hva, 4096, MADV_DONTNEED) != 0)          /* frame back to the host */
            vm->stats.balloon_madvise_err++;
        vm->balloon_pages++;
    }
}
/* Deflate needs nothing: the next guest touch faults a fresh zeroed page in. */
```

Points to say: validate every PFN; the guest may report the same page twice (idempotent); on hugetlbfs
backing, 4 KiB `DONTNEED` doesn't free anything (use `fallocate(PUNCH_HOLE)` on 2 MiB-aligned ranges, or
don't balloon).

## 8. Scale and defend

- Metrics: host free memory per node, remote-access ratio (perf `numa` counters), huge page usage and
  THP fallbacks, balloon size per guest, host swap in/out (should be zero), EPT violation rate.
- What fails first with denser hosts: fragmentation (no 2 MiB pages left), then host memory pressure.
- Test: a guest runs a memory-latency benchmark pinned local vs remote; launch/teardown in a loop and
  check that scrubbing leaves no data (write a pattern, tear down, launch, scan for the pattern).

## Traps

| Trap | Correct |
|---|---|
| "The host allocates guest memory when the VM starts" | Only with prefault/hugetlbfs reservation; otherwise lazily, per EPT violation |
| "Ballooning frees memory" automatically | Only with a guest driver that cooperates; customers' OS may not run it |
| Huge pages only help TLB hits | They also shrink page tables and shorten every 2-D walk |
| KSM is free density | It's a cross-tenant side channel |
| Forgetting to zero pages | The most serious bug in this design: data leaks across tenants |

## Check you can explain these cold

- [ ] The GPA → HVA → HPA chain and who owns each mapping
- [ ] Four overcommit mechanisms and the cost of each; why fixed-size instances don't overcommit
- [ ] The page-table size numbers for 4 KiB vs 2 MiB vs 1 GiB
- [ ] hugetlbfs vs THP
- [ ] NUMA placement for a guest bigger than one node
- [ ] The balloon handler and its validation
