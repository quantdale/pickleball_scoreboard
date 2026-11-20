# Court 1: `court-undo-003` installed wirelessly

On 2026-11-20, Court 1 (`3c:8a:1f:a1:ba:28`) completed one authorized signed
upload and one install request. It rebooted into **`court-undo-003` on `ota_0`,
`VALID`, confirmed and healthy**. A fresh authenticated session matched the
exact running image SHA-256:

`fa35289481a36305e830553c8fd775112827ae54075b08350d05f5f8f4866820`

The unchanged package verified against Court 1 signing trust. Package SHA-256:

`89599f5f3e6b26def4af5b124e0a18e18e21d2d857e969cec8fcae4f668839b2`

The one-shot guard rechecked the pinned device, live `court-controller-002`
source on `ota_1`, exact source digest, `VALID` health, and idle load admission
immediately before upload. `ota_0` was the verified inactive slot; planned
application write range was `[0x10000, 0x194830)`, sector erase range
`[0x10000, 0x195000)`, plus normal OTA metadata. The firmware hashed all
**1,591,344 bytes** read back from the inactive slot before accepting install.
No raw flash dump was taken. `ota_1` was not targeted and remains the expected
`court-controller-002` fallback. No rollback was attempted.

## Runtime measurements

| Authenticated sample | Current internal free | Minimum internal free | Largest internal 8-bit block | HTTP stack free watermark |
| --- | ---: | ---: | ---: | ---: |
| Source, independent pre-upload gate | 60,880 B | 43,144 B | 36,852 B | 5,504 B |
| Source, after full flash readback | 60,796 B | 36,264 B | 36,852 B | 3,980 B |
| `court-undo-003`, after reboot during maintenance | 56,036 B | 36,172 B | 30,708 B | 5,516 B |

Post-boot current free heap exceeded the unchanged 49,152 B operation floor by
6,884 B; the largest internal byte block exceeded the 4,096 B sector floor by
26,612 B. The minimum is a historical low-water mark, not the current-free
admission metric. These are maintenance measurements, not a candidate upload,
settled gameplay, or long-duration measurement. No thresholds changed.

## Recovery and handoff

An operator approved the normal Windows elevation prompt. The hardened workflow
used the pinned BSSID and a temporary static IPv4 address before and after
reboot. Maintenance close was acknowledged, Windows observed the pinned link
drop, and BLE initial state synchronization and normal disconnect passed. The
temporary IPv4 address and profile were removed, radio settings restored, and
Ethernet TLS passed before, during and after the update.

Court 1 is ready for the owner's physical gameplay and Undo test. No scoring
commands, physical panel test, additional reboot, USB access, panel changes or
Android changes were made. The one-write authorization is consumed.

Session evidence is in `20261120-pi-court1-undo-install-console/`, especially
`deployment/result.json`, `witness.json`, `evidence-audit.log` and
`uac-request.log`.
