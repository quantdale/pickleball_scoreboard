# Court 1 final portrait correction — wireless deployment passed

**PASS, 2026-11-19.** Court 1 (`3c:8a:1f:a1:ba:28`) is running
`portrait-r180-002` on **`ota_0`**, with exact image SHA-256
`b8aa04446f1e8b24a99905e714e6635ca2b85f5ae1385e684626c57127e608e0`,
**VALID**, `confirmed=true`, `healthy=true`. Illuminated-panel orientation
and Android play acceptance were not part of this isolated deployment.

| Checkpoint | Observation |
| --- | --- |
| Authorization and preflight | Owner reconfirmed idle court, disconnected controller, discharged panels with HUB75 disconnected while unpowered, USB-only ESP32 power without external positive 5 V, and recovery access. The three signed packages and two protected historical recovery copies were reverified offline. All 38 CLI host tests passed. |
| Live source | One BLE maintenance proof accepted. Initial fresh pinned BSSID appeared 26.529 s after the BLE disconnect; one WPA2/CCMP association and on-link DHCP followed. After login `/api/info` confirmed `portrait-cw-001 / ota_1`, exact SHA-256 `b039c3ca6e6e0d4a41077facb21802698c91de5f0e6b477d8ddf164ccadee5dd`, `VALID`/confirmed/healthy. Current heap was 62,696 B after authentication and 62,112 B at idle status, above the 49,152 B admission floor. Firmware enforced its pre-erase memory gates; exact contiguous heap, minimum heap and stack watermarks were not captured. |
| Write and install | One upload POST and one install POST. Package SHA-256 `549b48945e7e453534881dd2bff04a19b0f7250118280df5335397a0289b224b`; upload response in 22.427 s. The board verified flash readback of all 1,590,768 application bytes before acknowledging install. The payload replaced the prior horizontal fallback in `[0x10000,0x1945F0)`; the sector erase range was `[0x10000,0x195000)`. Normal boot selection/confirmation uses OTA metadata in `[0xE000,0x10000)`; raw metadata was not read. `ota_1` was not targeted and remains the expected `portrait-cw-001` fallback. |
| Reboot and independent verification | To avoid the [prior Windows observation failure](court1-portrait-wireless-install.md), the host released the old WLAN profile after acknowledgment and joined once after a fresh pinned BSSID observation. The CLI observed the exact new image and then a separate authenticated `/api/info` read confirmed the same build, digest, slot and health. Uptime fell from 2,239,647 ms before install to 9,570 ms on the confirmed image; this is reboot evidence independent of Wi-Fi link transitions. Candidate idle heap was 64,620 B, no staged build. |
| Close and BLE | Close acknowledged; the target WLAN link dropped 10.425 s later before final restoration. BLE advertising returned, one scoring-service connection received `0,0,L,2,0,D` without a scoring write, and fresh advertising was observed again after normal disconnect. No independent RF shutdown test. |
| Host | Both temporary WLAN profiles removed, original disconnected/radio-on state restored, source-bound Ethernet TLS verified before, during and after. Deployment CLI and post-close BLE checks passed. |

No second upload or install, USB/serial access, forced reboot, recovery write,
rollback experiment, wiring change, or scoring/Android/OTA trust change. The
one-time deployment approval is spent. Keep both cleanly archived recovery
packages; physical acceptance is a separate owner-observed step, not another OTA.

Evidence outside Git: `20261119-pi-court1-r180-live/`, including
`cli-deploy/result.json`, `guard-result.json`, `console.log`, `source-network/`,
`candidate-network/`, and `cli-offline-tests.log`.
