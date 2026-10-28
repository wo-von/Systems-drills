# Design 6 — launching and tearing down a VM on a Nitro-style host (review sheet, not yet mocked)

**The question:** a customer calls `RunInstances`. Walk from the API call to a booted guest, then from
`TerminateInstances` back to a host whose resources are clean and reusable.

**The one-paragraph answer:** the control plane picks a host with capacity and sends the host agent a
launch request with the instance's resources. The agent reserves cores, memory on one NUMA node and
device functions (a NIC and an NVMe controller exposed by the offload card as **SR-IOV virtual
functions**), and programs the **IOMMU** so those devices can only DMA into this guest's memory. It
then starts a VMM: `KVM_CREATE_VM`, memslots for guest RAM, vCPUs with a fixed CPUID, firmware or a
kernel loaded, and one thread per vCPU running `KVM_RUN`. The guest boots, fetches its configuration
from the metadata service, and status checks pass. Teardown runs the same steps in reverse: stop the
vCPUs, quiesce the devices (function-level reset), **scrub memory**, release everything, and report.
Every step is idempotent and can be rolled back, so a failed launch never leaks capacity.

Note: "Nitro-style" = KVM-based thin hypervisor, with networking, EBS and local storage handled by
offload cards that give the guest PCIe devices (ENA NIC, NVMe). That much is public. Details beyond
it, say as "I'd expect", not as fact.

---

## 1. Open with questions

