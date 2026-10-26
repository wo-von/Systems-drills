# OS 1 (boot) and OS 2 (MMU and page tables): review sheet (Mon 26 Oct)

Two recorded three-minute answers, then follow-ups. Each topic has: the model answer to say aloud,
your answer side by side with corrections, the background, and the checklist.

---

# OS 2: address translation and page faults

**The question:** a user process on x86-64 runs `mov rax, [0x00007f1234567008]`. How does the CPU turn
the virtual address into a physical one? What happens if the page isn't mapped yet (the first touch of
a page from `malloc`)?

## Say this (about three minutes)

> "First the **TLB**. On a hit we have the physical frame and we're done. On a miss, the **hardware
> page walker** starts at **CR3**, which holds the physical address of the **PML4**. Bits **47–39**
> index it, at **8 bytes per entry**. That entry holds the physical address of the **PDPT**, indexed
> by bits 38–30. Then the **PD** by 29–21, then the **PT** by 20–12. The **PTE** gives the physical
> frame, and the low **12 bits** are the offset within it. At every level the walker checks
> **present, user/supervisor, read/write and NX**, and it sets **Accessed** (and **Dirty** on a
> write). If the **PS** bit is set in a PD entry, the walk stops there with a **2 MiB** page; in a
> PDPT entry, a **1 GiB** page. The result goes into the TLB.
>
> If an entry isn't present: **page fault**, vector 14. **CR2** holds the faulting address, and the
> **error code** says not-present, read, user mode. The kernel looks up the **VMA** covering that
> address. If there's none, or the permissions don't match: **SIGSEGV**. For an anonymous mapping on a
> **read**, Linux maps the shared **zero page** read-only. On the first **write**, it allocates a
> zeroed frame, installs the PTE (and any missing intermediate tables), and returns. The faulting
> **instruction restarts** and this time succeeds."

## What you said and what to say instead

| You said | Correction |
|---|---|
| (TLB not mentioned) | Start with the TLB. Most accesses never walk |
| "Use the first 8 bits as an offset" | **9 bits** per level: 512 entries × 8 bytes = one 4 KiB table |
| Three tables, "there it sees the physical address" | **Four** levels: PML4, PDPT, PD, PT. Then **frame + 12-bit offset** |
| "Reads the address and adds it with the bits" | Entry address = **table base + index × 8** |
| "The kernel sees it belongs to a malloc" | The kernel knows nothing about malloc. It finds the **VMA** that `brk`/`mmap` created |
| "The CPU walks the page tables again" | The handler returns with `iret` and the **instruction restarts**, so the walk happens again as a side effect |
| Five indices for one address | x86-64 with 4-level paging has **four** indices (five only with LA57, 57-bit addresses) |

## Background

**Splitting the address (48-bit virtual, 4 KiB pages)**

```
63      48 47      39 38      30 29      21 20      12 11         0
[ sign   ] [ PML4  ] [ PDPT   ] [  PD    ] [  PT    ] [  offset   ]
  16 bits    9 bits    9 bits     9 bits     9 bits      12 bits
```

- **Canonical addresses:** bits 63–48 must copy bit 47. User space is `0x0000_0000_0000_0000`–
  `0x0000_7fff_ffff_ffff`; the kernel is `0xffff_8000_0000_0000` and up. A non-canonical access
  raises **#GP**, not #PF.
- **On paper:** convert hex to binary nibble by nibble, then cut **from the right**: 12, then 9, 9, 9, 9.
  The offset is always the last **3 hex digits**.
- **Sanity checks:** every index is ≤ **511**, and the offset is ≤ **4095**. A bigger number means
  you put a boundary in the wrong place.

**Page-table entry (all levels share this layout)**

| Bit | Name | Meaning |
|---|---|---|
| 0 | P | present; if 0, the access faults and the other bits are free for the OS (e.g. swap location) |
| 1 | R/W | writable |
| 2 | U/S | user may access; if 0, kernel only |
| 5 | A | accessed: set by the hardware on any access |
| 6 | D | dirty: set by the hardware on a write (last level only) |
| 7 | PS | page size: in a PDE gives 2 MiB, in a PDPTE gives 1 GiB |
| 8 | G | global: not flushed on a CR3 switch (used for kernel pages) |
| 51–12 | address | physical frame of the next table, or of the page |
| 63 | NX | no execute |

