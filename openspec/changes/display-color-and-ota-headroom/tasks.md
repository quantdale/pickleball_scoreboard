## 1. Color

- [x] 1.1 Palette, `K<code>` command, `K,<code>` report and repaint (`display_color.h`, parser, BLE service, renderer)
- [x] 1.2 Shared settings object, settle-delayed NVS save of changed keys only
- [x] 1.3 Android swatches, per-setting coalescing, preview in the board color
- [x] 1.4 Unity, JVM and Compose tests at three sizes; screenshots

## 2. Controller

- [x] 2.1 Remove the Undo key; Resume sends one Undo per ended report
- [x] 2.2 Keep the activity (and BLE) across configuration changes

## 3. Memory and OTA

- [x] 3.1 32-position Undo ring; static DRAM 68,484 -> 62,452 B; audit limit 63,000 B
- [x] 3.2 Armed-timer stop-and-delete, status counters
- [x] 3.3 Safe revert of a cancelled install; 503 for heap refusals; `inactiveWritten`
- [x] 3.4 Bounded receive-timeout tolerance; CLI headroom gate
- [x] 3.5 Advertising guard

## 4. Release

- [x] 4.1 Signed APK 2.2.0 (code 9), upgrade from 2.0.0
- [x] 4.2 Signed `court-color-001`, authenticated with the Court 1 key
- [x] 4.3 Owner-authorized USB install on Court 1 with verified backup and readback (`court-color-001`)
- [ ] 4.4 Repeated maintenance lifecycle on hardware: settled heap, `timerRecovered`, closure, BLE recovery
- [ ] 4.5 Separately authorized wireless write acceptance
- [ ] 4.6 Physical color and brightness acceptance (Spec 03 §2a)
