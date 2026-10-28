# Design 8 — updating hypervisor-host software across a fleet without evicting instances (review sheet, not yet mocked)

**The question:** you need to roll a new host kernel and hypervisor to every host in the fleet. Guests
must keep running: no reboot, no eviction, and ideally no noticeable pause. Design the update on one
host, then the rollout across the fleet.

**The one-paragraph answer:** choose the lightest tool that fits the change: **live patching** for
small fixes, **restarting the VMM process** with its state handed over for userspace changes,
**live migration** away when there's spare capacity, and for a new host kernel, **kexec with guest
memory preserved**. Stage and verify the new kernel. Serialize each VM's state (vCPU, devices, KVM
metadata) into memory the next kernel won't touch, along with a map of the guest RAM frames. Pause all
vCPUs, kexec without going through firmware, let the new kernel reserve those frames, recreate the
VMs from the serialized state, and resume. The pause is the **blackout**, and the goal is well under a
second. The hard parts are passed-through devices doing DMA during the switch (IOMMU tables must stay
valid), timekeeping, and **state format compatibility** in both directions so you can roll back. The
fleet rollout is canary → waves → automatic halt on health metrics.

Note: AWS has said publicly that Nitro hypervisor updates happen without rebooting instances; the
mechanism isn't public. Upstream Linux is adding this as **KHO (Kexec HandOver)** and live-update
work. Present the design as yours.

---

## 1. Open with questions

| Question | Assumption to state |
|---|---|
| What changes? | Host kernel + KVM + VMM; sometimes only the VMM or a small fix |
| Pause budget? | Blackout < 1 s, target tens to hundreds of ms; no guest reboot, connections survive |
| Spare capacity to migrate everyone away? | Not for the whole fleet at once |
| Fleet size? | Hundreds of thousands of hosts, many hardware generations |
| Devices | NIC/NVMe VFs passed through from an offload card; they keep running |

## 2. Pick the tool for the change

| Tool | Changes it covers | Guest impact | Limits |
|---|---|---|---|
| **Live patch** (kpatch/livepatch) | Small function-level fixes in the kernel | None | No data-structure changes; not a version upgrade |
| **VMM restart with state handoff** | VMM userspace only | Short pause of that guest | KVM VM fd and guest memory must survive (pass fds over a Unix socket) |
| **Live migration away** | Anything | Migration pause per guest | Needs spare capacity, network, time per guest |
| **kexec with memory preserved** | The whole host kernel + KVM + VMM | One blackout per host | The hardest to build; this sheet |

## 3. One host: kexec live update

```
 old kernel                                                           new kernel
 ─────────────────────────────────────────────────────────────────────────────────────────►
 1 stage      2 prepare            3 BLACKOUT (vCPUs paused)                4 resume
 kexec_file_  serialize per VM:    ├ final state save (vCPU, devices)       ┤
 load (signed)  - memory map       ├ quiesce: stop device-model threads     ┤
              (GPA → frames)       ├ kexec: jump to new kernel, no firmware ┤
              - device state       ├ new kernel: reserve preserved frames   ┤
              into preserved       │   and the handover data before the     │
              memory (KHO)         │   buddy allocator starts               │
                                   ├ new KVM + VMMs: rebuild VMs from state ┤
                                   ├ re-attach VFs, IOMMU tables still valid┤
                                   └ resume vCPUs ──────────────────────────┘
```

**What must survive the kexec:**
- **Guest RAM**: the frames stay where they are; the new kernel is told not to hand them to its allocator.
- **The handover data**: per-VM serialized state + the GPA → frame map, in a known format, at a
  location passed to the new kernel.
- **IOMMU page tables** for passed-through devices: the devices keep doing DMA; if the new kernel
  resets or rebuilds the IOMMU from scratch, in-flight DMA faults or lands in the wrong place.
  Either preserve the tables or quiesce the devices first (which costs blackout time).

**What is rebuilt:** EPT (lazily, from the memory map), KVM structures, VMM processes, host page tables.

## 4. The hard parts

- **Blackout time:** dominated by the new kernel's boot to the point where VMs resume. Shrink it by
  skipping firmware (kexec), deferring every non-essential driver and service until after resume, and
  restoring VMs in parallel.
