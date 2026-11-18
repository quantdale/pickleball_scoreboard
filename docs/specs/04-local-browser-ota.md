# Spec 04: Local browser OTA

Status: APPROVED for implementation by owner's browser-OTA master task. Physical acceptance pending.

## 1. Preserved behavior

Specs01–03, all BLE bytes/UUIDs,64×16 pixels and brightness12/255 remain unchanged. RAM-only scores survive maintenance cancellation but reset to0-0-2 after an explicitly confirmed update reboot. No Android changes, internet, cloud, captive portal or new hardware.

## 2. Provisioning and trust

No credentials/default signing key in firmware/Git. Provision one NVS record over isolated USB during the first180 seconds, only without BLE control or maintenance. Record: schema1,33-byte NUL-padded SSID,33-byte NUL-padded32-character AP PSK,32-byte SHA256 of an independently generated256-bit admin secret,65-byte SEC1 uncompressed P-256 public point. Store atomically as one NVS blob; validate all fields and curve point before saving. Never print secrets. Physical recovery/reprovisioning requires isolated USB and a boot window. Losing the signing private key or passwords cannot be bypassed over Wi-Fi; back them up before sealing.

## 3. Local radio lifetime and gameplay priority

Provisioned device exposes WPA2 AP192.168.4.1/24 for180 seconds after boot, without STA/routing/DNS interception. Limit buffers and clients; initialization/low-memory failures disable maintenance, not normal BLE. Unauthenticated traffic never extends this window. A controller connection closes maintenance for that boot. Owner must stop/disable the scoring controller before power-cycling for maintenance.

An authenticated session is absolutely bounded to600 seconds, including any transfer (individual transfer maximum120 seconds; socket idle receive timeout2 seconds). Maintenance admission requires no central or pending command, confirmed advertising-stop event and a quiet interval. Preserve DMA/last score; no continuous redraws. BLE is not deinitialized/recreated. Unexpected BLE activity cancels maintenance; accepted commands always run through the existing loop. Non-reboot exit restores advertising. No installation while BLE controls the scoreboard.

For a running AP, close the HTTP server and observe AP_STOP after default netif stop before deinitializing Wi-Fi. If stop/acknowledgement/deinit fails, retain the SDK dependencies and refuse reactivation until reboot; do not automatically reboot. The cleanup worker owns one static task for the boot and parks between requests.

## 4. HTTP authorization and request integrity

Fixed local HTTP origin `http://192.168.4.1`; exact Host/Origin and endpoints/methods are enforced. WPA2 protects local transport; HTTP is NOT TLS and a malicious peer knowing the AP PSK may MITM/observe sessions. Separate high-entropy administration credentials and signed firmware limit this risk; do not share the PSK/admin key broadly. No cookies/CORS/external assets. Session login verifies the admin-secret hash in constant time; five failed attempts close the AP. Use a fresh random bearer token and single-use random state-changing request nonce per session; rotate/consume the nonce before processing each mutator. Tokens remain in browser memory only and are invalid after timeout/reboot. No GET request changes firmware or reboots. Authenticated `/api/status` reports zero `received`/`total` and an empty `stagedBuild` when no transfer is active or staged; the build is published only after verification.

## 5. Signed firmware package

Little-endian fixed170-byte header: magic `PKSOTA01`8, format u32=1, image size u32, target16=`PKSCORE-ESP32-V1` NUL padded, build32 (ASCII alphanumeric/underscore/dash/dot, NUL padded), payload SHA25632, DER signature length u16, DER ECDSA signature72 (unused bytes zero). P-256/SHA256 signs the first96 bytes, binding metadata and complete-image hash. Trust only the provisioned public key; a hash alone is not authorization. The payload is an ordinary ESP32 application image, NOT a merged flash binary; require the OTA compatibility marker and SDK image/chip validation. Authorized older signed versions are permitted; no eFuse anti-rollback is claimed. HTTP request replay is independently blocked by session/nonce state.

## 6. Transaction and partitions

Required4MB table: NVS0x9000/0x5000, otadata0xe000/0x2000, ota_0 at0x10000/0x1f0000, ota_1 at0x200000/0x1f0000, coredump0x3f0000/0x10000. Refuse unexpected layouts. Initial migration must be explicitly authorized; no automatic erase, repartition, bootloader replacement or eFuse write.

