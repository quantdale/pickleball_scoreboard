# PickleScore 2.2: LED color, no Undo key, OTA headroom

> **Update (same day): Court 1 runs `court-color-002` on `ota_0`.** The owner
> approved installing it. Four wireless attempts stopped safely before any
> upload:
>
> - Twice the CLI headroom gate refused, at a session heap of 58,816 and
>   59,740 B against 60,456 B.
> - Twice Windows reused a cached `192.168.4.2` lease and HTTP never answered.
>
> The owner then chose a second Mode B USB write. A fresh NVS/otadata read
> showed only the saved display settings had changed. `verify_flash 0` matched
> both before (`a6ba3ce5…`) and after (`f4a81b32…`) writing the application
> region. Boot: `OTA build=court-color-002 … slot=ota_0 pending=0`, network-ready
> 65,428 B. Of two authenticated maintenance cycles, one passed end to end: close
> acknowledged, then BLE recovery with initial state. The other lost the close
> reply again, so the 500 ms flush helps (0/4 → 1/2) but does not fully fix it.
> **Wireless updates from 2.2 stay blocked by the conservative host gate.** The
> session heap measures 55.5–61.1 KB against 60.5 KB. The next headroom work
> (Wi-Fi RX buffers, HTTP stack) must land over USB or bench before OTA can be
> the routine path.

**Court 1 runs `court-color-001` on `ota_0`.** It was installed by the one
owner-authorized USB application write (Mode B isolation freshly confirmed). The
whole-device digest was verified on the chip before and after the write. BLE,
settings persistence and authenticated maintenance were verified on hardware.
The panels were disconnected, so **lit-panel acceptance is still the owner's**.
A wireless write has not been attempted; `court-color-002` is prepared for that
and needs separate approval.

## What changed

| Area | Change |
|---|---|
| Color | Six full-channel colors (White default, Red, Yellow, Green, Cyan, Blue) for every lit pixel. `K<code>` command, `K,<code>` report after `B,<level>,<cap>`. NVS-persisted with brightness after a 3 s settle, writing only the changed key. Spec 02 §§4–5a, Spec 03 §3. |
| Brightness | Unchanged ladder and uncertified cap of 4 (Spec 03 §2a). One cap covers every color: LED current scales with lit channels, so White is the worst case, and Red/Green/Blue draw about a third of it. Single colors look dimmer at the same level. |
| Android 2.2.0 (code 9) | Board-menu swatches showing only the reported color. A per-setting coalesced write slot, so color never replaces a pending brightness write. Preview drawn in the board color. **No Undo key**: the bottom row is Switch Court, End Game/Resume and Reset at equal width. |
| Resume | After End Game, Resume sends `U` once per ended report. A fast second tap used to queue a second `U` that silently undid a real point or serve change (found in review, now tested). |
| Activity | Declared config changes, so night mode, resize, font or locale changes no longer recreate the activity and drop BLE mid-game. Verified on the emulator: same Activity instance after `uimode night yes`. |
| Undo ring | 2,048 → **32** positions (6,144 → 96 B). Resume needs one, and 2.0.0 phones keep a short Undo. Spec 01 §6. |
| OTA firmware | (1) The `esp_timer_delete` wrapper stops and frees an armed timer that Wi-Fi teardown (`ets_timer_done`) would leak once per close. (2) A cancelled install now checks the revert to the running slot and confirms it, so a later reset is never a trial boot that falls back to the staged candidate. (3) A heap refusal at `flash-begin` returns 503, not 422, and status reports `inactiveWritten`. (4) Up to three consecutive 2 s receive timeouts are tolerated inside the transfer window. (5, `court-color-002`) The server stays up 500 ms after an acknowledged close, so the 200 reaches the client. |
| BLE | `AdvertisingGuard`: GAP start/stop outcomes are recorded, and the loop restarts advertising that failed or never confirmed within 2 s. |
| CLI | Refuses before upload unless the session heap is at least **60,456 B**: the measured demand above both device floors plus 3 KiB. Records `inactiveWritten` and the timer counters. No firmware floor was lowered. |

