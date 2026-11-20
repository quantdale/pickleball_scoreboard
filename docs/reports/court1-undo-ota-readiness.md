# Court 1 Undo OTA readiness — verification only

**Ready for a separately authorized OTA. No firmware was written.**
Court 1 authenticated as `court-controller-002` on `ota_1`, exact image
SHA-256 `1707f73e2de98ab92d69538ce01ffd7c6dc16e2e47a77f4c660df35856992ffd`,
`VALID`, confirmed and healthy. `doubles-serve-001` is the expected inactive
image; the verification-only API did not read `ota_0`.

## BLE finding and recovery

The previous OTA attempt did not see the pinned BLE address in its 15-second
active lookup. A separate active scan saw other devices but not Court 1. It
never connected or submitted a maintenance proof. In the follow-up, a native
Windows **passive** watcher received a connectable advertisement from the exact
pinned BLE address at **−66 dBm**. The Windows endpoint-enumeration query timed
out; it did not establish who had been connected earlier. BLE initial state
subscription and normal disconnect then passed with the regular client. The
reporting uptime during verification was about 94 minutes, spanning the initial
miss. No power cycle, adapter reset, USB access or scoring command was needed.

The installed firmware restarts advertising on normal central disconnect or
maintenance closure. The current Android disconnect path requests disconnect
and closes its GATT client, but this does not prove what the physical phone
was doing at the first miss. No evidence establishes that it retained a
connection, that advertising failed to resume, or that interference, stale
Windows state, or the BLE stack caused the miss. Both active and passive paths
worked later, so the passive observation is a useful recovery path, not proof
that active scanning was broken.

The CLI previously stopped on the first clean active miss without using an
alternative discovery mode. It now allows one eight-second passive fallback
per lookup for both activation and post-close BLE recovery, with an exact
address match. Existing pre-proof GATT retry and no-proof-replay behavior are
unchanged. Three added mocked regressions went red on the old CLI; all 74 CLI
tests pass with the fix. A separate real passive Bleak lookup also found the
pinned board. No firmware, signed artifact, Android layout or display change.

## Authenticated maintenance

One verification-only session used the pinned WPA2/CCMP AP and a temporary
static IPv4 address. Authenticated idle status passed existing admission:

| Metric | Actual | Check |
| --- | ---: | --- |
| Current free internal heap | 61,044 B | 11,892 B over 49,152 B |
| Largest contiguous internal 8-bit block | 36,852 B | 32,756 B over 4,096 B |
| Historical minimum internal heap | 47,140 B | Low-water mark, not the current free check |
| HTTP stack free watermark | 5,504 B | Status session, not an upload measurement |

Only `/api/session` and `/api/close` POSTs were sent. Close was acknowledged,
Windows observed the pinned link drop, the temporary IPv4 address and WLAN
profile were removed, radios restored, and Ethernet TLS verified. Pinned BLE
initial sync and disconnect passed after closure. No independent RF shutdown
measurement or candidate heap measurement is implied by this result.

## Next transaction — requires renewed authorization

Use the unchanged signed `court-undo-003` package, reverified against Court 1's
existing public trust. Recheck all live gates before the single write. Target
inactive `ota_0`, preserve `court-controller-002` on `ota_1`, and explicitly
approve the replacement of the expected `doubles-serve-001`, one reboot, and
loss of volatile scores/Undo:

- Package SHA-256: `89599f5f3e6b26def4af5b124e0a18e18e21d2d857e969cec8fcae4f668839b2`
- Image SHA-256: `fa35289481a36305e830553c8fd775112827ae54075b08350d05f5f8f4866820`
- Planned application write `[0x10000, 0x194830)`, sector erase
  `[0x10000, 0x195000)`, plus normal OTA metadata updates.

Raw evidence is in `20261120-pi-court1-ble-readiness/` in the project session
artifacts: `native-observation.json`, `before-maintenance-ble.json`,
`verification/result.json`, `verify-witness.json`,
`pending-candidate-verified.json`, `bleak-passive-compatibility.json`, and
red/green test logs. This verification did not stage, install or reboot.
