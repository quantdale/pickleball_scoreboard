# PickleScore local browser OTA

## Read first

This feature is implemented on `feat/browser-softap-ota`, based on the preserved working `5ae28e2` baseline (production source `3bf53dd`). **No OTA firmware, partition table or credentials have been written to the owner's board.** No eFuse, flash erase, bootloader replacement, merge or push is performed by the software workflow.

The owner has confirmed the existing production display works. That is baseline evidence, not OTA acceptance. **Do not seal the enclosure until two consecutive wireless updates and recovery tests below pass.** Review [Spec04](specs/04-local-browser-ota.md), [verification/size report](reports/browser-softap-ota-verification.md), and the existing [physical scoreboard checklist](commissioning-dual-p10.md).

## Behavior

- Normal scoring initializes unchanged: two horizontal32×16 panels,64×16 pixels, brightness12/255, left-serving0-0-2. Scoring/serving rules and the Android app are unchanged.
- A provisioned board offers its own WPA2 network, normally `PickleScore-Court1`, for180 seconds after boot. Open **http://192.168.4.1** directly; no router, internet, captive portal, DNS redirection or cloud.
- Use the separate maintenance key to claim a session (maximum10 minutes, including transfers). A file transfer has a120-second absolute limit and2-second socket idle timeout. Requests do not renew deadlines. Five failed login attempts close maintenance for that boot.
- A scoring BLE connection/activity closes the AP for that boot. Stop/disable the scoring controller before intentionally power-cycling for maintenance. Session admission stops BLE advertising and waits for its acknowledgement/quiet period; it refuses a connected controller or pending command. BLE is not deinitialized/recreated. The ordinary loop always processes accepted scoring commands; unexpected BLE activity cancels maintenance.
- The last score remains displayed; no expensive continuous redraw during transfer. Timeout/Exit closes maintenance and restores advertising without resetting state. An explicitly confirmed installation **does reboot and resets the RAM-only score to0-0-2**. No match persistence is implemented.
- Driver/netif/HTTP allocations are released on exit and the Wi-Fi radio stops. IDF4.4 does not support deinitializing lwIP itself: its TCP/IP task/semaphores and firmware static storage remain. There is no promise of zero retained RAM after first use.
- Unprovisioned/invalid credentials disable the AP securely; ordinary BLE remains available. Allocation/layout/startup failures disable OTA and are logged. A pending updated app failing its startup checks rolls back rather than confirming a broken maintenance stack.

## Build without touching hardware

From repository root, Git Bash:

```bash
PYTHONIOENCODING=utf-8 firmware/.venv/Scripts/pio.exe run -d firmware -e ota -e ota_b
firmware/.venv/Scripts/python.exe scripts/run_firmware_host_tests.py --output-dir <new-external-session>/host
firmware/.venv/Scripts/python.exe scripts/test_ota_tool.py --output-dir <new-external-session>/signing-tests
firmware/.venv/Scripts/python.exe scripts/check_glyphs.py
firmware/.venv/Scripts/python.exe scripts/check_display_snapshots.py --cpp-tool <new-external-session>/host/render-snapshots.exe
```

`ota` embeds `ota-v1-a`; `ota_b` embeds `ota-v1-b`. Both contain the real production scoreboard plus the same updater. These IDs appear on the page and serial log; compare IDs **and selected slot** to prove actual updates. For a later release, change the build marker deliberately and preserve the signed release package/manifest. Original `esp32dev` remains a non-OTA profile with its original partition layout; do not use it for wireless updating. Always select an explicit environment.

OpenSSL3 or compatible1.1.1 CLI is required only on the offline packaging computer, not the ESP32/browser. This host has OpenSSL3.2.4 in Git Bash. Python uses only standard-library modules except existing pyserial for the separately invoked physical provisioning command. Production uses only the installed ESP-IDF/Arduino libraries—no new networking framework.

## Generate private credentials and signing trust

Do this locally before commissioning, with an **owner-selected private directory outside Git**. The command refuses an existing directory instead of overwriting keys:

```bash
firmware/.venv/Scripts/python.exe scripts/ota_tool.py provision-files --directory <PRIVATE-OWNER-DIRECTORY> --ssid PickleScore-Court1
```

Outputs:

- `owner-credentials.json`: unique32-character WPA2 PSK and independent64-hex-character maintenance key (256 bits of entropy).
- `signing-private.pem`: randomly generated NIST P-256 signing key. **Only the release computer needs this file. Never upload it or put it on the scoreboard.**
- `signing-public.pem`: public verifier.
- `provisioning.bin`:164-byte record containing SSID, AP PSK, SHA256 of the maintenance key, and the65-byte public point. The private key/raw maintenance key are NOT in the record.