## Before / after

| Measure | `court-landscape-001` | 2.2 |
|---|---:|---:|
| OTA static DRAM (`.data`+`.bss`) | 68,476 B | 62,460 B |
| esp32dev static DRAM | 45,580 B | 39,612 B |
| OTA image | 1,591,008 B | 1,595,136 B (`-001`), 1,595,280 B (`-002`) |
| Slot headroom | 440,608 B | 436,336 B |
| **Network-ready heap (Court 1, boot)** | 59,456 B | **65,484 B** |
| Authenticated session heap (Court 1) | 55,044 B (422 at flash-begin) | 59,556–61,076 B over 3 sessions |
| Armed timer deletes leaked per close | 1 (counted, never freed) | 0 (`timerRecovered` = `timerArmedDeletes` = 7) |
| Closed, settled heap (Court 1) | 116,276 B (earlier sessions) | 119,884 → 118,472 B over 4 closes |

The settled heap was sampled with BLE connections and notifications in
progress. Four closes are too few to call it leak-free; the 50-cycle endurance
run still needs repeating on this image.

## Court 1 USB install (authorized once, consumed)

- **Identity:** COM11 CH340, ESP32-D0WD-V3 rev 3.1, MAC `3c:8a:1f:a1:ba:28`,
  4 MB flash (`0x85/0x2016`).
- **Backup:**
  - At 460,800 and 230,400 baud the CH340 link dropped bytes, so the backup was
    read in 256 KiB chunks at 115,200 baud, retrying failed chunks.
  - `verify_flash 0` against the assembled file: `digest matched`. SHA-256
    `e47148ef…4f2b633d`, identical to the expected image recorded after the
    `court-landscape-001` install, so nothing had changed since.
  - The backup contains private NVS: it stays in the session folder, never in
    Git.
- **Before:**
  - Partition table byte-identical to the build.
  - otadata sequence 9 points to `ota_0`, VALID.
  - `ota_0` = `court-landscape-001` (`922091fa…`).
  - `ota_1` = `court-controller-002` (`1707f73e…`).
  - The owner public key is present in the NVS trust record.
- **Write:** `write_flash --flash_mode keep --flash_freq keep --flash_size keep
  0x10000` of `court-color-001` (image `e01a8420…2981c`). Erased
  `[0x10000, 0x196000)`; esptool verified the data hash.
- **After:** `verify_flash 0` against the backup with only that region replaced
  (SHA-256 `0a1e3c74…94a15db`): `digest matched`. The bootloader, partition
  table, NVS and credentials, otadata, the `ota_1` fallback and coredump are
  byte-identical to before.
- **Boot log:**

  ```text
  OTA build=court-color-001 ABI=PKSCORE-OTA-ABI1 slot=ota_0 pending=0
  DISPLAY ready 64x16 landscape mount=1, DMA 64x16, chain=2, brightness=12/255 color=0xFFFF
  OTA AP ready ssid=PickleScore-Court1 address=192.168.4.1 window=180s heap=65484 min_heap=65364
  SCOREBOARD ready state=0,0,L,1,0,D heap=65484 undo_budget=96 brightness=1/4 color=W
  ```

  No panic, watchdog or brownout. The only error line is the expected
  `nvs_open NOT_FOUND` (no display settings saved yet).

## Hardware verification after install

| Check | Result |
|---|---|
| BLE initial sync | 7/7 connect/disconnect cycles: `0,0,L,1,0,D,0` then `B,1,4` then `K,W` |
| Settings writes | `B2` → `B,2,4`; `KR` → `K,R`; `KX`, `B9` and a repeated `KR` produced no report |
| Persistence | Serial reset: `brightness=16/255 color=0xF800`, reports `B,2,4`/`K,R`. Defaults (`B1`, `KW`) restored afterwards |
| Authenticated maintenance | 3 of 4 BLE-activated sessions joined Wi-Fi, logged in and verified the exact running image and health. The fourth's AP appeared after the CLI's 30 s scan deadline and was cancelled by the next activation, as designed |
| Close | `POST /api/close` reset or timed out in every session, so the CLI skipped its BLE check. The serial log shows the loop stopping the server right after the close request. `court-color-002` adds a 500 ms flush. Teardown itself was clean every time (`stop=ESP_OK deinit=ESP_OK`) |
| Windows | WLAN profile, radios and temporary addresses restored after every run |
| BLE after cycles | 2/2 initial syncs |