- **Time:** guests must not see a clock jump. Save the TSC offset and kvmclock, and restore them so guest
  time continues. Report the stolen time to the guest as steal time.
- **Interrupts and timers** pending at the pause: saved as vCPU events and LAPIC state, re-injected on resume.
- **Compatibility:** the old kernel writes the state, the new one reads it. Use a **versioned,
  self-describing format** (TLV), and support reading both N-1 and N+1 so **rollback** works the same way.
- **Failure during blackout:** the new kernel can't restore a VM → that's the worst case. Mitigate:
  validate the handover data before jumping (a dry run), keep the old kernel staged for a kexec back,
  and treat any VM that can't be restored as an instance failure to report, not a host-wide crash.

## 5. Implement: a versioned state record

```c
/* Every record: type, version, length, then payload. Unknown types are skipped, not fatal. */
struct rec_hdr { uint16_t type; uint16_t version; uint32_t len; };

static int read_records(const uint8_t *buf, size_t size, struct vm_state *st)
{
    size_t off = 0;
    while (off < size) {
        if (size - off < sizeof(struct rec_hdr)) return -EINVAL;
        struct rec_hdr h;
        memcpy(&h, buf + off, sizeof h);                    /* no unaligned pointer casts */
        off += sizeof h;
        if (h.len > size - off) return -EINVAL;             /* bound before use */
        switch (h.type) {
        case REC_VCPU:   if (parse_vcpu(buf + off, h.len, h.version, st)) return -EINVAL; break;
        case REC_MEMMAP: if (parse_memmap(buf + off, h.len, h.version, st)) return -EINVAL; break;
        case REC_DEVICE: if (parse_device(buf + off, h.len, h.version, st)) return -EINVAL; break;
        default:         break;                            /* newer writer: skip what we don't know */
        }
        off += h.len;
    }
    return validate_complete(st);                          /* every required record present */
}
```

Each `parse_*` accepts every version it knows and rejects unknown *required* versions. Add a checksum
over the whole buffer, and check it before the jump.

## 6. Fleet rollout

```
 build + sign ─► test fleet (every hardware generation) ─► canary: 1 host / AZ
     ─► 1% ─► 10% ─► per-AZ waves (never two AZs of a region at once) ─► 100%
              ▲                                 │
              └──── automatic halt + rollback ◄─┘ on: blackout p99 over budget, guest-visible
                                                   errors, kernel panics, restore failures
```

- **Version inventory:** know what every host runs; the rollout system queries it, and so do incident
  responders. (Bridge: signed OTA and the version inventory for 16,000+ field units.)
- **Rollback** uses the same mechanism (kexec back), which is why the format must read both ways.
- **Hosts that can't live-update** (an old generation, a guest that failed the dry run): migrate their
  guests away, or wait for the customer's own reboot/stop.

## 7. How you know it worked

- Per host: blackout duration (pause → last vCPU resumed), restore failures, time from staging to done.
- From guests: heartbeat gaps, steal-time spike, TCP retransmits, no reboot detected.
- Fleet: percentage updated, halts triggered, rollbacks, hosts stuck on an old version.
- Before shipping: a soak test that live-updates the same host back and forth (N → N+1 → N) hundreds of
  times with guests running memory checksums, disk I/O and network streams.

## Traps

| Trap | Correct |
|---|---|
| "Reboot the host, guests come back" | That's an eviction; the requirement is no guest reboot |
| "Live migrate everything" | Needs spare capacity for the whole fleet; fine for some hosts, not as the only tool |
| Forgetting passed-through devices keep doing DMA | Preserve the IOMMU tables or quiesce the devices |
| One-way state format | Rollback needs the old kernel to read the new format too |
| No dry run | Validate the handover before the jump; the blackout is the worst time to find a bug |

## Check you can explain these cold

- [ ] The four tools and which change each covers
- [ ] What survives a kexec live update and what is rebuilt
- [ ] Why the IOMMU tables matter
- [ ] How time stays continuous across the blackout
- [ ] The versioned record format and why it must read both ways
- [ ] The rollout waves and the halt signals