No default password or trusted private key is built into firmware. Restrict filesystem access (Windows ACLs on the private folder, or0700/0600 on POSIX), back up the owner files securely, and exclude them from shared screenshots/logs/Git/cloud sync. Private files are unencrypted at rest: use the owner's encrypted storage/backup policy. Do not reuse the disposable test keys under session `tool-tests*` as production trust.

Court2 can use the same commands with its own private directory and SSID; it should have independent credentials and ideally independent signing trust.

## Sign and verify browser update packages

Upload **`.pksota` packages**, not raw `.bin` or merged/full-flash images. Sign with the key corresponding to the record provisioned on that board:

```bash
firmware/.venv/Scripts/python.exe scripts/ota_tool.py pack --image firmware/.pio/build/ota_b/firmware.bin --key <PRIVATE-OWNER-DIRECTORY>/signing-private.pem --build ota-v1-b --output <external-release-directory>/PickleScore-ota-v1-b.pksota
firmware/.venv/Scripts/python.exe scripts/ota_tool.py verify --package <external-release-directory>/PickleScore-ota-v1-b.pksota --public-key <PRIVATE-OWNER-DIRECTORY>/signing-public.pem --work-dir <external-session>/signature-check

# Prepare A as the second wireless update too:
firmware/.venv/Scripts/python.exe scripts/ota_tool.py pack --image firmware/.pio/build/ota/firmware.bin --key <PRIVATE-OWNER-DIRECTORY>/signing-private.pem --build ota-v1-a --output <external-release-directory>/PickleScore-ota-v1-a.pksota
```

The tool checks actual ESP32 image structure, checksum, appended hash, bounds, compatibility marker and compiled build ID. The signature covers target/build/size and complete-image SHA256. The device verifies that signature using its provisioned public key before allocating/writing the inactive application, then verifies received digest, SDK image/chip compatibility, exact verified image extent and full flash-readback hash. **SHA256 alone is not authentication.** No code path writes the running app, bootloader or partition table via HTTP.

## Partition migration and initial USB installation — OWNER AUTHORIZATION REQUIRED

The old application slots are0x140000 bytes each; the OTA-enabled image does not fit. This is a real migration, not an ordinary app-only update. The new CSV is `firmware/partitions_ota.csv`:

| Partition | Address | Size | Purpose |
|---|---:|---:|---|
| NVS |0x9000|0x5000|Existing NVS plus one transactional credential blob |
| OTA metadata |0xe000|0x2000|Two independently maintained metadata sectors |
| ota_0 |0x10000|0x1f0000 (2,031,616 bytes)|Application0 |
| ota_1 |0x200000|0x1f0000 (2,031,616 bytes)|Application1 |
| Coredump |0x3f0000|0x10000|Existing reserved crash partition |

This occupies exactly4MB. There is no filesystem. Migration changes old app1/filesystem interpretation; **it is not power-fail atomic**. Keep USB accessible and stable power until first installation/provisioning is verified. NVS and coredump offsets are unchanged. A complete original flash backup is essential.

Before **any** serial reset, identification, backup or write, obtain a fresh explicit confirmation: panel supply disconnected/discharged; HUB75 disconnected while all power was off; ESP32 USB-only; external5V disconnected. Also obtain explicit approval to write the new partition table, reset OTA metadata and install/seed application slots. Earlier isolation confirmations are stale after panel operation. COM11 is historical metadata, not proof of current identity/isolation.

After those approvals, the commissioning operator/agent must:

