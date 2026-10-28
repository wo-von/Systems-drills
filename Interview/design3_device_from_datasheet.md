# Design 3 — a device from its datasheet: 16550 UART driver and emulated device (review sheet, not yet mocked)

**The question:** here is the register map of a 16550-style UART (or a watchdog). Design the driver,
then the emulated device a VMM gives guests: registers, interrupts, errors, and the test plan. Name the
customers first.

**The one-paragraph answer:** customers are the guest kernel's driver (it must work unmodified), the
operators reading the console, the VMM team and the test engineers. The **driver** programs the divisor
and line format, enables the FIFO, then works interrupt-driven: the ISR reads IIR until nothing is
pending, drains RX while LSR.DR is set and refills TX up to 16 bytes when THR is empty. A spinlock
with interrupts disabled protects the TX buffer against the ISR. The **emulated device** is a struct of
register values in the VMM. Each guest access to ports 0x3F8–0x3FF exits to the VMM, which applies the
datasheet rules (DLAB aliasing, IIR priority, reads that clear state) and recomputes the IRQ line.
The guest controls every value, so bound the FIFOs and never block the vCPU thread on the backend.
Test it against the datasheet tables, with Linux's own 8250 probe and with a fuzzer.

---

## 1. Customers and requirements

| Customer | One need |
|---|---|
| Guest OS driver (Linux 8250, Windows, firmware) | Behaves exactly like real 16550 hardware, so it works unmodified |
| Operators / customers reading the console | No lost output during boot and panics |
| VMM team | Small, simple, auditable: it's guest-facing attack surface |
| Test engineers | Deterministic behaviour, testable without a guest |

## 2. The register map (COM1 at port 0x3F8, IRQ 4)

| Offset | Read | Write | Notes |
|---|---|---|---|
| 0 | RBR (RX byte) | THR (TX byte) | **DLL** (divisor low) when LCR.DLAB = 1 |
| 1 | IER | IER | **DLM** (divisor high) when DLAB = 1. Bits: 0 RX avail, 1 THR empty, 2 line status, 3 modem status |
| 2 | **IIR** | **FCR** | Same port, different register on read vs write |
| 3 | LCR | LCR | Bit 7 = DLAB; 0x03 = 8 data bits, no parity, 1 stop |
| 4 | MCR | MCR | Bit 3 OUT2 (gates the IRQ on PCs), bit 4 **loopback** |
| 5 | LSR | — | Bit 0 DR (data ready), 1 OE (overrun), 5 THRE, 6 TEMT |
| 6 | MSR | — | Modem lines |
| 7 | SCR | SCR | Scratch; drivers use it to probe that a UART exists |

**IIR priority** (bit 0 = 1 means nothing pending): line status `0b0110` > RX data / char timeout
`0b0100` / `0b1100` > THR empty `0b0010` > modem status `0b0000`. Reading IIR clears a pending THRE
interrupt; reading LSR clears line-status errors; reading RBR clears "RX data".

## 3. The driver

```
init:   IER = 0                              mask everything while configuring
        LCR = 0x80                           DLAB = 1
        DLL/DLM = 115200 / baud              divisor
        LCR = 0x03                           8N1, DLAB = 0
        FCR = 0xC7                           enable FIFO, clear RX/TX, RX trigger 14 bytes
        MCR = 0x0B                           DTR, RTS, OUT2 (IRQ enabled on the PC)
        IER = 0x01 (| 0x04)                  RX available (and line status)

write:  spin_lock_irqsave
        put bytes in the software TX ring
        if THR empty: push up to 16 bytes now
        IER |= THRE                           ask for an interrupt when it drains
        spin_unlock_irqrestore

ISR:    while (!(IIR & 1)) {                  until nothing is pending
            switch (IIR & 0x0E) {
            line status: read LSR, count OE/FE/PE errors
            RX / timeout: while (LSR & DR) push(RBR) to the RX ring (count drops if full)
            THRE:        write up to 16 bytes from the TX ring; ring empty → IER &= ~THRE
            modem:       read MSR
        }   }
```

Points an interviewer probes:
- **Why the loop in the ISR:** a second condition may be pending behind the first, and on an edge-triggered
  line you'd never get another interrupt for it.
- **The THRE trap:** reading IIR clears a THRE interrupt you didn't service. Enabling THRE while THR is
  already empty raises it immediately on a real 16550, and drivers rely on that to start transmission.
