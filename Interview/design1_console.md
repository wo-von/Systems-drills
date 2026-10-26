# Design 1 — console and log channel from every guest (mock, Mon 26 Oct)

**The question:** hosts run a few to a few hundred guests. Customers and operators need what a guest
prints to its console (boot messages, panic output), even when the guest is wedged and has no network.
Operators read it after the fact. Design the path from guest to reader.

**The one-paragraph answer:** the guest kernel writes its console to an emulated 16550 UART at COM1
(port 0x3F8). Each write exits to the VMM, which appends the byte to a per-guest, size-capped log on
the host. Capture sits outside the guest, so it works on panic and on a wedged guest. A host agent
reads that log without locking against the VMM and serves it to the control plane. Customers reach
it only through the API. For bulk output, add virtio-console, which moves many bytes per exit, and
keep the UART for early boot and for panics.

---

## 1. Open with questions (3–5 min), then draw the path

Ask these, state an assumption for each, and write the numbers on the board:

| Question | Assumption to state |
|---|---|
| Who reads it? | Customers (their own guest only) and our operators |
| Live or after the fact? | Both: live tail, and the last N KB after a crash |
| How much is kept? | The **most recent** output: last 64 KB per guest for the API, more for operators |
| Scale? | Up to ~300 guests per host; a normal boot writes ~50 KB |
| Is the guest trusted? | No. It can write any bytes, at any rate, forever |
| Input too? | Interactive serial console is a follow-up, so design output first |

Then state the principle that drives everything:

> **Capture must sit outside the guest's failure domain.** Anything inside the guest (journald,
> a kernel buffer flushed later, a network agent) fails in exactly the cases we care about: panic,
> wedged guest, no network.

That rules out journald, guest-side buffering and Ethernet in one sentence.

## 2. One byte, end to end

1. **Guest config:** kernel command line `console=ttyS0,115200 earlycon=uart8250,io,0x3f8,115200`.
   `earlycon` gives output from the first instructions of boot, before the full 8250 driver loads.
2. **Guest writes 'A':** the 8250 console code polls the **LSR** (Line Status Register, port 0x3F8+5)
   until **THRE** (bit 5, 0x20: "transmit holding register empty") is set. Then it does
   `outb('A', 0x3F8)` to write the **THR** (transmit holding register, offset 0).
   That is **two exits per character**: an `inb` of LSR, then an `outb` to THR.
3. **The exit:** the hardware does a VM exit (VMX exit reason 30, I/O instruction). KVM has no
   in-kernel device at 0x3F8, so `KVM_RUN` returns to userspace with
   `run->exit_reason == KVM_EXIT_IO` and
   `run->io = { .direction = KVM_EXIT_IO_OUT, .size = 1, .port = 0x3f8, .count = 1 }`.
   The byte is at `(uint8_t *)run + run->io.data_offset`.
4. **VMM's UART model:** on a write to THR, append the byte to this guest's log. On a read of LSR,
   always return THRE|TEMT (0x60), so the guest never waits. Then call `KVM_RUN` again.
5. **Host:** the VMM `write()`s to a per-guest log file (or a shared-memory ring the agent reads,
   see §4). Once `write()` returns, the byte is in the host's page cache, so it **survives a VMM
   crash**. It does not survive a host crash unless you fsync or ship it off-host, which you can do
   in batches (every second, or on guest stop).

**The misconception to drop:** exits are how the byte reaches the VMM, not the log. Log the
**payload** (the bytes the guest wrote). Exit reasons are telemetry for us, never something to show
a customer.

## 3. The cost, with numbers

| Path | Cost per round trip |
|---|---|
| VM exit handled inside KVM (e.g. ioeventfd) | ~1 µs or less |
| Exit out to the VMM in userspace and back | a few µs (more with security mitigations) |

Where the time goes: the hardware saves guest state and loads host state, KVM's exit handler runs,
`KVM_RUN` returns to userspace, the VMM handles the exit, `KVM_RUN` is called again, and the
hardware does a VM entry.

- **Boot:** 50 KB × 2 exits × ~5 µs ≈ **0.5 s** of vCPU time. Fine.
- **Chatty guest at 10 MB/s:** 20 M exits/s × 5 µs = **100 s of exits per second**. Impossible. In
  practice the guest saturates its own vCPU at a few hundred KB/s.
- **Context:** a real UART at 115200 baud moves ~11.5 KB/s, so the emulated UART is already faster
  than real hardware. It is the right device for low-volume, must-not-lose output.

## 4. The host side: buffer, bounds, reader

**Keep the latest output.** Use a ring that **overwrites the oldest bytes**. The panic is the last
thing written, and that is what the reader needs. A ring that stops writing when full keeps the boot
messages from three weeks ago and drops the panic.

**Bounds, so a noisy guest can't hurt the other 299:**
- **Memory and disk:** a fixed size per guest (e.g. 64 KB ring for the API, a few MB on disk with
  rotation). The size comes from a per-guest budget, so it never grows with output.
- **CPU:** exits run on that guest's own vCPU thread, so a chatty guest mostly burns its own time.
  That is the right failure mode.
- **Disk bandwidth (shared):** rate-limit the bytes written per guest (token bucket). Drop the
  excess, count it, and write a marker such as `[12345 bytes dropped]`, so the reader knows output
  is missing.

