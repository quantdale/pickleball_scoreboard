# Court 1: `court-landscape-001` installed over USB

**Result: installed.** Court 1 runs `court-landscape-001` from `ota_0`. The
wireless route was refused for heap ([blocker report](court1-landscape-ota-blocker.md)).
The owner then chose an application-only USB install and freshly confirmed Mode B
isolation: panel supply off, HUB75 disconnected while unpowered, ESP32 on laptop
USB only with no external 5 V.

## Identity and backup

- **Chip:** COM11, ESP32-D0WD-V3 rev 3.1, MAC `3c:8a:1f:a1:ba:28`, 40 MHz
  crystal, 4 MB flash (JEDEC `0x162085`).
- **Full backup:** the 4 MB flash was read twice at 460,800 baud. Both reads gave
  SHA-256 `89e882fad399e1ddf5e166df829e3a95e24c7bca37c1f421cc8c95ae454949a6`.
  The backup holds private NVS: keep it outside Git and never publish it.
- **State before writing:**
  - Partition table unchanged: NVS `0x9000`, otadata `0xe000`, `ota_0` `0x10000`,
    `ota_1` `0x200000` (each `0x1f0000`), coredump `0x3f0000`.
  - `ota_0`: `court-undo-003`, image `fa352894…4f866820`.
  - `ota_1`: `court-controller-002`, image `1707f73e…56992ffd`, intact. The
    rejected wireless uploads never erased it.
  - otadata: sequence 9 → `ota_0`, state VALID.

## Write and verification

- **Write:** `write_flash --flash_mode keep --flash_freq keep --flash_size keep
  0x10000 firmware.bin`, 1,591,008 B (image SHA-256 `922091fa…55d6ba`, the
  payload of signed package `afe09108…2d46c5`). Sectors `[0x10000, 0x195000)`
  were erased; esptool verified the written data hash.
- **Whole-device check:** the expected image is the backup with the new
  application and an erased tail in `[0x10000, 0x195000)`, SHA-256
  `e47148ef…4f2b633d`. `verify_flash 0` against it passed on-chip:
  `verify OK (digest matched)`.
- **What that proves:** the bootloader, partition table, NVS and credentials,
  otadata, the `ota_1` fallback and coredump are byte-identical to before.
- **Read-back method:** host-side full read-backs after the write dropped bytes
  on the CH340 link. esptool rejected those blocks, so the on-chip digest
  comparison was used instead.

## Boot, display and BLE

```text
OTA build=court-landscape-001 ABI=PKSCORE-OTA-ABI1 slot=ota_0 pending=0
DISPLAY ready 64x16 landscape mount=1, DMA 64x16, chain=2, brightness=12/255
BLE advertising PickleScore; state owner=Arduino loop
OTA AP ready ssid=PickleScore-Court1 address=192.168.4.1 window=180s heap=59456 min_heap=59360
SCOREBOARD ready state=0,0,L,1,0,D heap=59456 undo_budget=6144
```

There was no panic, watchdog or brownout. A pinned BLE connection received the
initial Legacy State `0,0,L,1,0,D` and the Manual State `0,0,L,1,0,D,0`. No
scoring commands were sent.

## Not verified

- **Wi-Fi self-report:** the authenticated status check, in which the board
  reports its running digest and health over Wi-Fi, did not complete. The first
  run (Mode B USB activation) reached the AP, but Windows reused a cached
  `192.168.4.2` lease and port 80 never answered. The elevated retry's UAC prompt
  was cancelled.
- **Boot state:** VALID follows from the unchanged otadata entry and
  `pending=0`; it was not reported over HTTP.
- **Panels:** the panels were disconnected. Lit-panel acceptance, the physical
  landscape rotation (`0 ◀ 0` witness, operator guide) and gameplay remain with
  the owner.

## Follow-ups

- **OTA headroom:** AP-ready heap is 59,456 B, and the same 6,144 B Undo journal
  is present. Wireless updates *from* this image are expected to hit the
  `flash-begin` heap floor too. Fix the firmware headroom before relying on OTA.
- **Rollback:** `court-undo-003` is no longer on the device. It remains in the
  backup, at tag `portrait-1.3.1-court-undo-003` and in its signed package.
  Restoring it is a separate, owner-authorized USB operation.

Evidence: `_AgentArtifacts/pickleball_scoreboard/sessions/20261120-claude-landscape/usb-install/`.
