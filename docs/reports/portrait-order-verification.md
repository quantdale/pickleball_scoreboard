# Court 1 portrait panel-order correction — `portrait-order-004`

Signed offline on 2026-11-19. No hardware access, maintenance activation, upload, installation, reset or wiring change in this session. **Illuminated acceptance of the new candidate is pending.** `portrait-installed-003` retains the faulty order and is superseded for Court 1.

## Evidence and fix

The [earlier isolated install](court1-portrait-r180-install.md) and the later no-write installed check (`20261119-pi-court1-installed-witness/result.json`) authenticated `portrait-r180-002` on `ota_0` as `VALID`, confirmed and healthy. The later check used normal installed power with both panels connected, one authenticated BLE/Wi-Fi session, and verified close and BLE recovery with `0,0,L,2,0,D`. The owner now reports repeatedly seeing upright `2 / ← / 0 / 0` on the real display. This report uses the owner's description; the photograph was not inspected in this session.

| Viewer half | Old DMA columns and content | Corrected DMA columns and content |
| --- | --- | --- |
| Logical top, y0–31 | x31→0, both scores appeared on physical bottom | x63→32, both scores on physical top |
| Logical bottom, y32–63 | x63→32, server and arrow appeared on physical top | x31→0, server and arrow on physical bottom |

The old renderer sent the right pixels to the wrong blocks. The observation anchors the model independently of production: physical top is raw x32–63, physical bottom is x0–31, with raw x descending as viewer rows increase within each. Old `rawX = y < 32 ? 31 - y : 95 - y` becomes `rawX = 63 - y`; `rawY = x` remains unchanged. This toggles raw x bit 5 for every pixel and preserves both local axes, not a 180° rotation. Horizontal `physicalDisplayX()`, GPIO, brightness 12/255, all logical pixels (including the retained two-row score gap and dark perimeter/seam), Android, BLE/scoring and OTA code are unchanged.

## Verification

- On unchanged production mapping from `721d5ef`, `test_previous_block_order_reproduces_owner_observed_startup` **passed**, while `test_installed_startup_has_scores_above_server_and_arrow` **failed**: “First zero is missing from the physical top panel.” The coordinate regression failed with raw block expected 1, actual 0. On the correction, **97 Unity tests** passed. They include all 1,024 coordinates, corners and seam, asymmetric one-/two-digit views and both arrows, 31 incorrect order/axis combinations, and the two-row gap and perimeter.
- **72 full logical pixel snapshots** matched the actual production C++ logical renderer, shared oracle and Kotlin test renderer without fixture updates. **44 Android JUnit and 3 API 35 Compose tests** passed. Android debug and test APKs built; lint used the unchanged baseline with two visible portrait-lock warnings. No new Android package is needed for this mapping-only change.
- **46 OTA CLI and 15 signing/container tests** passed. `esp32dev`, `ota` and `ota_b` production profiles built. The offline audit verified target ESP32, unchanged 4 MiB partition table and bootloader, 2,031,616-byte OTA slots, bootloader rollback instructions and strong linked deferred confirmation. Court 1 P-256 public trust independently verified the new package and the embedded payload is byte-for-byte identical to `firmware.bin` (1,590,816 B, 440,800 B slot headroom).
- Local evidence in `D:\Documents\tryPython\_AgentArtifacts\pickleball_scoreboard\sessions\20261119-pi-portrait-order-004`. `panel-order-before-after.svg` is **simulated**, using the installed layout for the before image and retained spacing for the after image; the startup before panel pair matches the **owner's physical description**, and the asymmetric cases are predictions. `host-old-mapping.log` and `host-regression.log` retain the red/green evidence.

| Candidate artifact | SHA-256 |
| --- | --- |
| `candidate/portrait-order-004.pksota` | `fbd748f4c3a8143dde28d8dfdb9345d082f17a1b4cf29ddab83a1de73c5686a6` |
| `candidate/firmware.bin` | `683136620020b4f5f9c7706800b34fce1e0dab0ad8e7df17c8182dc86a45ee35` |

## One-time installed OTA authorization request — not granted here

- **Exact target/source:** Court 1 base MAC `3c:8a:1f:a1:ba:28`, BLE `3c:8a:1f:a1:ba:2a`, AP BSSID `3c:8a:1f:a1:ba:29`. Reconfirm authenticated `portrait-r180-002` / `ota_0` / `VALID`, confirmed and healthy, image SHA-256 `b8aa04446f1e8b24a99905e714e6635ca2b85f5ae1385e684626c57127e608e0` before upload. Stop on any mismatch.
- **Scope:** stage the exact signed `portrait-order-004` above once to inactive `ota_1`; this replaces the expected `portrait-cw-001` alternate. Application bytes `[0x200000,0x384620)`, sector erase `[0x200000,0x385000)`, normal boot selection and confirmation metadata `[0xE000,0x10000)`. No planned writes to active image, partition table, bootloader, NVS or eFuses. Authorize the normal install reboot and volatile score/Undo loss, not forced reset or retry. Leave both panels connected and powered by the approved installed arrangement; confirm idle court, stable power, voluntary controller disconnect and safe recovery access.
- **Recovery:** keep signed `portrait-r180-002`, `portrait-cw-001` and `court1-return-002` archives. `ota_0` remains selected until install and remains the expected trial fallback. If transfer or boot confirmation is uncertain, stop and establish the exact running image. No automatic replay, forced rollback or USB fallback. If wireless recovery is unavailable, use a separately approved isolated service plan; the powered no-write check did not exercise a full transfer or trial rollback.
- **Physical acceptance after install:** verify exact `portrait-order-004` / `ota_1` / `VALID`, confirmed and healthy, close maintenance and confirm BLE sync. Witness upright `0 / 0 / 2 / ←` with separated zeros, then use the [bounded operator sequence](../portrait-operator-guide.md#mount-and-verify--separately-authorized-work) to capture `2 / 3 / 2 / ←` and `2 / 3 / 2 / →` with a seam and physical-left landmark; compare with Android. Stop on discrepancy. No full hardware acceptance is claimed until those observations exist.