**The reader must never block the writer.** If the agent holds a lock and stalls, the vCPU thread
waits on that lock and the guest freezes. Use either of these instead:
- **An append-only file:** the VMM appends, and the agent reads from its own offset. They share no
  lock, and the file survives a VMM crash.
- **An SPSC ring with indices that only grow (the one you built):** the writer publishes `head`
  with a release store; the reader loads it with acquire and keeps its own `tail`. If
  `head - tail > size`, the writer has overwritten unread bytes, so the reader skips to
  `head - size` and reports the gap.

## 5. The read path: customers never touch the host

```
customer ─▶ public API (authn, authz: is this their instance?)
         ─▶ control plane ─▶ agent on the host (management network)
         ─▶ reads the last 64 KB of that guest's log ─▶ back the same way
```

- Customers have no credentials on hosts and no access to them. A host runs other customers' guests.
- Guest output is **untrusted data**. It can contain escape sequences, invalid UTF-8 or arbitrary
  binary. Return it as opaque bytes (EC2's GetConsoleOutput returns base64), and let the viewer
  render it safely.
- Operators reading after a **host** crash need the logs shipped off-host, which is the batched
  shipping from §2.

## 6. Scaling up: virtio-console, and the UART stays

**The problem:** two exits per byte cannot carry bulk output.

**virtio-console** (a paravirtual device; the guest driver exposes `/dev/hvc0`, set with `console=hvc0`):
- The guest puts **many bytes per descriptor** into the transmit **virtqueue**, which lives in
  shared guest memory, then **kicks** (notifies the device) once by writing the notify register.
- With **ioeventfd**, KVM turns that kick into an eventfd signal **inside the kernel**, without an
  exit out to userspace. The VMM's I/O thread wakes, drains the queue, and marks the buffers used.
  Completions go back to the guest with **irqfd**.
- **Notification suppression** (`VIRTIO_RING_F_EVENT_IDX`): while the host is still draining, the
  guest doesn't kick at all. The cost goes from 2 exits per byte to about 1 cheap exit per batch.

**Keep the UART as well.** Configure both consoles (`console=ttyS0 console=hvc0`); printk writes to
every registered console.
- **Early boot:** the UART works from the first instruction with `earlycon`. virtio-console works
  only after PCI enumeration and driver probe.
- **Panic:** the UART is polled and has no driver state to go wrong, just one `outb` per character.
  virtio-console needs a working driver, intact virtqueues, and interrupts, any of which can be
  broken at panic time.

**Ethernet is the wrong answer here.** It needs the guest's network stack configured and working,
and usually a userspace agent to ship the logs. That breaks the requirement in the question.

**Interactive console (input):** the VMM puts a received byte in the UART's **RBR** (receive buffer
register), sets **DR** (data ready) in LSR, and injects **IRQ 4**. Real-world example: the EC2 Serial
Console.

---

## What you said and what to say instead

| My question | What you said | What to say |
|---|---|---|
| (opening) | "Log all the exit reasons in the VMM" | Questions from §1 first, then "capture outside the guest's failure domain" |
| A customer reads a log of exit reasons. Useful? | "Trim it to what the guest sent" | "Exits are the transport. I log the payload bytes, never the exits" |
| What device, at what address? | "systemd/journald, then emit with IO_OUT" | "An emulated 16550 at COM1, port 0x3F8, `console=ttyS0 earlycon`" |
| Same, second time | "Buffer in the guest, flush periodically" | "No buffering in the guest: a wedged or panicked guest never flushes" |
| One byte, end to end | outb to 0x38f (typo), KVM_EXIT_IO, read kvm_run | §2, including the LSR poll: 2 exits per character |
| Cost of one exit? | "A few ms", then "a few µs" after the arithmetic | "A few µs to userspace, ~1 µs in-kernel", then the steps in §3 |
| Where does the buffer live? | "RAM, dumped to persistent memory" | "Appended to a per-guest file on each write: page cache survives a VMM crash" |
| Noisy guest? | "Ring that stops when full" | "Ring that overwrites the oldest, plus a byte rate limit with a drop marker" |
| How does a customer read it? | "Their directory on the host, with credentials" | §5: API → control plane → host agent; never host access |
| Reader holds a lock? | "Separate reader and writer, read when the producer isn't writing" | "Lock-free: append-only file with reader offsets, or my SPSC ring" |
| 10 MB/s? | "Ethernet after boot, UART at boot" | "virtio-console with ioeventfd and batching; keep the UART for earlycon and panic" |

## Check you can explain these cold

- [ ] 16550 registers: THR, RBR, LSR (THRE, TEMT, DR), and COM1 = 0x3F8, IRQ 4
- [ ] Port I/O exit vs MMIO exit (`KVM_EXIT_IO` vs `KVM_EXIT_MMIO`), and the `kvm_run.io` fields
- [ ] Where the microseconds of an exit go, and in-kernel vs userspace handling
- [ ] ioeventfd (guest → host, no userspace exit) and irqfd (host → guest interrupt)
- [ ] Virtqueue: descriptor table, available ring, used ring; what a kick is; EVENT_IDX
- [ ] Why the UART stays: earlycon and the panic path
- [ ] Page cache vs fsync: which crash each one survives
- [ ] Overwrite-oldest vs drop-newest, and when each is right
- [ ] Control plane vs data plane: why customers never reach a host
