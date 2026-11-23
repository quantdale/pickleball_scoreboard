# PickleScore Android 1.3.0

## What changed

The 1.1 screen is the layout reference: permanent 16×64 preview on the left, Rally Left, Rally Right, Undo, Switch Courts, End Game and Reset stacked on the right, and connection status and Board menu above. The 1.2 court diagram, side-by-side rally buttons and preview dialog are gone. The cream, teal, lemon and aqua palette, rounded shapes and animated raised keys carry forward from 1.2. [Duolingo](https://refero.design/screens/0307c5b0-172f-457f-b1bf-dc897ebf13a6) informed the key press feedback; [Brilliant](https://refero.design/screens/cf1e5091-af40-4c51-8f06-9dee0549c3f8) informed framing and color hierarchy, not the layout. The controller still uses the board's score and Undo status; BLE, rendering assets and firmware are unchanged.

## Why the previous APK did not show the restoration

The earlier restoration was only in the working tree, built under an intermediate `1.2.1` version. The artifact previously handed over by filename and SHA-256 was the archived signed `1.2.0` APK, which contains the rejected court layout. Reinstalling those bytes reproduces that layout. The emulator accepted the new APK as a same-signer upgrade. There is no evidence that caching or signing caused the old layout on the owner's phone; no phone was attached to verify its exact installed state.

## Verified artifact

Session root: `D:/Documents/tryPython/_AgentArtifacts/pickleball_scoreboard/sessions/20261120-pi-android-1.3.0/`.

- File: `apk/PickleScore-1.3.0.apk` (non-debuggable release).
- Package: `com.example.pickleballscoreboard`; version name `1.3.0`; version code `5` (1.2.0 was `3`).
- APK SHA-256: `d59a33036bd71316e74daaac74f9ed42916e896d39470a4358e828ecd018053c`.
- Signing certificate SHA-256: `548d4e27a37909013053429dd013c1b3750f2728c149c9255df3e8788407ef06` (same as archived 1.2.0, APK v2/v3 verified; local distribution, not a Play Store release).

## Runtime evidence

On an API 35 emulator, all three exact signed APKs displayed the same injected `2,3,L,2,0,D` snapshot. The screenshots and UI hierarchies show stacked controls with the inline left preview in 1.1 and 1.3, versus the large court and horizontal rally pair in 1.2. The screen was captured at 320 dp wide with 1.35× font scale. The released 1.3 also rendered a Singles snapshot and displayed `PickleScore 1.3.0` in **Board**. See `screens/comparison-1.1-1.2-1.3.png`, `screens/1.3.0-doubles.png`, `screens/1.3.0-singles.png`, and `screens/1.3.0-version.png`.

The 1.2 → 1.3 install used `adb install -r` without uninstalling or clearing data; app UID, first-install time, a private test file, and both granted Nearby Devices permissions were retained. The APK pulled from the installed package matched the delivered APK byte-for-byte. Virtual BLE exercised the legacy and extended Notify paths through Android's stack without scoring writes. 51 JVM tests and five Compose emulator tests passed, covering layout, exact pixels, control roles and target sizes, confirmations, Undo, reconnect gating and version visibility. Lint and release builds passed. No physical board or phone was contacted.

## Install on the phone

Transfer `PickleScore-1.3.0.apk` to the phone, open it, and choose **Update**. If needed, temporarily allow APK installation from the app used to open the file. Do not uninstall first. Open **Board** to confirm `PickleScore 1.3.0`. With authorized ADB, the equivalent is `adb -s <phone-serial> install -r <absolute-path-to-PickleScore-1.3.0.apk>`.