**CR3** holds the physical address of the PML4, plus a **PCID** in its low 12 bits when CR4.PCIDE is
set. The tables hold **physical** addresses throughout. Otherwise the walk would need a translation
to do a translation.

**Cost of a TLB miss**

- Natively: **4** entry reads to translate, plus 1 for the data itself. The CPU also caches the upper
  levels (**paging-structure caches**), so a real walk often reads fewer.
- In a guest with **EPT**: every guest-physical address in the guest walk (gCR3 and the four guest
  entries) and the final data address must go through a 4-level EPT walk first.

```
                          EPT walk (4 reads)   then read at host PA
gCR3 → guest PML4 page    ■ ■ ■ ■              □ guest PML4E
       guest PDPT page    ■ ■ ■ ■              □ guest PDPTE
       guest PD page      ■ ■ ■ ■              □ guest PDE
       guest PT page      ■ ■ ■ ■              □ guest PTE
       data page          ■ ■ ■ ■              ◆ the data
```

  5 × 5 = 25 accesses, of which **24 translate** (20 EPT + 4 guest) and 1 is the data:
  (4+1)(4+1) − 1. Say it as "**24 references to translate one address, against 4 natively.**"
- **No software runs during either walk.** The hardware walker does both dimensions. Software steps
  in only on a failure: a guest page fault goes to the **guest kernel**, and an **EPT violation**
  exits to **KVM**.
- Huge pages in the guest *or* the EPT remove a row or a column from that grid. That's why VMs care
  so much about them.

**The TLB**

- It is per core and caches virtual page → physical frame plus permissions.
- A CR3 write flushes the non-global entries, unless **PCID** tags them. `invlpg` drops one page.
- **TLB shootdown:** when a mapping is removed while other cores might cache it, the kernel sends
  them an **IPI** to invalidate it. This is expensive and shows up in munmap-heavy workloads.
- In VMs, **VPID** tags entries per vCPU, so a VM entry or exit needn't flush.

**Page faults on Linux**

- `exc_page_fault` → `handle_mm_fault`: find the VMA and check its permissions, then:
  - **anonymous, read:** map the shared zero page read-only
  - **anonymous, write:** allocate a zeroed frame (or copy-on-write from the zero page or after `fork`)
  - **file-backed:** take the page from the page cache, reading it from disk if it isn't there
- **Minor fault:** no disk I/O. **Major fault:** had to read from disk.
- **`malloc`:** glibc takes small requests from its heap (`brk`) or arenas, and large ones
  (≥ 128 KiB by default) straight from `mmap`. Either way the kernel only creates or extends a VMA;
  frames arrive one fault at a time. That's **lazy allocation**, and it's why overcommit is possible.

**ARM64, in one breath:** two base registers, **TTBR0_EL1** (user half) and **TTBR1_EL1** (kernel
half). With a 4 KiB granule and 48-bit addresses it also uses 4 levels of 9 bits. **ASID** is the
equivalent of PCID. Stage-2 translation is ARM's version of EPT.

## Drills (answers at the bottom)

Give the four indices and the offset, in decimal:

1. `0x0000555555554abc` (a typical PIE binary address)
2. `0x00007fffffffe3a8` (near the top of the user stack)
3. `0xffffffff81000000` (kernel text; why is the PML4 index what it is?)

---

# OS 1: from power button to PID 1

**The question:** power on an x86 server. What runs, in what order, in what CPU mode, and what does
each stage hand to the next, up to the first userspace process?

## Say this (about three minutes)

> "At reset the CPU is in **16-bit real mode**, fetching from **0xFFFFFFF0**, 16 bytes below 4 GiB,
> where the **flash** is mapped. **UEFI** runs **SEC** and **PEI**, using **cache-as-RAM** because
> there's no RAM until **memory training** is done. **DXE** loads the firmware's drivers, enumerates
> **PCI**, and brings up a console, by now in **long mode**. **BDS** picks the boot device and loads
> the bootloader. **GRUB** loads the **bzImage** and the **initramfs**, and fills in **boot_params**:
> the **memory map**, the **command line**, and the initrd location. Then **ExitBootServices**, after
> which the kernel owns the machine.
>
> The kernel **decompresses itself**, and **startup_64** sets up its own page tables, GDT and IDT.
> **start_kernel** builds the memory allocators from the memory map, then sets up traps, interrupts,
> timers and the scheduler. The other cores (**APs**) are woken with **INIT-SIPI-SIPI**; they start
> in real mode at a trampoline and climb the same ladder. Drivers probe through the **initcalls**.
> The initramfs is unpacked, and its **/init** runs as **PID 1**. It mounts the real root,
> **switch_root**s to it, and **exec**s systemd, which stays PID 1."

