# Court 1 hardening release (`court-hardened-006`)

**Result: flashed over isolated USB, verified on hardware, and display accepted by the owner.** Court 1 runs `court-hardened-006` on `ota_1` (`0x200000`), installed via an owner-authorized Mode B application-only USB write on `COM11`. `court-headroom-005` remains intact on `ota_0` (`0x10000`) as a valid fallback, and bootloader, partition table, NVS provisioning, `otadata`, and coredump are byte-identical to before the write.

## 1. Display investigation and owner acceptance

- **Host & DMA audit:** Auditing [`display_render_logic.cpp`](../../firmware/src/display_render_logic.cpp), [`display_render.h`](../../firmware/include/display_render.h), [`display_render.cpp`](../../firmware/src/display_render.cpp), and the pinned `ESP32-HUB75-MatrixPanel-I2S-DMA` (`3.0.15`) driver (`esp32_i2s_parallel_dma.cpp`) alongside a bit-exact I2S Mode 1 simulator, 60 C++/Python pixel snapshots, and an exhaustive 10,000-score boundary suite (`0..99 × 0..99`) confirmed zero out-of-bounds pixels, zero stray adjacent pixels, matching `ESP32_TX_FIFO_POSITION_ADJUST`, and `>60` I2S clocks of blanked `OE` HIGH guard around every `LAT`/`ABC` switch at brightness `12–32`.
- **Hardware localization:** In `esp32_i2s_parallel_dma.cpp` (`_div_num = 4`, `tx_bck_div_num = 2`), `HZ_8M` clocks the HUB75 bus at 10 MHz with max GPIO drive strength (`3.3 V` into `5 V` P10 inputs) alongside known 1/8-scan P10 row-switch/LED leakage.
- **Owner decision:** After physical panel inspection following the `court-hardened-006` install, the owner reported extremely faint unintended pixel illumination only noticeable at very close distance, without affecting readability during normal use, and accepted it as a minor visual imperfection with **no** display timing, DMA, rendering, or hardware modifications.

## 2. Implemented hardening changes

- **Firmware BLE (`ble_service.cpp`, `ble_command_ingress.h`, `ble_activation_core.h`, `ble_activation.cpp`):**
  - Replaced cross-task `BLECharacteristic::setValue()` / `notify()` calls with direct `esp_ble_gatts_send_indicate` using atomically tracked `gattsInterface`, `activeConnId`, `stateHandle`, and `manualHandle`, plus `pendingNotifies` retry flags in `processBleCommands()` preserving Spec 02 §6 ordering (`manualState` before `B`/`K`).
  - Protected `activeConnId` clearing on disconnect with `compare_exchange_strong` against `param->disconnect.conn_id`.
  - Added `ExecWriteTracker` in `dispatchGatts` to return `ESP_GATT_OK` for orphaned `ESP_GATTS_EXEC_WRITE_EVT` requests when no non-scoring characteristic prepared a write.
  - Added `ActivationGate::isAvailable()` and gated `Callbacks::onRead` on it so `...26aa` returns `"NO"` when consumed, invalidated by scoring subscription, or rate-limited.
- **Firmware OTA maintenance (`ota_maintenance.cpp`):**
  - Fed `esp_task_wdt_reset()` on healthy `loop()` ticks during `pendingBoot && !confirmationDone && startupOK`.
  - Cleared `networkStopping = false` if `cleanupTask` creation fails in `requestNetworkStop()`.
  - Compared `HEAP_REFUSED` with `std::strcmp` rather than pointer equality, added `Serial.flush()` before `ESP.restart()` on install, and logged `"BLE activity detected"` when activity reverts a pending install.
- **Android BLE (`BleClient.kt`, `GattSession.kt`, `debug/AndroidManifest.xml`):**
  - Added a 15-second `SCAN_TIMEOUT_MS` bound on `startScan` (reporting `"No PickleScore board found"` only if no board was seen).
  - Retained `requestedBrightness` across the GATT ACK-to-notify window and cancelled redundant queued `B`/`K` writes when stepping back to the reported value.
  - Added `|density|fontScale|uiMode` to debug-only `LandscapeTestActivity` `configChanges` so `aosp_atd` density transitions do not recreate the test host mid-test (`12/12` `connectedDebugAndroidTest` passing on `atd35`).

## 3. USB identity, backup, flash, and live BLE verification (`COM11`)

External evidence directory (outside Git): `_AgentArtifacts/pickleball_scoreboard/sessions/20261126-usb-hardening/`.

| Step | Observed evidence |
|---|---|
| MCU & flash ID | `ESP32-D0WD-V3 (revision v3.1)`, 40 MHz crystal, MAC `3c:8a:1f:a1:ba:28`, 4 MB flash (`85` / `2016`) |
| Pre-flash 4 MiB backup | `pre-flash-4mb.bin`, `4,194,304 B`, SHA-256 `0b9be4ffd5225435ce4be7be848b89d0e20e8141a4296c41339277ed01f87b53`, verified on chip |
| Pre-flash slots & partitions | Partitions matched built `partitions.bin` (`36860d07...`); `otadata`: `ota_0` (`seq=9`, VALID, `court-headroom-005`), active `ota_1` (`seq=10`, VALID, `court-headroom-005b`) |
| App-only USB write (`0x200000`) | `court-hardened-006`, `1,598,944 B`, SHA-256 `52eaffc319433860cef17e26a019f2f286f41ed2359dc0bf189939f8b5d08f84`, `--flash_mode keep --flash_freq keep --flash_size keep` |
| Whole-chip verification | `flash_backup.py verify` against `expected-after.bin` (`96348b8fae3ea14bb4b60df3420c855f5d6aa462543c668bdec2a155bcd4afdf`) returned `digest matched` |
| Serial boot witness (`boot-serial.log`) | `OTA build=court-hardened-006 ABI=PKSCORE-OTA-ABI1 slot=ota_1 pending=0`; `DISPLAY ready ... mount=1 ... brightness=12/255 color=0xF800`; `OTA AP ready ... heap=68592 min_heap=68456`; `SCOREBOARD ready state=0,0,L,1,0,D heap=68592 undo_budget=96 brightness=1/4 color=R`; zero panics/brownouts/WDT resets |
| Live BLE E2E (`3C:8A:1F:A1:BA:2A`) | Service/characteristics verified (`26ab` absent), pre-sub `26aa` 39 B challenge, post-sub `26aa` `"NO"`, Spec 02 §6 sync order (`state`, `B,1,4`, `K,R`), scoring/undo/burst/court-switch/end/reset, malformed rejection, settings dedup/restore, and disconnect/reconnect verified |

## 4. Remaining unverified hardware & OTA checks

- Live Wi-Fi AP HTTP OTA upload, confirm, and failed-trial hardware rollback *from* `court-hardened-006` (installed by Mode B USB write; offline OTA/rollback unit & tool tests passed).
- Spec 03 §2a brightness levels `2–4` electrical current and thermal acceptance.
- Loaded 20-minute physical panel endurance and 50-cycle maintenance heap repeatability.
- Physical Android phone controller E2E against the hardened debug APK (emulator Compose instrumentation + unit tests verified offline).
