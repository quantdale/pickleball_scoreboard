# Court 1 Undo memory correction — `court-undo-003`

**Signed candidate only.** The last authenticated Court 1 image remains
[`court-controller-002` on `ota_1`](court1-controller-002-install.md), confirmed
and `VALID`, with `doubles-serve-001` the expected fallback. The archived running
image/package were independently verified against Court 1 signing trust. This
session did not contact the board, enter maintenance, send scores, upload, write
metadata, open USB or change hardware. Android source and the accepted layout
are unchanged.

## Root cause and reproduction

The installed OTA profile applies a **106 KiB reserve** to
`heap_caps_get_free_size(INTERNAL | 8BIT)`. Its first inverse action must clear
`108,544 + 520 + 32 = 109,096 B` of current byte-addressable heap, as well as a
separate 520 B largest-block check. The reserve was derived from **all** internal
heap samples from OTA maintenance (`ESP.getFreeHeap()` uses `INTERNAL` alone),
including memory that cannot hold byte-addressable data. The test is run even
with Wi-Fi off and an empty journal. In the relevant archived samples, it
rejects before `new (std::nothrow)` runs; no allocation failure, journal leak,
or fragmentation is needed to produce the symptom.

Replayed the old production predicate and parser/serializer on the host using
real archived Court 1 inputs and the Xtensa block size. At a representative
closed-maintenance sample of **100,612 B** byte-addressable free with a
**47,092 B** contiguous block, the first rally did not allocate or score and sent
`0,0,L,2,0,D,2`. The Android parser maps bit 1 to “Board memory is low. Game
action was not recorded.” Even the archived **108,200 B** after BLE setup
failed with **106,484 B** contiguous. Isolating the checks: at 109,095 B with
room for the block the old predicate refused; at 109,096 B it allowed the
allocation. High total free with an injected 519 B largest block also refused,
for a different reason. The new journal recorded the first rally in all these
cases with zero journal allocations. No archived active-scoring heap trace
exists for `court-controller-002` itself; the replay establishes the faulty
code path and threshold, not the heap at the user's exact tap.

A separate integration defect could acknowledge a valid write and then discard
it when the FreeRTOS queue was full. The pinned Arduino BLE library responds
before `onWrite`. The firmware now takes ownership of the response for just the
scoring handle: success follows queue admission, and a full queue returns ATT
`0x11` (insufficient resources). Other GATT events still go through the Arduino
handler. Admitted work stays in order across disconnect. ATT success is still
transport only; if the next inverse record will not fit, the loop leaves the
score and prior history intact and reports the refusal on the existing extended
stream. The legacy stream and Android UI did not change.

## Memory budget

| Measure | Installed `court-controller-002` | Candidate `court-undo-003` |
| --- | ---: | ---: |
| Linked static DRAM (`.data` + `.bss`) | 62,188 B | 68,476 B (+6,288 B) |
| Linked scoring state | 36 B | 6,172 B (6,144 B journal) |
| History allocations in a session | 520 B object per block plus allocator metadata, when admitted | 0; same footprint empty or full |
| History bytes for a rally, side-out, Switch or End Game | 1 | 1 |
| History bytes per Reset | 9 | 3 when both scores fit in a byte, otherwise 9 |

Archived **hardware measurements** and a first-order estimate from the link
size delta (not a candidate measurement or bound on fragmentation):

| Court 1 observation | Actual archived free | Estimate subtracting 6,288 B |
| --- | ---: | ---: |
| Earlier connected score/Undo, `agent-wpa2`, total internal | 111,824–116,504 B | 105,536–110,216 B |
| Earlier `court1-return-002` pre-network, total / internal 8-bit | 119,944 / 100,612 B | 113,656 / 94,324 B |
| `court-controller-002` authenticated post-boot maintenance, total internal | 61,084 B | 54,796 B |

The earlier pre-network trace fell 57,128 B by AP-ready (including HTTP); it
went from 47,092 B to 38,900 B in largest internal byte block. The installed
controller's authenticated maintenance sample had a 38,900 B largest internal
block, 49,436 B historical minimum and 5,508 B HTTP stack watermark. Its OTA
source had a 3,916 B HTTP stack watermark after verified flash readback. These
are distinct metrics; a historical low-water mark is not current free heap.
The estimated 54,796 B leaves 5,644 B above the 49,152 B operation floor for
that sample, but cannot predict future low-water, heap layout, or RF load.