## What you said and what to say instead

| You said | Correction |
|---|---|
| Reset vector "just below the flash" | At the **top of the flash mapping**: 0xFFFFFFF0, 16 bytes below 4 GiB |
| (no mention of how firmware runs before RAM) | **Cache-as-RAM** until memory training is done |
| "All stages other than the first are in real 64-bit mode" | **Real mode is 16-bit only.** 64-bit is **long mode**. UEFI switches to it early |
| "The bootloader unzips the kernel" | bzImage **decompresses itself**. GRUB loads it and the initramfs |
| Boot args are "the address of the console etc." | The **memory map** first, then the command line (`console=` lives there) and the initrd location |
| (stopped at "starts the kernel") | The kernel's own stages up to PID 1 are where this round probes most |
| VMM follow-up: "skips the bootloader" | It skips the **firmware too**. There's no RAM training: guest RAM is host `mmap` |
| "The kernel will not notice the difference" | It's not fooled: the VMM meets the documented **64-bit boot protocol** |
| "Peripherals are set up by the VMM" | The kernel probes its own drivers. The VMM **tells it where devices are** |

## Background

**The x86 mode ladder**

| Mode | Width | Addressing | Paging |
|---|---|---|---|
| Real mode | 16-bit | segment × 16 + offset, 1 MiB | none |
| Protected mode | 32-bit | segments through the GDT | optional |
| Long mode | 64-bit | flat, canonical 48-bit | **required** (CR3, 4 levels) |

Getting to long mode: enable protected mode (CR0.PE), enable PAE (CR4.PAE), load CR3 with page
tables, set **EFER.LME**, then enable paging (CR0.PG). The CPU is then in long mode, and a far jump
to a 64-bit code segment starts executing 64-bit code.

**The reset vector:** after reset, CS has selector 0xF000 but a hidden **base of 0xFFFF0000**, and
IP = 0xFFF0. So the first fetch is at 0xFFFFFFF0 even though the CPU is in real mode. Firmware
typically puts a jump there.

**UEFI phases**

