# Friday 30 Oct, 10:00 — prep for Stanislav's round (x86 KVM)

Rules for Friday: no phone-screen story again (ModemManager, 85→98, SoM, lidar). He likes x86 KVM detail;
volunteer the Arm contrast once. Don't raise his patch; know the area in case it comes up.

## 1. A VM exit, end to end (VMX)

```
 KVM_RUN ioctl ─► vcpu_enter_guest ─► VMRESUME
                                        │ CPU loads guest state from the VMCS, runs the guest
 guest runs ... CPUID / HLT / I/O / EPT violation / external interrupt / XSETBV ...
                                        │ VM EXIT: CPU saves guest state into the VMCS,
                                        │ loads host state from the VMCS, jumps to KVM
 vmx_handle_exit(): read exit reason, qualification, guest-physical address, instruction length
   ├─ handled in the kernel (CPUID, MSR, EPT violation, APIC, HLT...) ─► re-enter (fast path)
   └─ needs userspace (port I/O, MMIO to an emulated device) ─► return from KVM_RUN,
        kvm_run->exit_reason = KVM_EXIT_IO / KVM_EXIT_MMIO ─► VMM emulates ─► KVM_RUN again
```

**VMCS areas:** guest-state, host-state, VM-execution controls (which events exit: I/O bitmap, MSR
bitmaps, CR masks, EPT pointer, VPID), exit controls, entry controls (event injection), exit information.

**Exits that always happen:** CPUID, XSETBV, INVD, GETSEC, VMX instructions in the guest. **Configurable:**
HLT, I/O, MSR access (bitmaps), CR access, external interrupts, the preemption timer.

**Cost:** a hardware round trip is on the order of a microsecond; a trip out to the userspace VMM is several
times that. Hence ioeventfd/irqfd, APICv/posted interrupts, and passthrough devices.

## 2. vCPU state (what migration, snapshot and live update move)

| State | ioctl |
|---|---|
| General registers | `KVM_GET/SET_REGS` |
| Segments, CR0/2/3/4/8, EFER, GDT/IDT | `KVM_GET/SET_SREGS` (`SREGS2` for PDPTRs) |
| MSRs (TSC, SYSENTER, STAR/LSTAR, PAT, kvmclock...) | `KVM_GET/SET_MSRS` |
| **FPU/SSE/AVX/AVX-512/AMX state** | `KVM_GET/SET_XSAVE` (4 KiB) / **`KVM_GET_XSAVE2`** (size from `KVM_CHECK_EXTENSION(KVM_CAP_XSAVE2)`) |
| XCR0 | `KVM_GET/SET_XCRS` |
| Local APIC | `KVM_GET/SET_LAPIC` |
| Pending exceptions, interrupts, NMIs, SMIs | `KVM_GET/SET_VCPU_EVENTS` |
| Debug registers | `KVM_GET/SET_DEBUGREGS` |
| Nested VMX/SVM state | `KVM_GET/SET_NESTED_STATE` |
| VM-wide: kvmclock, IRQ chip, PIT | `KVM_GET_CLOCK`, `KVM_GET_IRQCHIP`, `KVM_GET_PIT2` |

**Order on restore matters:** e.g. CPUID before XSAVE (CPUID decides which components exist), MSRs
before events.

## 3. XSAVE and guest FPU state (his area)

- **XCR0** (set with `XSETBV`, which always exits in a guest) enables user state components: 0 x87,
  1 SSE, 2 AVX, 5–7 AVX-512 (opmask, ZMM_Hi256, Hi16_ZMM), 9 PKRU, 17–18 AMX (TILECFG, TILEDATA, 8 KiB).
  **IA32_XSS** enables supervisor components (e.g. CET, PT), saved only by `XSAVES`.
- **The XSAVE area:** 512-byte legacy region (x87 + SSE) · 64-byte header (`XSTATE_BV` = which components hold
  non-init state; `XCOMP_BV` = compacted or not) · extended components.
- **Two layouts:**
  - **Standard (non-compacted)**, from `XSAVE`/`XSAVEOPT`: each component at a fixed offset given by
    **CPUID leaf 0xD** (sub-leaf *i*: size and offset). Holes for disabled components.
  - **Compacted**, from `XSAVEC`/`XSAVES` (`XCOMP_BV` bit 63 set): only enabled components, packed in order,
    optionally 64-byte aligned. Smaller; the kernel uses it.
- **Sizes:** CPUID.(0xD,0).EBX = size for the *currently enabled* XCR0; .ECX = size for *all supported*
  features. With AVX-512 ≈ 2.7 KiB; with AMX ≈ 11 KiB.
- **The user ABI is the standard layout; the kernel stores compacted.** Copying between them
  (`copy_xstate_to_uabi_buf`, `copy_uabi_to_xstate`) and sizing each buffer correctly is exactly where
  bugs live: the userspace buffer must be sized for the standard layout, the in-kernel one for the compacted
  layout of the enabled features. (That's the class of bug in his patch.)
- **AMX made state dynamic:** 8 KiB of tile data isn't allocated until a task asks
  (`arch_prctl(ARCH_REQ_XCOMP_PERM)`); **XFD** (extended feature disable) traps the first use. For guests:
  `ARCH_REQ_XCOMP_GUEST_PERM`, and `KVM_GET_XSAVE2` because 4 KiB (`struct kvm_xsave`) no longer fits.
- **KVM's FPU switch:** on entering the guest, KVM swaps the task's FPU state for the guest's
  (`fpu_swap_kvm_fpstate`), including XFD and PKRU handling; on returning to userspace it swaps back.
  The guest's state lives in a separate `fpstate`, sized for the guest's permitted features.
- **Arm contrast (volunteer once):** no XSAVE; FP/SIMD (V0–V31, FPCR/FPSR) and SVE (Z, P, FFR, size depends on
  the vector length) are saved explicitly; KVM/arm64 saves guest FP state lazily and traps first use with
  CPTR_EL2.

## 4. EPT

- **EPTP** in the VMCS points to a 4-level (or 5-level) table: GPA → HPA. Entries have R/W/X bits, a memory
  type, accessed/dirty bits (if enabled), and a 1 GiB / 2 MiB large-page bit.
- **EPT violation** (exit 48): the access isn't allowed or not mapped. Qualification says read/write/fetch and
  what the entry allowed; the faulting GPA is in the VMCS. KVM: memslot → HVA → host page → install the entry.
- **EPT misconfiguration** (exit 49): an entry with an illegal combination. KVM uses it on purpose for MMIO:
  emulated-MMIO GPAs get "poisoned" entries, so an access identifies itself as MMIO without a memslot search.
- **TLB:** **VPID** tags guest translations per vCPU (no flush on every exit); `INVEPT` invalidates
  GPA→HPA translations after KVM changes the EPT; `INVVPID` for guest-linear ones.
- **Cost:** a guest TLB miss is a 2-D walk, up to 24 memory references with 4 + 4 levels; huge EPT pages and
  guest huge pages cut it.
- **Dirty logging** (live migration): write-protect EPT entries, or EPT D-bits + **PML**.
- **Arm contrast:** stage-2 translation (IPA → PA), VTTBR_EL2, stage-2 faults → KVM; VMID tags the TLB.

## 5. Show what improved since 28 Sep

Say it once, briefly, if asked "anything you've worked on since we spoke": the timed C problem set (in C, with
your own `main` and sanitizers), the design sheets, and that you've practised building a compiling skeleton
first. Don't overclaim: you practised, you didn't ship.