1. Enumerate USB; identify actual ESP32 revision/MAC/4MB flash and inspect security state. Known historical MAC is `3c:8a:1f:a1:ba:28`. Stop if chip/flash/security configuration differs. No eFuse burning or encryption/secure-boot changes are part of this feature.
2. Read **all0x400000 bytes** into a new session `pre-ota-full-flash.bin` at a conservative115200 baud; hash it and verify the read. Retain the existing bootloader/partitions/boot_app0/working application recovery set and Git baseline. Do not confuse previous partial/application-only backups with full flash.
3. Verify the current bootloader bytes against the inspected known-good image (allow documented header normalization only if independently understood). The generated OTA bootloader matches the preserved baseline exactly; **do not replace the bootloader**. If it differs, stop for a new rollback audit instead of assuming it is compatible.
4. Prepare public artifacts with `scripts/check_ota_build.py --output-dir <new-external-session>/deliverables --baseline-dir <preserved-commissioned-baseline-directory>`. It verifies layouts, size bounds, linked delayed-confirmation override and actual rollback instructions. It does not open serial.
5. Only after the explicit migration approval, write these ranges in one controlled esptool operation at115200: **0x8000 new partitions.bin;0xe000 initial-otadata.bin (standard boot_app0.bin);0x10000 OTA-A firmware.bin;0x200000 preserved working baseline firmware.bin**. This deliberately seeds a bootable non-OTA fallback and resets selection to slot0. **Do not use generic PlatformIO upload for this first migration**: it also writes a bootloader and does not seed the fallback. No `erase_flash` is needed or permitted implicitly. Record hashes/addresses and independently verify each image.
6. First USB-installed boot has initialized/undefined metadata, **not a pending trial**. It must be supervised and verified while USB is still accessible. Before credentials exist it intentionally has no AP. If startup fails, restore via USB; do not claim automatic rollback protects this first migration. The seeded baseline is a readable-image fallback, not a permanently reserved rescue slot; the first wireless update will overwrite it while preserving the now-running OTA-A.
7. Provision the164-byte record over USB within180 seconds of boot, with no scoring controller connected. The command below explicitly resets the board, sends no secrets to logs, waits for acknowledgement and closes the port:

```bash
# PHYSICAL OPERATION: only after fresh confirmation and approved installation above
firmware/.venv/Scripts/python.exe scripts/ota_tool.py provision-serial --record <PRIVATE-OWNER-DIRECTORY>/provisioning.bin --port <freshly-verified-port> --confirm-isolated-usb-only
```

8. Require `OTA PROVISIONED; reboot manually to open maintenance AP`. The record is saved atomically as one NVS blob. Then reboot under the same verified isolated setup, capture boot/build/slot/heap and `OTA AP ready` diagnostics. If allocation/layout/radio initialization fails, investigate while USB remains available; do not seal or treat compilation as proof.
9. Close serial, unplug USB, and reconnect HUB75/panel power only while everything is unpowered. Keep existing regulated5V panel/common-ground arrangement and ESP32 USB power. Brightness remains12/255. Never reconnect live or apply upstream12V to panels/ESP32.

For expert manual execution, the range-write template (NOT run by any software verification tool) is:

```text
esptool.py --chip esp32 --port <verified-port> --baud 115200 write_flash
  0x8000 <deliverables>/ota/partitions.bin
  0xe000 <deliverables>/initial-otadata.bin
  0x10000 <deliverables>/ota/firmware.bin
  0x200000 <deliverables>/baseline-usb-recovery/firmware.bin
```

Verify security state/bootloader and backup first. Do not execute placeholders blindly. The updater never performs this migration itself.

## Two consecutive wireless updates — owner acceptance

Use a non-match bench state; installing will reset scores. Keep the scoring controller disconnected for maintenance.

1. Power on OTA-A. Connect the laptop to its unique WPA2 SSID; accept “no internet/stay connected” if needed. Open **http://192.168.4.1**, not a search engine. Record device ID, `ota-v1-a`, slot`ota_0` and free heap.
2. Sign in with the separate maintenance key. Verify normal BLE control cannot simultaneously claim the device. The page should state the last score is preserved and BLE advertising paused.
3. Select signed OTA-B. Upload progress is transfer progress; the next message distinguishes image/readback validation. Success means **staged only**; no reboot/new slot yet. Confirm staged build`ota-v1-b`.
4. Click Install; cancel once to prove cancellation has no boot effect. Click again and explicitly confirm that the score may reset. Device reboots only after valid selection. Reconnect AP/refresh; require`ota-v1-b`, slot`ota_1`, normal boot state and successful boot-confirmation log if serial is safely available.
5. Upload signed OTA-A and explicitly install again **without USB**. Reconnect/refresh; require`ota-v1-a`, slot`ota_0`. This is the second distinct update. File upload success alone is insufficient.
6. Independently reconnect the Android BLE remote and verify both panels, scores/serve arrows,0-0-2, rallies, undo, switch courts, end/freeze, reset sides/modes and reconnect state retention. Existing nonstandard doubles full side-out→server2 is unchanged. The minor accepted border issue is not reopened.
7. Demonstrate no-login180-second AP timeout and10-minute authenticated limit, BLE-priority cancellation, normal scoring after AP closes, and reasonable command bursts. Observe at least20 minutes of normal operation after Wi-Fi shutdown and under controlled transfer load. Capture free/minimum heap and watchdog/reset diagnostics; do not substitute static RAM figures for runtime evidence.

