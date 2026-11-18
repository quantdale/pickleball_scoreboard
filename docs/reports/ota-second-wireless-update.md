# Court 1: second signed wireless OTA — 2026-11-18

**Upload, flash readback, install acknowledgement and UART trial/confirmation passed. Full CLI acceptance did not pass.** The one-time owner-authorized `ota_1 → ota_0` transaction issued one upload POST and one install POST. The bench monitor stopped after misclassifying the expected software reboot; no subsequent maintenance activation, USB recovery, rollback, or retry occurred. Do not run the deployment again to fill the evidence gap.

| Checkpoint | Evidence |
|---|---|
| Target/source | Fresh owner-confirmed isolated USB-only setup with panels discharged/disconnected and recovery physically accessible. Pinned BLE challenge, WPA2/CCMP BSSID and authenticated local HTTP reverified Court 1 (`3c:8a:1f:a1:ba:28`), `agent-life2-budget` / `ota_1` / `VALID`, `confirmed=true`, `healthy=true`, SHA-256 `0c57f2d912c128e8d67c80e3798e7bb9750085973f8ebbfe8af7aa9cc70d013c`. Verified before opening the previously identified CH340 at COM11/location 1-1; reverified source inside the deploy transaction. COM11 used only for receive, without DTR/RTS reset or TX. |
| Package/recovery | `court1-return-002` package SHA-256 `1d1c56ed20ea1f5f2a0946f27200a40e9ad006274db894f2d7bc3acef95310f4`; archived image SHA-256 `077d888af21625335b6f3790b6b179928f2c1852c0f42b3444e3c0688917a00e`. Both source and candidate independently verified with the provisioned key; both pre-update 4 MB copies rehashed to `181dcc684f706a7021cd7e39c31f2ccb226ae050b35056bbd4de8243a7a76b2f`. Saved partition table and bootloader match build artifacts; no new physical full-flash read. |
| Transfer/install | **1,591,184 image bytes**, **22.536 s** from upload POST to response. Device reported exact staged build and extent after independently reading the written image. Install POST acknowledged. The approved write replaced `agent-wpa2` in `ota_0`, payload `[0x10000,0x194790)`, sector extent `[0x10000,0x195000)`, plus normal OTA metadata changes `[0xE000,0x10000)`. No operation targeted `ota_1` or any other region. Its preservation was not independently raw-read after install. |
| UART boot | Install marker, exactly one ROM `rst:0xc (SW_CPU_RESET)`, then `court1-return-002` on `ota_0` with `pending=1`. `SCOREBOARD ready` logged `0,0,L,2,0,D`, followed by `OTA boot confirmed build=court1-return-002`. In this build the confirmation marker is emitted only after `esp_ota_mark_app_valid_cancel_rollback()` returned `ESP_OK`. No stack fault, panic, watchdog or brownout in the captured window. |
| Windows | Ethernet source-bound TLS passed before/during transfer and after restoring the original disconnected/radios-on WLAN state. Temporary Wi-Fi profile removed, COM11 released. |
| Not verified | CLI `deploymentSuccess` is false. No post-reboot authenticated HTTP observation of the exact running image digest or `VALID`/confirmed/healthy fields. UART logged BLE advertising, not BLE GATT initial sync or reconnection. Candidate maintenance reactivation and teardown watermarks were not measured. Panel illumination, rollback, and long-run stability are not accepted. |

## Measured source-side resource headroom

| Stage | Current free INTERNAL | Largest INTERNAL 8-bit block | HTTP free stack watermark |
|---|---:|---:|---:|
| Network start | 119,944 B | 47,092 B | — |
| After Wi-Fi start | 78,688 B | 47,092 B | — |
| Network ready | 62,816 B | 38,900 B | — |
| Authenticated/session admitted | 63,200 B | 38,900 B | — |
| Signature start / finish | 56,104 / 56,004 B | 25,588 B | 4,496 / 3,852 B |
| Flash begin (before / after) | 55,676 / 53,560 B | 25,588 B | — |
| SDK end / extent check complete | 63,096 B | 38,900 B | 3,852 B |
| Full readback complete | 63,096 B | 38,900 B | **3,680 B** |

