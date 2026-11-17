## Context

See proposal.md. Installed HTTP routes accept ordinary direct HTTP with exact Host/Origin, X-Admin-Key, bearer token and rotated single-use X-CSRF-Token. Upload stages; install is separate. Existing info lacks boot confirmation. Windows Wi-Fi radio is currently software-off; Ethernet supplies internet.

## Goals / Non-Goals

**Goals:** one explicit deploy command, tested transport, restored Windows state, authenticated BLE activation and verifiable running image/health.

**Non-Goals:** BLE image transfer, Android redesign, autonomous match interruption, migration/provisioning, cloud control, hardware Secure Boot changes.

## Decisions

- Python CLI uses standard HTTP bound to Wi-Fi IPv4, never proxies/redirects, and Native WLAN API via ctypes. WLAN profiles are unique temporary manual-connect profiles with secrets passed in memory; remove them and restore prior profile/radio state in finally. Do not change routes/metrics: refuse if Ethernet no longer owns internet routing, or AP routing is ambiguous. Source-bound Ethernet TLS probe before/during/after deployment proves connectivity rather than guessing from adapter status. WSL delegates CLI to Windows Python.
- Device registry and owner credentials stay external. Target MAC, HTTP legacy byte-reversed device ID, SSID, WLAN interface and package key are pinned; later firmware adds authenticated health/image identity. Cross-process exclusive lock and explicit `--authorize-score-reset` required for every disruptive operation. Zero score is never interpreted as idle.
- BLE uses separate read/write characteristic in the existing service (no change to scoring characteristics). Read: version1, canonical base MAC6, random nonce32 (39 bytes). Write: version1, flags1 (=1 explicitly permits volatile-match loss), HMAC32 over domain `PKS-MAINT-1\0` + entire challenge + flags. HMAC-SHA256 key is existing adminHash; signing private key remains independent. Challenge is per connection, expires20 seconds, consumed on first attempt, disconnected/new connection invalidates it; five failures lock activation60 seconds. Callback queues authorization only; loop opens bounded AP only after that central disconnects and no intervening BLE activity/queued scoring commands. Controller occupancy produces a bounded refusal, never forced disconnection.
- Preserve initial180-second boot window for backward compatibility. Authenticated BLE activation reloads credentials, starts fresh180-second window and stops on scoring activity. Cleanup waits for WIFI_AP_STOP acknowledgement before deinit, checks errors, and refuses reactivation if cleanup failed. No repeated init while cleanup task runs.
- Updated HTTP info exposes boot state/confirmation, uptime, app hash and capability. CLI requires expected version, opposite slot, exact image hash and confirmed healthy state; legacy firmware requires safely available serial confirmation and explicitly reports legacy health limits otherwise. Never retry uncertain install POST; observe reboot instead.

## Risks / Trade-offs

- HTTP/WPA2 is not TLS → preserve existing signed-package security limits; verify target, package and network locally, no secret logging.
- Single BLE central occupied → fail and ask owner to disconnect controller; do not evict it.
- Windows permissions/location/radio policy may deny WLAN/BLE → report actionable blocker, restore what changed, no privilege/policy bypass.
- Wi-Fi shutdown warning → instrument return codes/event acknowledgement and physically test repeated activation before claiming fixed.
- Initial app upgrade can lose RAM game state → require explicit authorization regardless of score.

## Focused heap-admission correction

The logged48,208 is Arduino's minimum-ever INTERNAL heap, not the current-free value used for admission. Keep the100,000 startup/49,152 operation/32,768 emergency floors; their original conservative policy has no measured worst-case transfer calibration and cannot safely be reduced to fit a historical number. Add current/minimum/largest8-bit and internal8-bit telemetry plus exact initialization/rejection stages. Add a known-contiguous-allocation fence before the existing8KB HTTP task and4KB sector operations, without counting historical minima as available memory.

Pinned SDK disassembly shows dynamic TX bypasses static TX allocation, so clearing its inherited8 static-TX count is not a memory fix. Instead Arduino's btStart enables BTDM under this SDK although PickleScore needs only BLE. Its ELF contains a15,448-byte CLASSIC-only controller region; initialize BLE-only once at boot and release only unused Classic controller RAM BEFORE initialization using the supported API. BLEDevice then sees the enabled BLE controller and does not restart it in BTDM mode. Never deinitialize/release BLE during maintenance and never touch eFuses; release is RAM-only for this boot and reset restores the initial image. Apply only to opt-in OTA builds. A host adapter test must enforce ordering, mode selection and fail-closed preconditions; actual reclaimed memory and coexistence remain hardware gates.

Build/sign agent-wpa2 but do not flash until new candidate-specific owner authorization and fresh electrical isolation. Existing recovery/signing/partitions remain unchanged.

## Migration Plan

First prove CLI against existing signed B/A via Wi-Fi with safe serial reset if isolation remains confirmed. Then OTA application-only agent-enabled build. No bootloader/table/NVS migration. Prove repeated BLE-triggered updates after startup timeout. Keep verified full backup and original release; report destructive rollback fixture and lit-panel acceptance separately if not safely demonstrated.
