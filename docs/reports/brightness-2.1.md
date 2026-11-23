# PickleScore 2.1: operator brightness

Release candidates that let the operator step the P10 display brightness from the
Android controller. Nothing was installed on Court 1. No maintenance session was
opened, and no panel, supply or wiring was touched. Court 1 still runs
`court-landscape-001` on `ota_0`.

## What the driver can actually do

The pinned driver (ESP32 HUB75 DMA 3.0.15) implements `setBrightness8` by
rewriting only the OE bits in the single existing DMA buffer. Clock, scan, colour
depth, refresh rate and pixels stay the same, nothing is allocated, and
repainting the score keeps the setting because `clearScreen` touches only colour
bits. Modelling `setBrightnessOE` for Court 1's setup (64-clock rows, latch
blanking 2, transition bit 0, 135 row transfers) gives:

- **Every value from 1 to 12 produces the identical minimum output** (a two-clock
  OE window). 12/255 is already the dimmest lit setting, so "dimmer than today"
  is not available without changing pixel values. Doing that would touch the
  deferred colour work, so it is out of scope.
- Above that, light rises in whole multiples of the default. The ladder picks one
  value inside each of eight plateaus:

| Level | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
|---|---|---|---|---|---|---|---|---|
| `setBrightness8` | 12 | 16 | 24 | 32 | 48 | 64 | 96 | 128 |
| × level 1 | 1.00 | 1.96 | 2.93 | 3.91 | 5.86 | 7.82 | 11.73 | 15.64 |

`scripts/check_brightness_levels.py` reproduces this table from the firmware
header and now runs in CI. The ladder stops at 128/255, half the driver's range
and far from the near-255 values the driver documents for ghosting.

## Safety limit

No supply rating or panel current has been documented or measured for Court 1.
The board therefore accepts only levels 1–4 (×3.9) by default, through the
build constant `PICKLE_BRIGHTNESS_MAX_LEVEL`. Firmware rejects anything above
it, whatever the phone sends. **This cap is a conservative choice, not an
electrical rating.** Spec 03 §2a defines the physical acceptance test. It
measures branch current, panel voltage and temperatures with `88 ◀◀ 88` at each
level, and compares the image with level 1 for flicker, ghosting and colour. A
failed level lowers the cap; raising it needs a new build after the test passes.

## Design

