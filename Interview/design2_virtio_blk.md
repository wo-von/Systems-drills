# Design 2 — a virtio-blk device in a VMM, then moved out of the VMM's main path (review sheet, not yet mocked)

**The question:** guests need a block device backed by a file or volume on the host. Design the
virtio-blk device in the VMM: how a request travels from the guest to the backing store and back, then
make it fast.

**The one-paragraph answer:** the guest driver puts each request in a **virtqueue**, a ring in guest
memory: a read-only header (type, sector), the data buffers, and a one-byte writable status. It
notifies the device by writing a doorbell register. With **ioeventfd** that write wakes an I/O thread
without a trip to the VMM's vCPU loop. The I/O thread pops the chain, **copies and validates every
descriptor** (the guest is hostile), does the I/O with `preadv`/`pwritev` or io_uring, writes the
status, pushes the used ring and injects an interrupt through **irqfd**. To go faster, move the data
plane out of the VMM: **vhost-user-blk** (a separate polling process such as SPDK, sharing guest
memory) or hardware offload. On Nitro, EBS volumes show up as NVMe devices served by the Nitro card,
not virtio.

---

## 1. Open with questions

| Question | Assumption to state |
|---|---|
| Backing store? | A raw file or a network volume on the host |
| Load? | Up to ~100k IOPS (4 KiB random) and ~2 GB/s per guest; dozens of guests per host |
| Latency target? | Device overhead well below the storage latency (tens of µs) |
| Durability? | A flush from the guest must mean "on stable storage" |
| Trust | The guest controls the ring contents, all addresses and all lengths |

## 2. The path of one request

```
 guest kernel (virtio_blk driver)                   host
 ┌────────────────────────────────┐
 │ desc table  [0] hdr  (RO) ─────┼─┐  type=IN/OUT/FLUSH, sector
 │             [1] data (RW/RO) ──┼─┤  NEXT-linked chain
 │             [2] status (WO) ───┼─┘
 │ avail ring: idx++ , ring[i]=0  │
 │ write QUEUE_NOTIFY ────────────┼──► EPT/PIO exit ─► KVM: ioeventfd matches ─► eventfd
 └────────────────────────────────┘                                               │
                                     VMM I/O thread (epoll on eventfd) ◄──────────┘
                                       pop avail → copy + validate chain
                                       preadv/pwritev or io_uring on backing file
                                       write status byte, used ring: {id, len}, used.idx++
                                       write irqfd ─► KVM injects MSI-X ─► guest IRQ handler
                                                                           reaps used ring
```

**The split virtqueue** (3 areas in guest memory, all sized `queue_size`, e.g. 256):
- **descriptor table:** `{u64 addr; u32 len; u16 flags (NEXT, WRITE, INDIRECT); u16 next;}`
- **avail ring** (driver → device): `flags, idx, ring[queue_size]` = heads of chains ready
- **used ring** (device → driver): `flags, idx, ring[] of {u32 id; u32 len}` = finished chains

**Memory ordering on both sides:** the device reads `avail->idx` with **acquire** before reading the
entries, and writes the used entries **before** publishing `used->idx` with **release**. The guest
does the mirror image. This is your SPSC ring with each side owning one index.

## 3. Choices

| Box | Choice | Why not the alternative |
|---|---|---|
| Notification | **ioeventfd** + I/O thread | A plain exit to the VMM's vCPU thread costs a userspace round trip and blocks that vCPU during the I/O |
| Completion | **irqfd** (MSI-X) | Injecting through an ioctl from the VMM is slower and serializes |
| Batching | `VIRTIO_RING_F_EVENT_IDX` | Without it, one notify and one interrupt per request. With it, each side says "notify me at index N" |
| I/O submission | **io_uring** or Linux AIO, `O_DIRECT` | Blocking `pread` per request needs one thread per in-flight I/O |
| Queues | **Multiqueue**, one queue per vCPU, each with its own thread | One queue becomes the lock and the bottleneck |
| Polling | Adaptive poll on the avail ring for a few µs before sleeping | Interrupt/notify cost dominates at high IOPS; pure polling burns a core |
| Data plane location | VMM thread → **vhost-user-blk** (separate process, shared guest memory through memfd) → **hardware offload** | Each step removes copies and exits, and moves the parser out of the VMM's privilege |

Note: in-kernel `vhost-blk` was proposed but isn't upstream. The in-kernel option is vhost-scsi. Say
"vhost-user-blk" for blk.

