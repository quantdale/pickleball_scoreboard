# Manual End-to-End Checklist: Display Rendering

Execute this checklist once the LED panel hardware is fully assembled and the Android app can be installed on a phone.

## Prerequisites

- [ ] Firmware flashed to ESP32 and powered.
- [ ] LED panel connected and receiving power/signal.
- [ ] Android app installed and BLE permissions granted.
- [ ] Phone paired/connecting to the `"PickleScore"` peripheral.

## Checks

- [ ] **Initial state (0-0-2 left)**
  - Panel shows: two left arrows, left score `0`, right score `0`, center divider.
  - App preview matches panel exactly.

- [ ] **Rally Left (serving side wins)**
  - Tap Rally Left.
  - Panel/app both update to left score `1`, still two left arrows.

- [ ] **Side-out to server 1**
  - Tap Rally Right twice from a left-serving state to force a side-out while keeping left serving with server 1.
  - Panel/app both show a single left arrow and unchanged score.

- [ ] **Full side-out to right**
  - Tap Rally Right again (or from server-1 state tap Rally Right).
  - Panel/app both switch to right serving with two right arrows.

- [ ] **Double-digit scores**
  - Score rallies until one side reaches `10` and the other reaches `25`.
  - Both digits render clearly on each half without overlapping the divider.
  - App preview matches panel.

- [ ] **Switch Courts**
  - Tap Switch Courts.
  - Scores and arrows swap left/right symmetrically on both panel and app.

- [ ] **End Game**
  - Tap End Game.
  - Panel/app freeze on the current score and serving arrows; no further rallies change the display.

- [ ] **Reset (left 0-0-2)**
  - Tap Reset, choose Left.
  - Panel/app return to two left arrows, `0`–`0`.

- [ ] **Reset (right 0-0-2)**
  - Tap Reset, choose Right.
  - Panel/app return to two right arrows, `0`–`0`.

- [ ] **Color verification**
  - Lit digits and arrows are white, not green.
  - Background is black/unlit.
  - Center divider is visibly dimmer/sparser than the score content.

## Sign-off

- [ ] All checks passed.
- [ ] Any discrepancies between panel and app preview documented below.

Notes:
