# Court 1: first signed wireless OTA update

**PASS — one controlled bench deployment on 2026-11-18. Not production acceptance.** The owner confirmed idle/USB-only isolation and explicitly authorized replacing the inactive original fallback with this exact package. Authorization was consumed by one BLE proof, one upload POST and one install POST. No recovery, forced rollback, USB reset/write or further activation was performed.

## Transfer and boot evidence

| Checkpoint | Physical observation |
|---|---|
| Target | Court 1, base MAC `3c:8a:1f:a1:ba:28`, pinned Wi-Fi BSSID, WPA2/CCMP and DHCP. Authenticated source `agent-wpa2`/`ota_0`, exact image SHA-256 `db32e3d046acdfb775797d0d504cb0074445d6bb6ead7171805feb91f0555dea`, healthy, `UNDEFINED` (not a confirmed trial). |
| Pre-write | Reverified owner signature and exact package SHA-256 `1ac03dc47d8e4abaa27382d9f4a978d628f541ff80be878fdcd1530cf39b19b8`; two hash-matched previously device-verified recovery copies; passively opened pinned COM11 with no TX. Live source identity/admission and admin authentication passed. Layout/metadata lower-sector details are from the saved verified backup, not a fresh raw read. |
| Upload | **1,592,416 payload bytes in 20.220 s**; device verified signed header, full received image and flash readback, and published exact staged build/size. Only then was install requested. |
| Selection and reboot | Install acknowledged; serial witnessed the install marker and one `SW_CPU_RESET`. New image booted `ota_1` with `pending=1`, then logged confirmation. |
| Confirmed image | Pinned `/api/info` after rejoining returned `agent-life2-budget`, `ota_1`, SHA-256 `0c57f2d912c128e8d67c80e3798e7bb9750085973f8ebbfe8af7aa9cc70d013c`, `bootState=valid`, `confirmed=true`, `healthy=true`. |
| Handoff | Authenticated new firmware, closed maintenance, observed BLE initial sync `0,0,L,2,0,D` without scoring commands, and released COM11. Ethernet source-bound TLS passed before, during, after reboot and after WLAN restoration. Temporary WLAN profile removed; original disconnected/radios-on state restored. Total run 63.186 s. |

The authorized sequential SDK write targeted inactive `ota_1` (`[0x200000,0x384C60)` payload; touched sectors through `0x385000` exclusive). It overwrote the original on-device fallback. The original remains in the protected backups, both SHA-256 `181dcc684f706a7021cd7e39c31f2ccb226ae050b35056bbd4de8243a7a76b2f`. The only other authorized flash changes were OTA selection/trial/confirmation metadata. No operation targeted `ota_0`, the bootloader, partition table or eFuses. No post-install raw flash comparison was run. Do not automatically restore the backup; any recovery or later deployment needs separate approval.

## Resource observations and open issues

| Source upload checkpoint | Current internal free | Largest internal 8-bit block |
|---|---:|---:|
|Authenticated, before upload|59,492|38,900|
|Before signature|52,044|17,396|
|Before flash begin|52,408|17,396|
|After flash begin|50,012|17,396|
|After complete readback|59,496|36,852|

The source boot-wide historical minimum fell **39,756 → 38,692 bytes** during the transaction (5,924 above the emergency floor). It is not an instantaneous per-stage minimum or a guaranteed reserve. The 49,152-byte current heap / 4,096-byte contiguous admission checks remained intact. No admission rejection, canary, watchdog, or unexpected reset was observed. The older source does not expose HTTP stack watermarks. Candidate **loop task** margin measured **4,784 bytes free** through boot validation/startup/confirmation; candidate **cleanup task** margin was **2,000 bytes free**. Neither measures upload/readback stack headroom. The historical 4 KB drift and production resource envelope remain open.

One type-4 Wi-Fi late-dispatch warning recurred despite `esp_wifi_stop`/AP_STOP/`esp_wifi_deinit` all succeeding. The raw lines interleaved, causing the runner's strict warning field to be empty; independent reconstruction preserves raw bytes and confirms one warning. The candidate observed **one armed-timer delete rejection** at handle `0x3ffef7d4`, return address `0x401057b8`, symbolized to the pinned `ets_timer_done`. This confirms that the previously hypothesized rejection path actually occurs. It does **not** prove that this same timer emitted the warning or account for the historical retained heap. The same-session status checks saw stale `total=1,589,824` on old firmware, and zero receive/total and empty staged build on the new firmware.

## Next action (not authorized)

The smallest next update is one separately approved signed return-slot update `ota_1 → ota_0` from the instrumented sender, which would capture actual upload/readback watermarks and overwrite `agent-wpa2`. Preserve and verify the current working image/recovery first. Test a deliberate pre-confirmation failure and rollback separately, preferably on a matching spare; no rollback test, lit-panel check or production endurance was performed here.

Protected evidence: `D:/Documents/tryPython/_AgentArtifacts/pickleball_scoreboard/sessions/20261118-pi-first-wireless-ota/`, especially `result.json`, `serial-raw.log`, `serial-chunks.jsonl`, `independent-verification.json` and `timer-rejection-caller.log`. The original runner result is unchanged. No firmware source changes were required for this attempt.