### Safe failure exercises

- Submit wrong login, missing/stale bearer/nonce, wrong origin/path, wrong signing key, invalid/plain baseline.bin, truncated/mutated/wrong-target/oversized packages: reject without install/reboot/current-slot overwrite. Use disposable bench releases; never re-sign malicious firmware for the real owner key.
- Interrupt a transfer by disconnecting the laptop before completion (not by altering live HUB75/power wiring). The current build stays selected. Reconnect within the absolute session deadline and retry with a fresh status/nonce, or let maintenance expire and start a new safe boot window. A staged-but-not-installed image is not selected after power loss.
- Rollback commissioning must be supervised while USB recovery is available. A deliberately signed test app failing before confirmation should reset and return to the previous validated OTA slot. Such a destructive test build is **not** included/enabled in production. Use a separately reviewed test fixture and explicit owner consent. Do not deliberately power-interrupt partition migration or corrupt the running image.

## Actual rollback and recovery limits

The shipped Arduino2.0.17/IDF4.4.7 DIO bootloader enables rollback, and its ELF contains NEW→PENDING_VERIFY and PENDING_VERIFY→ABORTED writes. The application links a strong `verifyRollbackLater()` override to prevent Arduino's weak early auto-confirmation. A pending update gets a30-second task watchdog and is confirmed only after display/scoring/BLE/provisioned AP startup and at least5 seconds/100 healthy loop iterations. Startup failure requests rollback; a hang/reset before confirmation causes the bootloader to select the previous valid app.

This is genuine compiled support, **not hardware-demonstrated rollback**. It does not validate actual light output/RF, protect bad behavior after confirmation, preserve RAM scores or guarantee recovery if both apps/metadata/flash are damaged. A reset/power failure during the confirmation window may deliberately roll back an otherwise good release. No eFuse anti-rollback is enabled; authorized older signed firmware is allowed.

If rollback reaches the seeded original non-OTA baseline, scoring works but wireless maintenance does not: physical USB recovery is required. After the first successfully confirmed wireless update, the other slot holds an OTA-capable previous app, so ordinary rollback retains the updater. Before sealing, test two successful updates and the approved failure recovery.

To restore the original baseline layout, obtain fresh isolation and explicit restoration approval; use the verified full-flash backup or known-good original partition/application recovery set, including correct OTA metadata. Do not flash only the baseline app into an arbitrary new slot or write an application at address zero. Restoring old flash can discard credentials/current settings; retain private backups.

## Security model and limitations

- WPA2 PSK protects the local Wi-Fi network; the separate256-bit admin key gates sessions; unpredictable bearer/nonces plus exact local Host/Origin stop browser CSRF, DNS rebinding and unauthorized request replay. Fixed methods/routes, no cookies/CORS, no filesystem paths, strict sizes/timeouts and one HTTP worker limit attack surface. Concurrent/duplicate updates cannot select an incomplete image; retry must obtain the rotated nonce.
- ECDSA-P256/SHA256 authenticates the full manifest/image hash with a provisioned public key. Received hash, SDK image validation and readback guard corruption/truncation. Never call these checks hardware Secure Boot: a physical flash writer can bypass the software trust chain. No irreversible eFuse operation or key-burning is performed.
- Local HTTP is **not TLS**. Someone who knows the PSK and attacks another associated peer may observe/alter sessions or cause denial of service. Signed firmware still cannot be forged without the signing key. Use a unique strong PSK and independent admin secret; do not share them with players. AP supports one station at a time, reducing accidental multi-client use, not constituting a formal MITM defense.
- Credentials/NVS/signing private files are not encrypted by this implementation; restrict physical/storage access. There is no network password-reset/backdoor. Losing credentials/signing trust while sealed can require physical USB access; preserve backups and test provisioning/recovery before permanent installation.
- No firmware-image anti-replay version counter is promised. A current authenticated owner can intentionally install an older correctly signed ABI-compatible release. Single-use HTTP nonces prevent stale request replay, not intentional authorized downgrades.
- Radio coexistence and heap/stack margins are hardware commissioning gates. The design briefly keeps BLE/AP available together, then pauses advertising for an exclusive maintenance session; it does not reclaim BLE stack memory. If it fails the hardware load test, do not seal the enclosure or lower integrity thresholds to force it through.