- **Lock:** the write path and the ISR share the TX ring and IER, so take the lock with interrupts
  disabled on that CPU.
- **Polled mode** for panic and earlycon: no interrupts, spin on LSR.THRE with a timeout.

## 4. The emulated device in the VMM

```
 guest:  out 0x3F8, al                                 VMM (vCPU thread)
   │                                                   ┌───────────────────────────────┐
   └─ PIO exit (KVM_EXIT_IO, port, size, dir, data) ──►│ uart_write(port - base, val)   │
                                                       │   DLAB? → divisor : THR        │
                                                       │   THR → tx_fifo → backend      │
                                                       │ recompute IIR, line level      │
                                                       │ irq level changed?             │
                                                       │   → KVM_IRQ_LINE(4, level)     │
                                                       └───────────────────────────────┘
 backend thread: pty / socket / log file ─► rx_fifo (16 bytes) ─► DR=1 ─► IRQ if IER.RX
```

```c
struct uart {
    uint8_t ier, iir, lcr, mcr, lsr, msr, scr, dll, dlm, fcr;
    uint8_t rx[16]; unsigned rx_head, rx_count;    /* bounded FIFO */
    bool    thre_pending;
    struct backend *out;                           /* never blocks the vCPU thread */
};
```

- **The line level** is a pure function of the state: `pending = (IER.RX && rx_count) || (IER.THRE &&
  thre_pending) || (IER.LS && lsr_errors)`, and gated by MCR.OUT2. Recompute it after every access and
  after the backend adds input. Call `KVM_IRQ_LINE` only when it changes.
- **Reads with side effects:** RBR pops the FIFO; IIR clears `thre_pending` if THRE was the reported
  cause; LSR clears OE.
- **Overrun:** backend input arriving with the FIFO full sets LSR.OE and drops the byte, as hardware does.
- **TX:** each byte is one exit, which is why bulk output belongs on virtio-console. If the backend
  is slow, buffer up to a cap and then drop with a counter. **Never block the vCPU thread** on a
  pipe or socket.
- **Loopback** (MCR bit 4): THR goes straight to RX. Linux's 8250 probe uses it, so emulate it.
- **Baud rate:** store the divisor and report it back; there's no real wire to time.

## 5. Errors and security

- Every register value comes from the guest: no value may index an array without a bound.
- Output rate: cap the per-guest log (ring buffer) and rate-limit, or a guest fills the host disk.
- The device state struct must be saved and restored on live migration: registers plus FIFO contents.
- Port range check: exits for 0x3F8–0x3FF only; anything else goes to the next device or reads 0xFF.

## 6. Test plan

| Level | What |
|---|---|
| Unit, no guest | Reset values from the datasheet table; DLAB aliasing on offsets 0/1; IIR priority with several conditions pending at once; each read-clears rule; overrun |
| Driver compatibility | Boot Linux with `console=ttyS0 earlycon=uart8250,io,0x3f8`; the 8250 probe (scratch + loopback) must detect a 16550A; Windows/firmware console |
| Stress | Flood RX from the backend while the guest is busy; flood TX with a slow backend; nothing blocks, drops are counted |
| Fuzz | Random sequences of port reads/writes; the VMM never crashes, and IIR/LSR stay consistent |
| Migration | Save/restore in the middle of a transfer; no lost or duplicated bytes |
| Reference | Compare register traces with QEMU's serial device or real hardware on a HIL bench |

## Watchdog variant (if they pick that one)

Registers: control (enable, action), timeout, **kick** (often a magic key to avoid accidental kicks),
status (expired, pre-timeout). Driver: start, kick from a periodic task, stop only if "nowayout" is
off. Emulation: a host `timerfd` per guest, re-armed on each kick; on expiry raise a pre-timeout
interrupt, then reset the VM or report it to the control plane. Test: no kick → reset within the
timeout ± tolerance; kicks under host load (vCPU descheduled) must not cause false resets, so count
time the vCPU was runnable, or give a margin. QEMU's i6300esb is the reference model.

## Check you can explain these cold

- [ ] Customers and one need each
- [ ] The register map: what's on offsets 0, 1, 2 under each condition
- [ ] IIR priority, and why the ISR loops
- [ ] The THRE trap
- [ ] How the emulated device decides the IRQ line level
- [ ] Why the vCPU thread must never block on the backend
- [ ] A six-row test plan
