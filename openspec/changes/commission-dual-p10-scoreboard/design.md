## Context

Owner authorizes the 64×16 migration and accepts bring-up. Existing state engine matches the nonstandard side-out-to-2 rule; preserve it. Existing production loops redraw continuously and BLE sends initial Notify before CCCD subscription; Android reports connected before descriptor completion.

## Goals / Non-Goals

**Goals:** Actual production firmware/APK, matching logical pixels, safe hardware initialization, deterministic scoring/protocol/pixel tests, deployable artifacts.

**Non-Goals:** New scoring rules, seam re-investigation, second court, brightness changes, pairing, cloud or OTA.

## Decisions

- Use 10×14 score digits at y=1..14; scaled 5×7 font, 2px digit gap. Left center18.5 (two digits x8..29), right44.5 (x34..55). Outer arrows5×5 at x1/58; one at y5, two at y2/9. No divider. Outer columns and seam remain blank. Existing font retained, arrow bitmap reduced and mirrored. Overflow shows two dashes.
- Keep shared JSON as design source; firmware static copies remain drift-checked. Extract pure Kotlin pixel computation used directly by Compose and JUnit. Compare complete cross-language snapshots, not merely bounds.
- Reuse accepted pin/driver/clock/chain-block mapping; no new hardware library. Hold active-low OE high (blank) during setup; handle begin failure, draw boot state. Redraw only after changes; no full-frame allocation (existing vector contains only lit pixels).
- Firmware queues small command records from BLE callbacks to the Arduino loop. Only that loop mutates scoring/undo and draws. Atomic subscription gate requests current state when CCCD enables. No callback-driven full-game reset on reconnect.
- Android main-thread lifecycle, subscription+initial-state readiness gate, one outstanding acknowledged command write with bounded FIFO, connection timeout and stale-GATT guards. No new wire commands or readable characteristic.
- Host tests use existing Unity suites via MSVC if PlatformIO native is unavailable; no unrelated toolchain repair. Pixel fixtures are exercised in C++ and Kotlin, with rendered images in session artifacts.

## Risks / Trade-offs

- [House scoring differs from standard doubles] → Preserve approved full side-out to server2; flag as owner gameplay decision before match acceptance.
- [Suspected seam/edge artifact] → Accepted known limitation; act only if it affects readability/normal play.
- [Open BLE/one central] → Accepted v1 unchanged; test connection/discovery on hardware manually.
- [Power and live display inaccessible to software] → No automatic energizing; isolated USB-only gate before any real scoreboard flash.

## Migration Plan

Preserve diagnostic source/profiles and last known-working boundary binary/hash. Build actual firmware and debug APK; check host tests and emulator where available. Complete manual E2E checklist with owner, keeping radio/display acceptance separate from host passes. Roll back via explicit boundary environment with isolated board if needed.