| Phase | Does | Runs from |
|---|---|---|
| SEC | first code; sets up cache-as-RAM | flash, no RAM |
| PEI | trains memory (often Intel FSP or AMD's equivalent) | cache-as-RAM, then RAM |
| DXE | firmware drivers: PCI enumeration, storage, console, network | RAM, long mode |
| BDS | picks the boot device and loads the bootloader (an EFI application) | RAM |
| ExitBootServices | the OS takes over; only UEFI runtime services remain | |

Before any of this, on modern parts a management controller (Intel ME, AMD PSP) has already run, and
microcode gets loaded. Legacy BIOS skipped all these phases and stayed in real mode; the bootloader
did the mode switches.

**Bootloader to kernel handoff (`boot_params`, the "zero page")**

- **The memory map** (e820 on legacy, converted from the UEFI map otherwise): which physical ranges
  are usable RAM, reserved, or ACPI. It's the most important item, since the kernel doesn't know
  where RAM is without it.
- **The command line:** `console=ttyS0`, `root=/dev/…`, `init=`, and so on.
- **The initrd** address and size.
- Framebuffer info, the ACPI RSDP address and EFI system table pointers.

Alternatively the kernel boots directly as an EFI application (the **EFI stub**), with no GRUB.

**Inside the kernel**

1. **The decompressor** (`arch/x86/boot/compressed`) unpacks the kernel, then jumps to
   **`startup_64`** (`arch/x86/kernel/head_64.S`): early page tables, GDT, IDT.
2. **`start_kernel()`** (`init/main.c`):
   - `setup_arch`: parses the memory map into **memblock**, the early allocator
   - traps and interrupts, `sched_init`, timers
   - memory init hands free memory to the **buddy allocator**; the slab allocator comes up on top
3. **`rest_init`** starts the `kernel_init` thread (which becomes PID 1) and `kthreadd` (PID 2).
4. **`kernel_init`:**
   - `smp_init` brings up the APs with **INIT-SIPI-SIPI**. The SIPI vector points to a **trampoline
     page below 1 MiB**, because an AP starts in real mode, like the boot CPU did.
   - `do_initcalls`: drivers register and probe.
   - The initramfs is unpacked into a RAM filesystem as `/`.
   - `run_init_process("/init")`: **execve** turns the kernel thread into the first userspace
     process, **PID 1**.
5. **The initramfs `/init`** loads the drivers needed to reach the root disk, mounts it,
   **switch_root**s (deletes the initramfs contents and makes the new root `/`), and **exec**s
   `/sbin/init` (systemd). Because it's an `exec`, PID 1 stays PID 1.

**In a VM: direct kernel boot (Firecracker style)**

The VMM skips the firmware and the bootloader, and meets the **64-bit boot protocol** itself
(`Documentation/arch/x86/boot.rst`):

1. Guest RAM is host memory (`mmap`, then `KVM_SET_USER_MEMORY_REGION`). There's no training.
2. Load the kernel (`vmlinux`, an ELF) and the initramfs into guest memory.
3. Write `boot_params`: the **e820 map**, a pointer to the command line, the initrd address and size.
4. Set up a GDT, **identity-mapped page tables**, and CR0, CR4 and EFER for long mode
   (`KVM_SET_SREGS`).
5. Set **RSI = the address of boot_params** and **RIP = the 64-bit entry point** (`KVM_SET_REGS`).
   Set CPUID (`KVM_SET_CPUID2`) and MSRs.
6. Describe the devices. Firecracker has **no PCI**: virtio-mmio devices go on the command line
   (`virtio_mmio.device=…`), the serial port is at 0x3F8, and **MP tables** describe the vCPUs for
   SMP. The guest wakes its APs with INIT-SIPI-SIPI through KVM's emulated local APIC.

**So what:** skipping the firmware is a large part of a ~100–150 ms boot. The cost: no UEFI
services, no PCI hot-plug, and only kernels that support this boot path.

---

# Check you can explain these cold

**OS 2**
- [ ] The 9 + 9 + 9 + 9 + 12 split, and why 9 (512 × 8 bytes = 4 KiB)
- [ ] Splitting an address on paper, with the ≤ 511 and ≤ 4095 checks
- [ ] What CR3 holds; why the tables hold physical addresses
- [ ] The PTE bits: P, R/W, U/S, A, D, PS, G, NX
- [ ] 2 MiB and 1 GiB pages: where the walk stops, how big the offset becomes
- [ ] TLB, PCID, `invlpg`, shootdown by IPI; VPID in VMs
- [ ] The fault path: CR2, the error code, VMA lookup, zero page, CoW, instruction restart
- [ ] Why the kernel never hears about `malloc`; lazy allocation
- [ ] The EPT walk: 4 → 24, who does it (hardware), when software steps in
- [ ] Canonical addresses: #GP, not #PF

**OS 1**
- [ ] The reset vector and why real mode can fetch at 0xFFFFFFF0
- [ ] Real / protected / long mode, and the steps into long mode
- [ ] Cache-as-RAM, and the UEFI phases SEC → PEI → DXE → BDS
- [ ] What's in `boot_params`, with the memory map first
- [ ] Self-decompressing bzImage, `startup_64`, `start_kernel`
- [ ] AP bring-up with INIT-SIPI-SIPI and the real-mode trampoline
- [ ] initramfs `/init` → switch_root → exec systemd, and why PID 1 stays PID 1
- [ ] What a VMM does for direct kernel boot, and what it gives up

---

## Drill answers

| Address | PML4 | PDPT | PD | PT | Offset |
|---|---|---|---|---|---|
| `0x00007f1234567008` (mock) | 254 | 72 | 418 | 359 | 8 |
| `0x00007ffde4a3c010` (redo) | 255 | 503 | 293 | 60 | 16 |
| `0x00007f0000401abc` (redo) | 254 | 0 | 2 | 1 | 2748 |
| `0x0000555555554abc` | 170 | 341 | 170 | 340 | 2748 |
| `0x00007fffffffe3a8` | 255 | 511 | 511 | 510 | 936 |
| `0xffffffff81000000` | 511 | 510 | 8 | 0 | 0 |

The kernel's text sits in the **last** PML4 slot (511) and the second-to-last PDPT slot: it is linked
into the top 2 GiB of the address space, which lets the kernel use 32-bit sign-extended addressing
for its code (`-mcmodel=kernel`).
