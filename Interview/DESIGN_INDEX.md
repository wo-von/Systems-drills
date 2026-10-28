# Design round: index and the frame every answer uses

## The 45-minute frame (from the loop brief)

| Minutes | Step | Say / draw |
|---|---|---|
| 5 | **Requirements** | Customers and one need each; the numbers (VMs per host, sizes, throughput, latency, pause budget); **the guest is hostile**. Say each assumption aloud and write it down |
| 10 | **Workflow** | The main path end to end. Boxes = components, arrows = how they talk: ring, doorbell, interrupt, ioctl, RPC |
| 10 | **Choices** | For each box: the alternative and why not. Security: copy guest data before validating, bound every guest index and length, least privilege. Performance: exits, copies, batching, polling vs interrupts, NUMA, huge pages |
| 10 | **Implement** | The core piece in C (every sheet has one) |
| 10 | **Scale and defend** | 10× load: what fails first, how you'd see it, rollout and rollback, how test engineers prove it |

## The questions

| # | Question | Sheet | One-line answer |
|---|---|---|---|
| 0 | The most complex system you've worked on | [design0_platform.md](design0_platform.md) | Your own platform, drawn in 5 minutes |
| 1 | Console and log channel from every guest | [design1_console.md](design1_console.md) | Emulated 16550 → VMM → capped per-guest log, outside the guest's failure domain; virtio-console for bulk |
| 2 | virtio-blk in a VMM, then off the main path | [design2_virtio_blk.md](design2_virtio_blk.md) | Virtqueue + ioeventfd/irqfd + I/O thread; copy-then-validate every descriptor; vhost-user-blk or offload for speed |
| 3 | A device from its datasheet (16550 UART / watchdog) | [design3_device_from_datasheet.md](design3_device_from_datasheet.md) | Driver: ISR loops on IIR; emulation: register struct, IRQ level = f(state), never block the vCPU; test against the datasheet |
| 4 | Live migration | [design_live_migration.md](design_live_migration.md) | Pre-copy with dirty logging until the dirty set fits the pause budget; stop-and-copy; commit point; auto-converge/post-copy |
| 5 | Guest memory: overcommit, ballooning, huge pages, NUMA | [design5_guest_memory.md](design5_guest_memory.md) | Fixed instances: no overcommit, hugetlbfs on the vCPUs' node, zero every page; overcommit tools ranked by cost |
| 6 | Launch and teardown on a Nitro-style host | [design6_vm_launch_teardown.md](design6_vm_launch_teardown.md) | Reserve → IOMMU → VMM + KVM ioctls → boot; teardown: FLR + IOMMU unmap before free, scrub, idempotent rollback |
| 7 | Snapshot and restore for microVMs | [design7_snapshot_restore.md](design7_snapshot_restore.md) | vCPU + device state + memory file; restore by mmap MAP_PRIVATE or userfaultfd; clones need VMGenID |
| 8 | Fleet update without evicting instances | [design8_fleet_live_update.md](design8_fleet_live_update.md) | Smallest tool that fits; kexec with guest memory + IOMMU preserved; versioned state both ways; canary → waves → halt |

## Ideas that come back in every answer

- **The guest is hostile:** copy once, validate the copy, bound everything, never crash the host.
- **State lists repeat:** vCPU (regs, sregs, MSRs, XSAVE, LAPIC, events, TSC) + device state appear in
  migration, snapshot and live update. Learn the list once.
- **Three translations:** GVA → GPA (guest), GPA → HVA (memslot), GPA → HPA (EPT). Say which one you mean.
- **Exits are the cost:** ioeventfd, irqfd, batching, polling and passthrough all exist to avoid them.
- **One owner at a time** (migration commit point, volume fencing, teardown order).
- **Numbers on the board, units said aloud.**
- **Prove it:** metrics you'd watch, the fuzz/soak test, canary rollout and rollback.