## 4. Security: the guest is hostile

- **Copy each descriptor once** into local memory, then validate the copy. Re-reading it from guest
  memory is a **double fetch** (TOCTOU): the guest changes `len` after the check.
- **Bound every address and length:** `addr + len` must not overflow, and the whole range must lie in one
  memslot. Translate GPA → HVA only after the check.
- **Bound the chain:** at most `queue_size` descriptors, otherwise a guest loops `next` forever. Indirect
  tables: bound their length and forbid nested indirection.
- **Check the request:** `sector + len/512 ≤ capacity`; the header must be device-readable; the status
  descriptor must be device-writable and at least 1 byte.
- Bad request → status `VIRTIO_BLK_S_IOERR` or mark the device broken (`NEEDS_RESET`). Never crash the VMM.
- **Fairness:** a token-bucket rate limit per guest (IOPS and bytes) so one guest can't starve the host.

## 5. Implement: the pop-and-validate loop

```c
/* One queue, one thread. q->last_avail is ours; avail->idx belongs to the guest. */
while (q->last_avail != atomic_load_explicit(&q->avail->idx, memory_order_acquire)) {
    uint16_t head = q->avail->ring[q->last_avail % q->size];   /* guest value: bound it */
    q->last_avail++;
    if (head >= q->size) { device_broken(q); return; }

    struct req r = {0};
    uint16_t i = head;
    for (unsigned n = 0; ; n++) {
        if (n >= q->size) { device_broken(q); return; }          /* loop in the chain */
        struct vring_desc d = q->desc[i];                         /* ONE copy, then use d only */
        void *hva = gpa_to_hva(q->mem, d.addr, d.len);            /* checks overflow + memslot */
        if (!hva) { device_broken(q); return; }
        if (!req_add(&r, hva, d.len, d.flags & VRING_DESC_F_WRITE)) { device_broken(q); return; }
        if (!(d.flags & VRING_DESC_F_NEXT)) break;
        i = d.next;
        if (i >= q->size) { device_broken(q); return; }
    }
    if (!req_parse(&r, q->capacity_sectors)) { complete(q, head, &r, VIRTIO_BLK_S_IOERR); continue; }
    submit_io(q, head, &r);          /* io_uring; completion calls complete() */
}
/* complete(): write status, used->ring[used_idx % size] = {head, written},
   atomic_store_explicit(&used->idx, ++used_idx, memory_order_release), then signal irqfd
   unless the guest's used_event says it doesn't need one yet. */
```

## 6. Scale and defend

- **What fails first at 10× guests:** host CPU for I/O threads; then the backing store's queue depth.
  Watch per-guest IOPS, latency p99, I/O thread CPU, rate-limiter drops.
- **Flush semantics:** advertise `VIRTIO_BLK_F_FLUSH`; on a FLUSH request call `fdatasync` (or the
  volume's flush) before completing. Lying here loses data on host power loss.
- **Test plan:** fio (random 4K, sequential 1M, mixed) against a bare-metal baseline; a **fuzzer** that
  writes random descriptor chains (loops, overflowing `addr+len`, out-of-range heads, changing a
  descriptor after notify); crash-consistency tests (pull power after a FLUSH completes); live
  migration in the middle of I/O (drain in-flight requests, save `last_avail`).
- **Rollout:** behind a feature flag per instance type, canary hosts, roll back by restarting the VMM
  on the old device model (state is just ring indices plus in-flight I/O drained).

## Traps

| Trap | Correct |
|---|---|
| "The device reads the request from the guest" (once) | The guest can rewrite memory at any time; copy each field once, validate the copy |
| Notify = interrupt | Notify is guest → device (a register write, an exit). Interrupt is device → guest (MSI-X) |
| "vhost-blk" | Not upstream. vhost-user-blk (userspace process) or vhost-scsi (kernel) |
| Forgetting the barrier between the used entry and `used->idx` | The guest could read a stale entry; release on the index store |
| Treating FLUSH as optional | It's the durability contract |

## Check you can explain these cold

- [ ] The three parts of a split virtqueue and who writes each
- [ ] One request end to end, naming ioeventfd and irqfd
- [ ] Five validation checks on a descriptor chain, and the double-fetch bug
- [ ] What EVENT_IDX saves
- [ ] Why move the data plane out of the VMM, and the options
