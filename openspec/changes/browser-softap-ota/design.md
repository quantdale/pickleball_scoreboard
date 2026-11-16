## Context

See proposal. Baseline is5ae28e2 (firmware3bf53dd), clean, with origin/main unchanged after fetch. Arduino2.0.17/IDF4.4.7,4MB device, application1,156,448 bytes; previous measured post-BLE heap135,592 bytes. Existing slots are0x140000. DIO sdkconfig enables rollback, and shipped bootloader ELF disassembly actually writes PENDING_VERIFY→ABORTED and NEW→PENDING_VERIFY. Arduino normally confirms before setup, so override verifyRollbackLater.

## Goals / Non-Goals

**Goals:** Bounded local OTA with verified authenticity, no active-game reboot, demonstrable safe storage and recoverable first installation.

**Non-Goals:** Secure-boot/eFuse migration, HTTPS/cloud, Android changes, scoring changes, persistent matches, promises of physical acceptance from simulation.

## Decisions

- Keep baseline `esp32dev` profile; add opt-in `ota` and distinguishable second build. Partition CSV: NVS0x9000/0x5000, otadata0xe000/0x2000, ota_0 at0x10000 and ota_1 at0x200000, each0x1f0000, coredump0x3f0000/0x10000. No filesystem. Refuse unexpected runtime layouts; migration cannot occur through OTA itself.
- Local provisioning is a bounded USB-serial boot operation storing one transactional NVS blob (SSID,32-character PSK, SHA256 of256-bit admin secret, P-256 public point). Offline tools generate secrets/private signing key outside Git; no generated default key is trusted. Missing credentials fail closed, leaving BLE usable. Recovery requires isolated physical USB reprovisioning; no network credential-reset endpoint.
- Use existing IDF Wi-Fi/AP and esp_http_server, not blocking Arduino WebServer or new networking libraries. Limit Wi-Fi buffers/sockets/timeouts and use1KB streaming buffers. Public page/status contains no secrets; authenticated sessions are absolute10 minutes. HTTP is protected by WPA2, not TLS; known-PSK malicious peers are a documented transport limitation, while firmware remains signed.
- Exact Host/Origin, bearer session token, single-use nonce, fixed endpoints/methods/content types, bounded body sizes and failed-login limit. No cookies, CORS, external assets or arbitrary file paths. Offline P-256 signature covers a fixed manifest including full-image SHA256; esp_ota_end plus flash readback hash must pass before installation.
- Separate upload/stage from explicit install. A boot AP may coexist briefly with BLE; first BLE activity ends Wi-Fi. Maintenance admission requires no controller/queue, acknowledged advertising stop and a quiet interval. No BLE deinit/reinit (avoids retained Arduino BLE allocations/leaks). Any unexpected BLE activity aborts maintenance; the ordinary loop always processes accepted commands. Advertising resumes on non-reboot exit. This coexistence/memory choice remains physically unverified, with allocation/heap failure closing AP rather than compromising scoring.
- Existing bootloader has real rollback; do not replace it. Delay pending-app confirmation until startup checks and healthy loop progress. Pending-boot watchdog ensures a hung first boot resets into rollback. Initial installation must seed a previous valid image in the migrated second slot; otherwise the first image has no fallback. No secure-boot/anti-rollback eFuses are burned. Authorized signed older firmware is not globally forbidden; request replay is prevented per session.
- Test the hardware-free policy/container/stream transaction using the same code and a fault-injecting sink. Exercise actual offline signature generation/verification separately. Compile actual adapters and run baseline regressions; do not label these radio/flash hardware proof.

## Risks / Trade-offs

- [AP/BLE/DMA runtime headroom not physically established] → reduced buffer limits, heap guard, BLE-priority shutdown and explicit commissioning load tests.
- [HTTP is not end-to-end encrypted] → unique WPA2 PSK plus independent admin secret, no external Wi-Fi, signed images; document hostile associated-peer/MITM limits.
- [First migration destroys old slot/filesystem interpretation] → full backup/recovery artifacts and explicit owner migration approval; no automatic flash operation.
- [Boot confirmation cannot prove light output/RF] → state exactly what self-tests cover; require owner two-update acceptance.
- [Credential/signing-key loss in sealed enclosure] → back up private material before sealing; recovery needs physical USB, not a security bypass.

## Migration Plan

Prepare artifacts only. After explicit migration authorization and fresh isolation, identify board, preserve full flash, install the approved partition/app/seed layout, provision credentials privately and capture startup. Owner then reconnects panels safely and proves browser updates A→B→A, rejection/interruption and game regressions. Keep original baseline partition/boot/app recovery set available for separately authorized USB restoration.