All pre-upload gates passed. Flash admission had 6,524 B above its 49,152 B current-free gate and 21,492 B above the 4,096 B contiguous gate. The boot-wide minimum reached **42,280 B** (9,512 B above the 32,768 B emergency floor). Allocation-sampled minima were **44,124 B INTERNAL / 24,792 B INTERNAL8** during signature and **42,400 B INTERNAL / 23,068 B INTERNAL8** during the body. The smallest *checkpoint* contiguous block was **25,588 B**; none of these samples is a worst-case guarantee. The 3,680 B watermark is cumulative on the 8,192 B HTTP task, not an isolated readback frame estimate. Source loop during selection: 4,784 B free. Candidate boot confirmed at 67,136 B current INTERNAL, 66,772 B boot minimum. The source's settled close checkpoint was 120,332 B current INTERNAL, 47,092 B largest; the source later started the deploy window at 119,944 B. They are different lifecycle stages. No new same-stage endurance trend or timer ownership was established.

## Why the monitor stopped

The external one-shot guard wrongly paired `SW_CPU_RESET` with `0x3`. Pinned ESP32 ROM `rtc.h` defines `SW_RESET=3` and **`SW_CPU_RESET=12` (`0xc`)**. The first wireless update also logged `0xc`. Offline replay checked the exact candidate install → single expected reset → pending trial → confirmation ordering and verified that a pre-install/extra reset, wrong reason, missing confirmation or firmware fault is rejected. Four replay tests passed. No original runner output was altered; the correction is in the offline audit. The UART observation supports a confirmed trial, but cannot substitute for the independently authenticated post-boot digest and health check.

## One authorized verification-only follow-up

**BLE initial sync passed; authenticated HTTP and AP shutdown still unverified.** Windows passively discovered Court 1 at BLE address `3c:8a:1f:a1:ba:2a`, connected, received the initial Notify `0,0,L,2,0,D`, and disconnected without sending any scoring command. The device's maintenance challenge carried the pinned base MAC `3c:8a:1f:a1:ba:28`; the single authenticated proof received `OK`. The package was reverified offline. An `OK` queues activation after disconnect; it does not prove the SoftAP is ready.

Windows then timed out without establishing a Wi-Fi association. WLAN AutoConfig logged association start (11000) and connection failure (8002), **“The specific network is not available.”** No WPA session, DHCP lease, HTTP request or AP shutdown was observed. The client evidence cannot tell whether the AP failed to start or Windows attempted association before it became available. There was no reconnect or second activation. BLE recovery after AP close could not be checked because there was no verified close. Windows restored the original disconnected/radios-on state and removed the temporary profile; Ethernet source-bound TLS passed before and after. COM11 remained unopened to avoid any reset risk. No new heap or stack sample was available; the transfer measurements above are from the prior installation. No score commands, uploads, staging, installs, firmware/metadata writes, or reset commands occurred.

The repository now has a hardware-free reset classifier and four regression tests (`tools/ota_boot_evidence.py`, `tools/test_ota_boot_evidence.py`). The classifier accepts one install-correlated `0xc (SW_CPU_RESET)`, rejects the old `0x3` pairing and fault/extra-reset cases, and explicitly does not attest boot health. It passed against the original UART capture; all 38 host tooling tests passed.

**Smallest next check:** get fresh approval for one no-write maintenance attempt with timestamps for BLE disconnect, an exact-BSSID AP-ready observation before association, and (only with fresh electrical approval and a no-reset capture setup) passive device telemetry. This distinguishes AP startup from a host association-timing failure. If the AP opens, verify the exact `ota_0` digest and `VALID` health, close it, then reconnect BLE without a scoring command. Do not replay the installation. Production also requires separately authorized rollback on a matching spare and owner-observed illuminated dual-panel acceptance; see the [commissioning checklist](../commissioning-dual-p10.md).

Protected external evidence: `_AgentArtifacts/pickleball_scoreboard/sessions/20261118-pi-ota-second-return/` (`source/result.json`, `deploy/result.json`, `bench-result.json`, `serial-raw.log`, `serial-chunks.jsonl`, `independent-audit.json` and replay results). The follow-up is in `20261118-pi-ota-readonly-accept/` (`verification.json`, `verify/result.json`, `wlan-events.json`, `unit.log`, `replay.log`, `tracked-cli-replay.log`). Raw logs, recovery files and package binaries are not in Git.