Only the inactive slot may be erased/written. Enforce exact Content-Length and signed size, validate signature before flash allocation, bound streaming/prefix buffers and reject invalid/truncated/oversized/interrupted/concurrent transfers. Verify received SHA256, SDK image validity and complete flash readback hash before staging. Upload alone never selects/reboots. Explicit install confirmation, a current one-use nonce and exclusive BLE-free state are required before boot selection/reboot. Failure leaves the running app selected; never reboot into rejected bytes.

## 7. Boot validation and rollback

The shipped DIO bootloader's real NEW→PENDING_VERIFY and PENDING_VERIFY→ABORTED logic is verified from its ELF, not merely SDK macros. Override Arduino's early auto-confirmation. Pending OTA boot must pass display/scoring/BLE/provisioned maintenance initialization plus healthy loop progress before confirmation. A pending-boot watchdog resets a hung application; bootloader rollback then selects the previous valid slot. Seed a known valid fallback during first authorized migration; no fallback is possible if only one valid image exists. No irreversible secure-boot/eFuse protection is enabled: physical flash access can bypass software trust. Boot self-checks do not prove real LED/RF behavior or post-confirmation application quality.

## 8. Agent-operated deployment (owner-approved extension)

Each deployment or maintenance activation requires explicit owner authorization to interrupt and lose volatile match state;0-0 is never evidence of idle. Keep scoring UUIDs/commands and queue ownership unchanged. Use the existing signed HTTP upload, refreshed bearer/one-use nonce and separate install operation. The live CLI requires a previously signed package pinned by its owner-approved digest and the approved source build, slot and image digest, currently `VALID`/confirmed/healthy. Verify these before upload, and send the same immutable bytes that were verified. No live build-and-deploy or USB-reset fallback. The separate `verify` operation can use isolated serial evidence for legacy firmware without health fields, but cannot authorize a live write from that source.

Verify intended base MAC/SSID/BSSID/device ID, exact staged build and byte extent, then expected version/opposite slot, exact running image SHA256 and confirmed startup. Build, package verification, upload acknowledgement, verified staging, install acknowledgement, expected image boot and confirmed health are distinct evidence. An accepted close request is not proof that SDK teardown completed.

A dedicated read/write characteristic `beb5483e-36e1-4688-b7f5-ea07361b26aa` on the existing service activates maintenance only, never transfers firmware. Read challenge is39 bytes: version1, canonical base MAC6, fresh random nonce32. Response is34 bytes: version1, flags1 (exactly1 = explicit score-loss authorization), HMAC-SHA25632. HMAC key is the provisioned adminHash; message is ASCII `PKS-MAINT-1` plus NUL, entire challenge, flags byte. Provisioned signing authority is independent. Challenge is per connection, single-use even on failure, expires20 seconds; five failed proofs block activation for60 seconds across reconnects. Disconnect invalidates a challenge. A successful proof returns `OK` on read and queues activation only after that central disconnects within20 seconds, with no intervening scoring command/subscription/new central. Occupied BLE never forcibly evicts the Android controller. Only the loop starts the bounded AP after cleanup, reloading existing credentials; no provisioning/migration is implicit.

Windows tooling must retain Ethernet internet connectivity and routing, bind local HTTP to the correct Wi-Fi address, use bounded retries, never replay uncertain install requests and restore prior profile/radio state in success/failure cleanup. Secrets and signing material stay outside Git/logs/command-line arguments. Concurrent deployments are locked. Unverified restoration/health/rollback is reported explicitly, never as success. Updated info includes application image hash, boot state and confirmed healthy-loop progress. Self-check health does not prove illuminated panels or RF scoring acceptance.

## 9. Acceptance

Host/fake-flash/security-tool tests and builds are software evidence only. Hardware acceptance requires owner-observed AP login, two distinguishable consecutive wireless updates, invalid/interrupted rejection, rollback failure exercise where safe, normal BLE/panel rendering and sustained heap/watchdog checks. Initial USB flashing/provisioning requires fresh isolation and explicit migration authorization.
