# Two independent two-panel scoreboards — deferred migration assessment

Owner's new target: two separate ESP32-controlled units, two 32×16 modules each.
This assessment does **not** amend approved production specs or implement that
migration. First commission one panel, then the chain and its actual scan mapping.

| Area | Actual repository state | Required later change |
|---|---|---|
| Contract | `docs/specs/03-display-rendering.md` §2/§6 mandates four modules, 64×32, identical app canvas | Approve a new two-panel layout and physical mounting/rotation per unit; update authoritative specs and OpenSpec deltas in a separate change |
| Driver | `firmware/src/display_render.cpp` uses width 32, height 16, chain 4, default GPIOs, ignores `begin()` result | Explicit commissioned pins, chain 2, checked initialization, brightness policy; scan/driver profile based on actual panel, not P10 branding |
| Geometry | Current direct DMA chain is 128×16, while renderer claims 64×32; no physical virtual-grid mapping is wired | Separate physical chain/scan mapping from logical layout. A two-panel raw chain is 64×16 only under ordinary 1:8 addressing; 32×32 stacked needs topology mapping. Four-scan needs its additional remap |
| Coordinates | `firmware/include/display_render.h` and `display_render_logic.cpp` assume 64×32; digit top y=15 | 64×16 clips the old seven-row digits. Re-layout arrows/digits/divider rather than scale/truncate implicitly |
| Assets/font | `shared/display_assets/layout.json`: 64×32, centers 15.5/47.5, arrows y=3, digits y=15, divider x=32 | Revise layout constants and mirrored C++ copies together; existing 5×7 digits can likely remain. Double 9×7 arrows plus 1px gap occupy 19px: they do not fit a 16px-wide half of a 32×32 canvas, requiring an approved design decision |
| Android | `DisplayRenderer.kt` reads shared assets; `ScoreboardScreen.kt:ScoreboardPreview` hardcodes aspect ratio 64/32 | Match the approved new layout and aspect ratio (4:1 for 64×16; 1:1 for 32×32). Preserve dumb-renderer behavior |
| Tests | Firmware rendering tests assume old coordinates/bounds; glyph drift checker compares JSON/C++ | Update expected pixel sets/bounds, add scan/topology mapping tests and physical row/column diagnostics; retain scoring/protocol tests unchanged unless separately approved |
| BLE identity | Both controllers would advertise `PickleScore`, same service/characteristics. Android filters service UUID then exact name (or null); UI already shows name plus address and explicitly selects a device | Existing address display distinguishes devices technically. For usable court identity, propose stable unit labels/selection and remembered identity. Suffix names would be rejected by the current name filter unless it changes too; do not rename firmware alone |
| State ownership | Each ESP32 owns its own game; app has one active client | Keep independent games and command routing to the selected unit. No inter-unit sync or multi-phone feature is implied |
| Docs/validation | AGENTS, manual E2E checklists and OpenSpec display artifacts describe four panels | Update after approval; include wrong-unit-selection tests with two nearby units and mounting/chain diagrams |

Recommended sequence: single-panel RGB pass → identify scan/ICs and verify pixel
rows/columns → two-panel chain test → confirm final mounting orientation →
propose/approve the production layout + identity migration → implement firmware,
shared assets, Android preview, tests and docs together. Do not flash current
production firmware as a hardware fallback without correcting its configuration.