See [Spec 02 §§4–6](../specs/02-ble-protocol.md),
[Spec 03 §2a](../specs/03-display-rendering.md#2a-brightness) and the
[OpenSpec design](../../openspec/changes/adjustable-brightness/design.md).

- **Wire.** `B1`…`B<cap>` on the existing command characteristic, through the
  same 16-deep FIFO as scoring, so a brightness write never overtakes an earlier
  score. The board answers `B,<level>,<cap>` on the Manual State stream: after
  the state on every initial sync, and after each change. It is never sent on
  Legacy State.
- **No new GATT objects.** A separate Notify characteristic would cost an
  estimated ~1 KB of persistent heap and fill the 15-handle service exactly. On
  a board whose OTA headroom is the known blocker, that was the deciding factor.
  Android 2.0.0's exact parser already ignores a non-state payload, so old
  phones stay synchronized.
- **Independence.** Brightness is not game state (Spec 01 §2). A single pure
  dispatcher, `applyBoardCommand`, used by both the loop and the tests, routes
  `B` commands away from `GameState` and Undo.
- **Persistence.** One byte in NVS namespace `pksdisp` (separate from the
  maintenance credentials), written after 3 s unchanged, never during
  maintenance. It is restored before the boot frame. A missing or invalid record
  gives level 1, and a record above the cap gives the cap.
- **Android.** The Board menu holds a `−` / rising-bar meter / `+` row. The
  meter shows only the board-reported level. Rapid taps step from the newest
  unacknowledged request within the reported range. At most one brightness
  write waits, it yields to every scoring write, and a newer target replaces it.
  Without a report, the row reads "Needs board update". The scoring surface and
  its bounds are unchanged.

## Resources

Built with the same toolchain as `court-landscape-001`. The unchanged source
reproduced that release's exact sizes.

| Profile | Static DRAM before → after | Flash before → after |
|---|---|---|
| `esp32dev` | 45,580 → 45,652 B (+72, Preferences now linked) | 1,149,517 → 1,154,061 B |
| `ota`, `ota_b` | 68,476 → 68,484 B (+8) | 1,584,425 → 1,586,725 B |

- OTA image 1,591,008 → 1,593,296 B; slot headroom 440,608 → 438,320 B. The
  partition table, bootloader, deferred confirmation and rollback linkage are
  unchanged (`check_ota_build.py` against the commissioned baseline `14e77d5d…`).
- `GameState` is still 6,172 B, Undo is still allocation-free, and ATT ingress
  is still linked (`check_scoring_build.py`). OTA memory floors are untouched.
- Runtime heap: there are no new GATT objects or persistent allocations, and the
  NVS handle exists only during a load or save. **Not measured on hardware.**

## Verification

| Check | Result |
|---|---|
| Firmware Unity, PlatformIO native | 93/93 (83 existing + 10 new) |
| New firmware coverage | Exhaustive 1- and 2-byte command space (exactly 10 + cap accepted), malformed `B` writes, independence from score/serve/Undo/End/Reset, 20,000-step interleaved random walk against a model, save debounce incl. failure retry and `millis()` wrap, restore clamping, shared-FIFO order and reconnect |
| Display regression | 60 snapshots × 1,024 coordinates identical across the Python oracle and actual C++; glyph check pass |
| `esp32dev`, `ota`, `ota_b` builds | Pass, no warnings in project sources |
| Scoring build audit / OTA build audit | Pass / pass |
| OTA package tests / CLI host tests | 15/15 / 80/80 |
| Android JVM | 32/32 (23 existing + 9 new: reports, writes, stepping, coalescing queue) |
| Compose on API 35 emulator at 569×320, 914×411, 1280×800 dp | 10/10 each |
| Release lint | No unbaselined errors |
| 2.0.0 → 2.1.0 install over the existing app | Success, same certificate, cold launch, no crash |
| OpenSpec `adjustable-brightness --strict` | Valid |

The Compose tests cover the following: brightness exists only in the Board menu
and only while synchronized, Dimmer and Brighter respect both ends, taps never
move the meter locally, the meter pixels match 4/4 and 2/4, 48 dp targets, fit at
1.35× text on the smallest screen, the older-board message, and unchanged
scoring-control bounds. ATD screenshots needed `debug.hwui.drawing_enabled`; an
all-black capture was caught and the meter check now requires the actual teal
and unlit colours.

**Not verified:** a BLE radio session, the physical display, light output,
current, heat, NVS writes on the ESP32 and runtime heap. Those need hardware and
separate authorization.

## Artifacts

Session root:
`D:/Documents/tryPython/_AgentArtifacts/pickleball_scoreboard/sessions/20261121-claude-brightness/`
(`SHA256SUMS` lists every file). Built from `bf891f4`; rebuilding `ota` from that
commit reproduced the image byte-for-byte.

| File | SHA-256 |
|---|---|
| `apk/PickleScore-2.1.0.apk` | `c838197169b3cc10e4dd3a669bfdba50744f34b8ed6e7ca9eeb01710d556ff22` |
| `candidate/court-brightness-001.pksota` | `d7ddd4d66145661732d104e6746fbe557cb792c5f8c89536b1ce52795183d235` |
| `candidate/firmware.bin` (1,593,296 B) | `c8eb14371f7761ba73eaa9a575ec18652b0920cfb134801cfa4aaf78258f3b21` |

- APK: version code 8, `sensorLandscape`, not debuggable. Signed v2/v3 with the
  same local-distribution certificate as 2.0.0 (`548d4e27…ef06`), so it installs
  over 2.0.0 as an update.
- OTA package: signed with the Court 1 owner key and independently
  authenticated. The same public key authenticates the previously trusted
  `court-landscape-001` package, and its point is in the provisioning record read
  back from Court 1's NVS. It is a candidate only.
- Screenshots: `screens/<size>/brightness-menu.png`,
  `brightness-menu-font-135.png`, `brightness-older-board.png` and the unchanged
  landscape set.

## Court 1 (separate authorization required)

1. **Deployment route.** Court 1's `court-landscape-001` keeps the same 6,144 B
   Undo journal whose source heap blocked the last wireless update at
   `flash-begin` ([blocker report](court1-landscape-ota-blocker.md)). This
   candidate adds no persistent heap, but it cannot fix the *source* image. A
   wireless install from `court-landscape-001` will most likely be refused the
   same way, before any write. Choose the route (paced first bytes, USB with
   fresh isolation, or a headroom fix first) with the owner.
2. After install, check that the boot log reads `brightness=1/4`, that the
   controller shows four bars at level 1, and that `0 ◀ 0` is unchanged.
3. Run the Spec 03 §2a physical acceptance test before relying on levels 2–4.
