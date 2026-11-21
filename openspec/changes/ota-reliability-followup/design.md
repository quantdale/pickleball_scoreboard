## Context

Court 1 runs `court-color-002` (ota_0); `ota_1` holds portrait `court-controller-002`.
Measured on 2026-11-21: network-ready heap 63.7–65.4 KB, authenticated session
heap 55.5–61.1 KB, login minimum about 39 KB, closed-settled 114.7–119.9 KB,
armed timer deletes all recovered. Device floors: 49,152 B (upload admission
and flash-begin), 32,768 B (emergency stop). CLI gate 60,456 B.

## Goals / Non-Goals

**Goals:** a session heap comfortably above the gate on every activation;
acknowledged closes; first-try Windows connectivity; evidence that repeated
sessions do not drift.

**Non-Goals:** lowering any floor or the gate without new measurements;
NimBLE migration; changing scoring behaviour or the approved layout.

## Decisions

To be made per task, with before/after measurements recorded in a report under
`docs/reports/`. Prefer bench hardware for anything that changes Wi-Fi buffers
or panel timing. Progress and evidence: `docs/reports/ota-reliability-followup.md`.

- **Headroom from runtime config only.** The Arduino SDK is prebuilt, so only
  `wifi_init_config_t`, `httpd_config_t` and controller fields can change. RX
  buffers 4/8 → 2/4 (one TCP window), CSI off, HTTP stack 7 KiB. Admission keeps
  the 8 KiB stack reserve, so no floor moves.
- **Find the join dip before sizing more.** It reached 25.6 KB, deeper than either
  recorded upload (20.8–20.9 KB). Milestones in every build; the bench budget
  build names the allocation that set the low.
- **Close waits for the client, not a timer.** `TCP_NODELAY`, acknowledgement
  before `Closed`, and Wi-Fi stays up until the reply's connection closes (≤5 s,
  one retransmission at the 3 s initial RTO). The CLI accepts a lost reply only
  with a link drop and BLE recovery.
- **The 60,456 B gate stays.** Its 24,616 B body figure is not reproduced by the
  recorded same-session pairs (20,800–20,904 B), but it changes only from new
  upload measurements.

## Risks / Trade-offs

- [Fewer Wi-Fi RX buffers slow uploads] → measure transfer time against the 120 s window.
- [Court 1 is the only measured board] → every write needs exact owner approval.