| Question | Assumption to state |
|---|---|
| Instance sizes? | 2 vCPU to a whole host; memory 4 GiB to TiBs |
| Launch time target? | Seconds to tens of seconds for a full VM (microVMs: ~125 ms, Firecracker's published figure) |
| Devices? | NIC and block from the offload card, as SR-IOV VFs passed through; console |
| Who can touch the host? | No operator shell on the host; everything through APIs |
| Failure | Any step can fail; capacity must never leak |

## 2. Launch, end to end

```
 customer ── RunInstances ──► control plane
                                │ placement: host with free cores/memory/VFs in the AZ
                                ▼
                       host agent (on the host)
                                │ 1 reserve: cores (pinned), memory on NUMA node N, VFs
                                │ 2 IOMMU: map guest RAM for each VF (DMA only into this guest)
                                │ 3 start VMM process (least privilege: seccomp, own user, jail)
                                ▼
                       VMM
                                │ KVM_CREATE_VM
                                │ mmap guest RAM (hugetlbfs, node N) → KVM_SET_USER_MEMORY_REGION
                                │ KVM_CREATE_VCPU × n; KVM_SET_CPUID2 (fixed CPU template)
                                │ load firmware (UEFI) or kernel + initrd at fixed GPAs
                                │ attach VFs (VFIO), virtual devices (console, RTC)
                                │ one thread per vCPU: loop on KVM_RUN
                                ▼
                       guest: firmware → bootloader → kernel → init
                                │ cloud-init: metadata service (169.254.169.254) → keys, user data
                                ▼
                       status checks pass ──► control plane: "running"
```

## 3. The vCPU loop (the core piece to write)

```c
for (;;) {
    if (ioctl(vcpu->fd, KVM_RUN, 0) < 0) {
        if (errno == EINTR) { if (vcpu->should_stop) break; continue; }   /* kicked by a signal */
        die_vcpu(vcpu, errno);
    }
    struct kvm_run *r = vcpu->run;                     /* mmap'd, shared with KVM */
    switch (r->exit_reason) {
    case KVM_EXIT_IO:                                  /* port I/O: console, PIT, ... */
        pio_dispatch(vcpu->vm, r->io.port, r->io.size, r->io.direction,
                     (uint8_t *)r + r->io.data_offset, r->io.count);
        break;
    case KVM_EXIT_MMIO:                                /* emulated MMIO devices */
        mmio_dispatch(vcpu->vm, r->mmio.phys_addr, r->mmio.data, r->mmio.len, r->mmio.is_write);
        break;
    case KVM_EXIT_HLT:          vcpu_wait_for_interrupt(vcpu); break;
    case KVM_EXIT_SHUTDOWN:     /* triple fault */
    case KVM_EXIT_SYSTEM_EVENT: request_vm_stop(vcpu->vm, r); return;
    case KVM_EXIT_FAIL_ENTRY:
    case KVM_EXIT_INTERNAL_ERROR:
    default:                    report_and_stop(vcpu, r); return;
    }
}
```

Most guest work never reaches this loop. Page faults are handled by the guest; EPT violations, the
local APIC and timers are handled inside KVM; the NIC and NVMe VFs are hardware. Exits here are
the slow path: legacy I/O, emulated devices, halt and shutdown.

## 4. Choices

| Box | Choice | Alternative and why not |
|---|---|---|
| Devices | SR-IOV VFs from an offload card + IOMMU | Emulated/virtio in the VMM: host CPU per I/O, bigger attack surface in the VMM |
| VMM | Small, one process per guest, seccomp, no shared state | One big daemon for all guests: one bug hits all tenants |
| Memory | hugetlbfs on the vCPUs' NUMA node, prefaulted | Lazy 4 KiB anon: slower, unpredictable latency |
| CPU model | Fixed CPUID template per instance type | Host passthrough: guests see features that disappear after migration or on other hosts |
| Boot | UEFI for general instances; direct kernel boot for microVMs | Firmware boot costs seconds; direct boot skips it but supports fewer OSes |
| Host access | No interactive shell; signed agent, APIs only | Operators with root on hosts are an insider risk |

## 5. Teardown, end to end

```
 TerminateInstances ──► control plane ──► host agent
   1 ask the guest to shut down (ACPI power button), wait N s; then stop the vCPUs hard
   2 stop the VMM; detach volumes after flushing (EBS-side state is the volume's, not ours)
   3 function-level reset (FLR) on each VF: wipes device state, queues, cached data
   4 remove IOMMU mappings; then and only then free guest memory
   5 SCRUB: zero all guest frames (before reuse, never skipped)
   6 release cores, memory, VFs to the host's inventory; report "terminated"
```

- **Order matters:** if memory is freed before the IOMMU mapping is removed and the device is reset, a
  device can still DMA into frames that now belong to someone else.
- **Scrub cost:** zeroing runs at roughly 10+ GB/s per socket, so 512 GiB is tens of seconds. Spread it
  over cores; or mark the memory dirty and zero it before the next allocation; never skip it.

## 6. Failure and rollback

- Each launch step records what it reserved; any failure undoes the steps in reverse (the teardown list).
- Steps are **idempotent**, so a retried request after an agent crash doesn't double-allocate.
- A reconciler compares the control plane's view with what the host actually runs and cleans leaks
  (an orphaned VMM, reserved memory with no instance).
- Guest never boots: the VMM reports the console log and a status-check failure; the instance is
  marked impaired; capacity is reclaimed on terminate.

## 7. Scale and defend

- Metrics: launch latency per step (placement, reserve, VMM start, firmware, kernel, first status
  check), failures by step, scrub time, leaked-resource count.
- What fails first at 10× launch rate: the host agent's serialization of steps, then memory
  zeroing bandwidth, then the control plane's placement.
- Test: launch/terminate in a loop for days with leak checks; inject a failure at each step and check
  rollback; the scrub test (write a pattern, terminate, launch on the same host, scan).
- Rollout of the agent/VMM: canary hosts, waves by AZ, automatic halt on launch-failure rate.

## Traps

| Trap | Correct |
|---|---|
| "The hypervisor emulates the NIC and disk" | On Nitro-style hosts they're card hardware passed through; the VMM handles little |
| Freeing memory before resetting devices | IOMMU unmap + FLR first, then free and scrub |
| Forgetting the scrub | Cross-tenant data leak |
| "The vCPU loop handles page faults" | Guest faults stay in the guest; EPT violations stay in KVM |
| No idempotency | Retries after crashes leak or double-allocate capacity |

## Check you can explain these cold

- [ ] The launch path in 8 steps, naming the KVM ioctls
- [ ] What the vCPU loop handles and what never reaches it
- [ ] Why SR-IOV + IOMMU, and what the IOMMU protects
- [ ] The teardown order and why it matters
- [ ] Scrub cost and where to hide it
- [ ] Rollback and the reconciler
