# Held color isolation — spare bench only

Use this baseline to correlate missing primaries with physical LED coordinates and receiver signals. See the [Court 1 findings and decision table](../../docs/reports/p10-physical-color-isolation.md). This is not a scoreboard firmware or an OTA image. It is not approved for any hardware write or reconnect by a successful build.

## Prepare without hardware access

```bash
PYTHONIOENCODING=utf-8 firmware/.venv/Scripts/pio.exe run -d hardware-tests/p10-bringup -e color_isolation
```

Record the image SHA-256 of `.pio/build/color_isolation/firmware.bin` under this project, the source commit and dependency versions. The older `single`, `dual` and `boundary` profiles still build separately. The new profile uses the installed 64×16 SHIFTREG/TYPE138 1/8 setup, single buffer, GPIO map, brightness 12/255 and unchanged clock/phase settings; it logs the I2S clock and sample-rate registers, not the measured CLK. It has no scoring, BLE, or maintenance code.

## First physical session: baseline only

1. Confirm explicit authorization of the spare and this image. Identify the spare by chip/port with panel power isolated and HUB75 disconnected; have a verified spare recovery image before any write. Follow the [power and isolation gates](README.md), including rated protected 5 V supplies with common ground, no panel 5 V at the ESP32 and no hot-plugging. **Never flash Court 1.** Do not reuse a historical COM port or change a production ribbon, GPIO, ground, firmware or supply.
2. Only after that gate, upload `color_isolation` to the verified spare via USB with panels isolated. Verify startup/black and record `CLKM_CONF` and `SAMPLE_RATE_CONF`. Have the bench operator reconnect through the normal powered-off procedure. For selecting patterns on the lit bench use the bench's verified no-reset 115200-baud console (no DTR/RTS reset connection); do not open a reset-capable USB console on a connected display. If this setup is unavailable, stop and agree on a safe bench console before continuing.
3. Start on `a` (both), `0`, `w`; after it settles, capture and mark all wrong-color lit positions and unexpectedly lit dark neighbors by eye and in unclipped originals. Hold until captured. Use phases `1` through `7` to cover every LED, not a rapid cycle. Each mask illuminates exactly 256 of 1,024 positions, 32 per ABC address across both panels (16 per module). Each LED is on in two phases. Brightness and sparse loading are **not electrical protection**. Stop for supply limits, instability, unexpected brightness, heat or smell.
4. At an affected phase, send `r`, `g`, `b`, `w`, then `k` individually, with settled captures at each step. Colors keep exactly the same mask; `k` is black. Compare a healthy on-pixel in the same bank and a dark neighbor. At the affected phase on each relevant module, compare `a` → `n` (near only) or `f` (far only) → `a` without changing the panel's own pattern or the wiring; command `k` when finished. Preserve both `P10 held:` log lines and photo IDs. No timing changes or substitutions are part of this first session.
5. A qualified operator captures DC rail at the panel's power connector and captures `CLK`, `LAT`, `OE`, `A/B/C`, and at least the affected and a healthy RGB lane at the receiving header (prefer all six lanes). Use low-capacitance scope probes with short local DC ground connections and valid bandwidth for signal edges, not just the clock frequency; configure logic-analyzer thresholds/sample rates for the actual interface (e.g. ≥100 MS/s for bit alignment, scope for nanosecond margins and ringing). Record instrument and probe settings. Never attach a ground clip to anything other than a verified compatible reference, defeat protective earth, or probe live mains. Do not place handheld probes where they can bridge pins. If no safe header/pad access exists, do the optical baseline and stop before probing.

### Reference for the wire capture

Trigger on LAT; capture multiple latches and at least one address rollover. Index clock samples **0..63** for data being loaded; the LAT word itself is at sample 63. Use the actual driver IC's latch/sample edges. `rawY` selects RGB1 for 0..7 and RGB2 for 8..15; use the address where the data will be **displayed**, not necessarily the ABC level during its shift. On the source at phase `0` and logical address `0`, either block has these expected white-mask bits, earliest first:

```text
RGB1 11000000 00000110 00110000 10000001
RGB2 00001100 01100000 00000011 00011000
```

In white, all three lanes of each bank should decode the same pattern; in a primary, only that lane carries it. This is the expected input stream, not a sample of actual signals. Compare the first 32 source clocks to the downstream panel's corresponding **last** 32 after propagation through the near 32-stage path. At any phase, a position is lit when `(rawX + 3*((rawX % 32)/8) + rawY + 4*(rawY/8) + phase) % 8 < 2` with integer division, and its block is selected. The same stream sequence does not guarantee the same waveform at each J1. Bench panels may not be mounted like Court 1; record connector and module orientation before assigning portrait coordinates.

**Hold point:** If the baseline does not reproduce, save the negative result and ask for the unchanged Court 1 held-score coordinates/read-only capture. Do not choose a different clock, cable, chip setting, or repair until baseline evidence distinguishes the failure and the relevant intervention is authorized. Reverting an experiment on a spare means restoring the saved spare image/configuration only, with the same isolation gate and separate permission; no Court 1 rollback is involved.
