# PickleScore Android 1.3.1: stable live connection status

The approved 1.3.0 layout is unchanged apart from reserved space inside the live
connection card. Previously the card inserted a text row for each BLE write and
removed it after acknowledgement. The preview and controls moved **22 dp** on
the first queued command at 320 dp / 1.35× font scale. The card now reserves the
same space at idle, during writes, and for the memory and legacy notices, using
the current width and font scale. A rejected-action warning takes priority over
queue progress. A real disconnect or write failure still changes to the
reconnect screen, shows the full error, and disables scoring until resynchronized.
BLE transport, command handling, scoring, display assets, and firmware did not change.

## Release and evidence

Session root: `D:/Documents/tryPython/_AgentArtifacts/pickleball_scoreboard/sessions/20261120-pi-final-fixes/`.

- APK: `apk/PickleScore-1.3.1.apk`; `com.example.pickleballscoreboard`, version code
  **6**, non-debuggable release. Open it over 1.3.0 and choose **Update**; do not
  uninstall first. The Board menu shows `PickleScore 1.3.1`.
- APK SHA-256: `ceccae7d06c3d470d35a28145fe11ed89cc5f897543d1fa37dc4ff2afa3cbfa9`.
- Signing certificate SHA-256: `548d4e27a37909013053429dd013c1b3750f2728c149c9255df3e8788407ef06`.
  APK v2/v3 verified. This is the existing local-distribution signer (certificate
  labeled Android Debug), not a Play Store signer; the app itself is not debuggable.
- `adb install -r` upgraded the archived 1.3.0 APK to the exact signed 1.3.1 APK on
  API 35. App UID, first-install timestamp, Nearby Devices grants and an app-private
  file were retained. Both installed APKs matched their source files byte-for-byte.
- 51 JVM tests, six Compose tests at both 320 dp / 1.35× and 390 dp / 1.0×, and
  release build and lint passed. The new Compose test failed on the old layout and
  passes with exact equality of the preview and all six control bounds while the
  queue changes repeatedly. Existing tests cover Undo, confirmations, reconnect
  gating and actual Singles/Doubles preview pixels. Lint reported no unbaselined errors.
- On the signed release, an emulator-only GATT peripheral held each of Rally Left,
  Rally Right, Undo and Switch Courts in flight in turn. The preview and six control
  bounds matched exactly before, during and after the ACK. An ATT write rejection
  showed the reconnect error and disabled the Rally Left button. No physical phone
  or scoreboard was connected.
- Screenshots: `screens/release-idle.png`, `screens/release-queued.png`,
  `screens/release-write-failure.png`, and `screens/release-version.png`. Before/fix
  Compose captures are in `screens/before-*.png` and `screens/after-*.png`. Raw
  evidence includes `compose-red.log`, `compose-320-135.log`, `compose-390-100.log`,
  `release-journey.log`, `upgrade.log`, and `apk/verification.log`.

The [P10 follow-up](p10-color-and-ghosting.md#follow-up-on-court-undo-003) records
what can and cannot be concluded offline. No firmware candidate was warranted.
