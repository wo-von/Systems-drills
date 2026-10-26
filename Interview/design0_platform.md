# Design 0: "the most complex system you've worked on" (Mon 26 Oct)

The answer: the live data path on the logger platform. Drawn in five minutes, then three follow-ups.
**Open item:** fill in the M ↔ A link details (§4) from the work repo.

## 1. The drawing: what to fix

The first sketch (`~/Downloads/51045.jpg`) had the Cortex-M ↔ Cortex-A (two arrows), the M → SD card →
A path, the A → peripherals, and the backend cloud (AOR, tunnel, email, live data). Before the loop:

- [ ] **Draw the data source.** Sensors on the left, with a count and an interface. "Where does the
      data come from?" is the first question.
- [ ] **Label every arrow** with what flows, over what link, at what rate (e.g. `UART, <baud>, <rate>
      frames`, `XMPP over LTE`). Say which M ↔ A arrow is live data and what goes back.
- [ ] **Mark your part:** the Cortex-M firmware, the A's kernel and bootloader, and the M ↔ A link.
- [ ] **AOR:** spell it out on the drawing, or be ready to explain it in one breath.

## 2. Say this (the five-minute version)

> "The logger records sensor data on a Cortex-M, which writes it to the SD card. That card is the
> source of truth: customers compute their statistics from the recording. Live data is a second path
> beside it. The M also streams each sample over a serial link to a Cortex-A running Linux, which
> forwards it over XMPP to our backend, so customers see it in real time.
>
> I built the Cortex-M firmware, the A's kernel and bootloader, and the link between the two
> processors. Live data runs on that substrate, and I contributed to its application too.
>
> The decision I'd defend: **live is a view, not a record.** Live data is best-effort and never
> buffered or replayed. That kept the A stateless for live data: no queue to bound, no backlog
> flooding a recovering link. What we gave up: a gap in the live view during an outage, which the
> customer fills from the recording later."

Say the drop decision as a principle. In the mock it came out as "unfortunately the live data is
lost", which sounds like a defect instead of a design choice.

## 3. Follow-ups from the mock

**SD card ownership.** Your answer: the M owns it and gates the A's access, taking it back whenever
it needs to write. That's the right shape. Have one sentence ready for each of these:
- [ ] **The handover:** a GPIO-driven mux, a message on the serial link, or both? Who asks, who
      grants, and what if the A doesn't release the card?
- [ ] **The Linux side:** if the A has the card mounted when the M takes it, the page cache and the
      FAT metadata are stale, giving wrong reads or corruption if the A writes. What did the A do:
      unmount before every handover, mount read-only, something else?

**Link outage (LTE down for 20 minutes).** Live data is dropped; the recording is unaffected. Use
the §2 phrasing.

**Your part.** Clear and honestly scoped. But the decision you defended (dropping live data) was a
team decision. The interviewer asked for one in **your** part, so use the link (§4).

## 4. The M ↔ A link: TO FILL from the work repo

In the mock: "a proprietary protocol with headers and a CRC; the receiver checks for corruption and
the message type (data, status, request); if the link drops it's not a problem, since everything is
recorded." That's correct but generic. "Not a problem, it's recorded" covers live **data** only, not
status messages or requests.

Fill in the blanks, then say it aloud in about three minutes:

> "Each frame is [start byte: ___] [length: ___ bytes] [type] [sequence number: yes/no, ___ bits]
> [payload, max ___ bytes] [CRC-___]. On a bad CRC the receiver drops the frame and resyncs by
> [___]. Types are data, status and requests. Data is [dropped when ___], status is [replaced by the
> next status / ___], and requests are retried after [___ ms] and are [idempotent / tagged with the
> sequence number], so a repeat is harmless. If the A falls behind, the M [drops / blocks / ___].
> A reboot shows up as [a hello frame / a boot counter in status / ___]. The decision I'd make
> again is [___]. It cost us [___]."

Facts to look up:

| Item | Value |
|---|---|
| UART baud rate, and DMA or interrupt on each side | |
| Frame layout and field sizes | |
| Start byte or escaping scheme (length field, COBS, SLIP, ...) | |
| CRC: which one (CRC-16-CCITT, CRC-32, ...) | |
| Resync after garbage: how | |
| Sequence numbers: yes/no, width | |
| Request timeout and retry count | |
| Are requests idempotent? | |
| What happens when the A is slow (M's TX buffer full) | |
| How each side detects the other rebooted | |
| Live data rate (samples/s, bytes/s) | |
| SD handover mechanism (mux, GPIO, message) | |
| How the A mounts the card | |

Candidate decisions to defend (pick whichever is true):
- **A length field vs delimiters with escaping** (COBS, SLIP): how a receiver recovers after garbage.
- **Custom protocol vs a standard one** (protobuf with nanopb, MAVLink-style framing): control and
  size, at the cost of tooling and onboarding.
- **Drop policy per message type:** live data dropped, status replaced, requests retried.
- **CRC-16 vs CRC-32:** frame overhead against undetected errors.