No scoring command was sent to the board.

## Tests

| Suite | Result |
|---|---|
| Firmware Unity (native) | 101/101: color command space, malformed input, independence, settle/save, restore, advertising guard, 32-position ring |
| Display snapshots vs actual C++ | 60/60; glyph check pass |
| `esp32dev`, `ota`, `ota_b` builds; scoring and OTA build audits | Pass (static DRAM limit lowered to 63,000 B) |
| Linked timer wrapper | `ets_timer_done` → `__wrap_esp_timer_delete` → `esp_timer_stop` + delete (disassembly) |
| OTA package / CLI host tests | 15/15 / 82/82 |
| Android JVM | 37/37 |
| Compose on API 35 at 569×320, 914×411, 1280×800 dp | 12/12 each |
| Lint | 0 errors |
| 2.0.0 → 2.2.0 in-place upgrade | Success, same certificate, cold launch, no crash |
| OpenSpec `display-color-and-ota-headroom --strict` | Valid |

## Artifacts

Session folder
`_AgentArtifacts/pickleball_scoreboard/sessions/20261121-claude-brightness/v22/`:

| File | SHA-256 |
|---|---|
| `apk/PickleScore-2.2.0.apk` (code 9, cert `548d4e27…ef06`) | `77728958da813c4edcfaabc34cdf672652296e2d311592bef4da54cd89c99564` |
| `candidate/court-color-001.pksota` (installed) | `1481173d071b17efbf3f90955d5b797b0940b73ffef7516484bd59b2914aa9d2` |
| `candidate/firmware.bin` | `e01a8420acde5f401b6c0a0c5c7480bab0d3a10d281d1e403dcb85de2322981c` |
| `candidate-002/court-color-002.pksota` (wireless test candidate) | `7a32a8c9c3d1f78218df5ecb68070f9843a9cafa577f92f597e2ef03b03f18e5` |
| `candidate-002/firmware.bin` | `a71823b76eaab7a017e13d47ce68662f994144b7d97c05aea4911f2969c8875a` |

Both packages are signed with the Court 1 owner key and authenticated offline.
Screenshots are in `screens/<size>/files/` (`color-menu-red`,
`color-menu-font-135`, `landscape-doubles` and others). Evidence is in
`usb-install/` and `ota-cycles/`.

## Open items and risks

1. **Wireless write acceptance (needs your approval).** Install
   `court-color-002` from `court-color-001`/`ota_0` into `ota_1`.
   - This replaces the `court-controller-002` fallback; `court-color-001` in
     `ota_0` becomes the fallback.
   - The measured session heap of 59.6–61.1 KB sits around the 60,456 B CLI
     gate. If a session falls below the gate, the CLI refuses before uploading;
     that is a clean stop, not a failure.
2. **Further headroom, bench-only.** Wi-Fi dynamic RX buffers 8 → 4 (body
   transient −4 to 6 KB [I]) and HUB75 colour depth 8 → 3 bits (about +16 KB).
   The latter changes accepted panel timing, so it needs owner re-acceptance.
3. **Repeat the 50-cycle endurance run** on this image to confirm the timer fix
   ends the earlier −69 B/cycle drift.
4. **Not changed, documented:**
   - Unlocked access to the Arduino BLE peer map during disconnect (a
     microsecond race; a crash has not been shown).
   - A failed notify is not retried (a stale phone view until the next change).
   - Execute Write from non-app centrals goes unanswered.
5. **Physical:** lit panels in each color, the `0 ◀ 0` witness, and the Spec 03
   §2a current, thermal and image test in White before relying on levels 2–4.
