## 1. Independent software preparation

- [x] 1.1 Inspect current Git state, existing firmware/specs, installed library API and USB enumeration without opening serial.
- [x] 1.2 Implement isolated explicit-GPIO single/dual diagnostic, low-brightness color cycle, blank startup and initialization diagnostics.
- [x] 1.3 Compile both environments for esp32dev and confirm generated firmware images.
- [x] 1.4 Document upload safety gates, staged single-panel procedure, scan uncertainty and deferred two-unit migration assessment.

## 2. Hardware-gated verification

- [x] 2.1 Obtain owner's PSU isolation, unpowered HUB75 disconnection, nonconductive support, USB-only power and serial-device identity confirmation.
- [x] 2.2 Identify connected ESP32 chip and successfully upload single-panel firmware; capture tool-confirmed upload result.
- [x] 2.3 Capture serial initialization and at least two complete color cycles with panel isolated; investigate any resets/errors.
- [ ] 2.4 Obtain physical wiring, branch protection, mains enclosure and first-power-on safety confirmation; receive owner's visual Panel 1 color-test result.
- [ ] 2.5 Only after explicit Panel 1 pass and owner authorization, commission the separately prepared two-panel mode; do not claim geometry from solid fills. Owner now reports Panel 1 passed and both chained panels illuminated; new independent geometry patterns still require visual verification.

## 3. Two-panel diagnostic extension

- [x] 3.1 Verify installed library 1/8-scan and serial block ordering; add independent half colors, grid and row/column sweeps while preserving single mode and brightness.
- [x] 3.2 Compile single and dual, audit GPIOs and geometry mapping, and document expected physical patterns.
- [x] 3.3 Obtain renewed USB-only flashing isolation confirmation after the owner's panel reconnection.
- [x] 3.4 Re-identify the ESP32 on COM5, flash dual and capture serial initialization and a complete diagnostic sequence.
- [ ] 3.5 Obtain owner's visual confirmation of independent colors, labels, grid orientation and row/column sweeps.
- [x] 3.6 Correct harness-local editor metadata for Xtensa headers/GCC flags; verify C++ diagnostics without changing the uploaded firmware or real build flags.

## 4. Final pixel-boundary validation

- [x] 4.1 Audit prior renderer/scan/chain boundaries; add a pure coordinate renderer and host checks, including a one-column-shift fault-detection control.
- [x] 4.2 Add a separately selected boundary profile with A/B borders/fills, C corners/seam/isolation, original green/white reproduction, checkerboards and D sweeps; preserve old profiles and brightness/timing.
- [x] 4.3 Compile all profiles and run host coordinate coverage/isolation checks; document exact visible patterns and unresolved physical defect.
- [x] 4.4 Establish current USB-only isolation and accessible ESP32 serial device; identify chip, upload boundary firmware, and capture the full serial sequence. Reconnected board identified on COM11 with matching MAC; upload hash checks and 140-second/two-sequence serial verification passed.
- [ ] 4.5 Receive owner's physical boundary observations; do not claim the reported green strip fixed from software checks alone.
