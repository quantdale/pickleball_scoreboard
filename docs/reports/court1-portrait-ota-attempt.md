# Court 1 portrait OTA attempt — 2026-11-19

**Stopped before activation or flash access. `portrait-cw-001` was not installed.**
The owner confirmed the isolated USB-only setup and explicitly authorized one
wireless deployment. The pinned deployment CLI was invoked once from portrait
baseline `3b1865f`. The authorization is consumed; do not retry on resume.

| Checkpoint | Result |
| --- | --- |
| Offline identity and recovery | Verified the signed `portrait-cw-001` and `court1-return-002` packages against the registered Court 1 trust. Rehashed both protected historical 4 MiB backups. The offline CLI dry-run and all 38 host CLI tests passed. |
| Intended transition | `court1-return-002 / ota_0` → `portrait-cw-001 / ota_1` on Court 1 (`3c:8a:1f:a1:ba:28`). The CLI reverified candidate package SHA-256 `1864b8a42767f8b70f06472a6c3723fe26e2625d037e477fac8d43c7f7104f81` and application SHA-256 `b039c3ca6e6e0d4a41077facb21802698c91de5f0e6b477d8ddf164ccadee5dd`. |
| First live gate | `ble_unavailable`: the Windows BLE discovery window did not find the pinned BLE address. This does not establish that a controller was connected or why the board was not discovered. The CLI stopped at `package-verified`, before GATT connect or maintenance proof. |
| Device actions | `wirelessTransferAttempted=false`, `deploymentSuccess=false`. No AP activation, WLAN join, device HTTP request, upload, install, reset, USB access, rollback, or rewiring. Neither slot nor OTA metadata was written by this attempt. |
| Host restoration | CLI reported `restored=true`, `unchanged=true`: it had not changed Windows Wi-Fi. Ethernet source-bound TLS passed before and after the attempt. |
| Not verified live | Running build/slot/hash/health and memory admission were not reached. No post-install verification or BLE recovery check was attempted. The last independently verified image remains `court1-return-002` / `ota_0` from the [prior report](ota-second-wireless-update.md#ap-readiness-gated-verification); it was not re-read in this attempt. |

Do not proceed to portrait mounting or physical acceptance on this result. Ask
for a separately authorized **no-write BLE discovery and sync check** first, with
scoring controllers disconnected voluntarily. Do not force a disconnection or
reset, and do not combine diagnosis with another deployment attempt. Any later
OTA requires new explicit authorization and a fresh live source check.

Raw evidence is outside Git under `20261119-pi-court1-portrait-preflight/`:
`cli-dry-run/result.json`, `artifact-verification.json`, `live-once/result.json`,
and `live-once-console.log`. No keys, credentials, or backups are in this report.