- HUB75 still allocates its single DMA framebuffer before BLE. At the pinned
  default depth it has 8,192 B of pixel words plus DMA descriptors and driver
  overhead. Panel geometry and rendering code are unchanged. The candidate
  logs before/after display heap for a later bench measurement.
- BLE-only startup still releases Classic RAM. The earlier Court 1 log measured
  +14,968 B for that release, then −86,224 B from release through controller
  and service setup. The command queue is now static (148 B linked storage,
  including its control object, instead of a boot-time allocation). The
  Arduino loop stack remains 8,192 B; OTA cleanup remains a separate static
  4,096 B stack. The journal is not on either stack.
- OTA keeps its exact pre-network (100,000 B, 8,192 B block), HTTP-start
  (57,344 B, 8,192 B block), ready/transfer (49,152 B, 4,096 B block) and
  emergency (32,768 B current internal) safeguards. They check the heap where
  networking and flash work actually execute. The journal never grows into
  their margin, and maintenance never drains or discards it. No thresholds
  were lowered. The previous OTA shutdown drift remains a separate open issue;
  removing journal allocation does not fix or certify Wi-Fi teardown.

## Capacity and regression checks

- Reversed **4,200 mixed actions per starting mode** (5,400 bytes, including
  600 Resets), with 55 simulated reconnects per sequence. Verified every prior
  score, serving end, server, mode, ended flag and opening-service flag.
- Exercised **6,144 one-byte actions** and complete Undo three times, **2,048
  same-position Resets**, exact-fit and one-byte-short cases for both Reset
  widths, maximum scores, full-queue rejection and FIFO across disconnect.
  No eviction. If space is exhausted, Undo can free it. Reset is itself
  undoable and does not clear the journal. A reboot clears the volatile session.
- Passed 109 firmware Unity tests via PlatformIO native and MSVC, 71 OTA CLI
  tests, 15 signing/package tests, 51 Android JVM tests plus debug build, and
  72 full C++ pixel snapshots. Six ESP32 profiles compiled, including the
  separately identified rollback fixture; partition/boot/link audit passed.
  The build audit checks the static OTA DRAM budget, journal size, inverse
  operations' lack of allocator calls and linked scoring ATT dispatch. The
  GitHub workflow runs these regression gates on PRs, including stacked PRs.

## Signed artifact and remaining acceptance

The OTA-profile candidate is in
`_AgentArtifacts/pickleball_scoreboard/sessions/20261120-pi-undo-memory/candidate/`
under `D:/Documents/tryPython/`. It uses the existing Court 1 signer and was
verified separately with the provisioned public trust.

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| `firmware.bin` | 1,591,344 | `fa35289481a36305e830553c8fd775112827ae54075b08350d05f5f8f4866820` |
| `court-undo-003.pksota` | 1,591,514 | `89599f5f3e6b26def4af5b124e0a18e18e21d2d857e969cec8fcae4f668839b2` |

Archived raw inputs: `20261118-pi-wpa2-install/serial-*.log`,
`20261118-pi-lifecycle-stability/serial-final-timeout.log`,
`20261118-pi-ota-second-return/serial-raw.log`, and
`20261120-pi-court1-authorized-ota/deploy-1/result.json`. Reproducer, red/green
output, build/test logs and signature evidence are in the current session.

Before courtside acceptance, separately authorize the exact package and slot
transaction, or qualify it first on a matching bench unit. On the candidate,
check cold/settled heap, byte-addressable free and largest blocks around display,
BLE connection, representative scoring with Undo, and AP/HTTP startup and
teardown with a populated journal. Exercise repeated maintenance and reconnects,
observe both panels, and verify upload/readback memory and stack margins in an
authorized OTA trial. If existing gates fail, preserve them and investigate the
measured stage rather than lowering floors. Host tests do not establish physical
BLE, LED or OTA behavior for this image.
