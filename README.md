# Pokémon GO Quick Catch + Enhanced Throw

A pinned ARM64 runtime patch for **Pokémon GO 0.433.1**. It keeps the existing RPC metadata probe and adds Quick Catch and Enhanced Throw based on the MIT-licensed PGRazor implementation.

## Features

- **Quick Catch:** speeds up the ordinary Poké Ball catch animation and automatically dismisses the normal post-catch summary/details sequence after a confirmed successful catch. New-species/form cutscenes remain under the game's normal flow.
- **Enhanced Throw:** sets the throw reticle to **Excellent** and enables the curveball flag when a supported ordinary throw is submitted or hit.
- **Diagnostics:** retains the read-only RPC metadata probe. Runtime events use the PGORPCLOADER, PGORPCFEATURE, and PGORPCPROBE log tags.

There is no settings UI in this first prototype. Both gameplay features are enabled automatically when the patch starts; auto-transfer is explicitly disabled. The patch is disabled by default in the Morphe patch manager.

## Compatibility and limitations

Only **Pokémon GO 0.433.1, ARM64** is declared compatible. PGRazor's IL2CPP class layouts, method RVAs, and instruction fingerprints are version-specific; other versions are intentionally not targeted. Source integration and CI tests do not replace an install/launch/catch smoke test on a real device. The existing read-only probe is useful if a target method fails validation.

## Build on Fedora/Linux

Install Java 21, Android SDK/NDK, CMake, Python 3, Node.js 22, and Android platform tools. Configure the SDK:

~~~sh
export ANDROID_HOME="$HOME/Android/Sdk"
export ANDROID_NDK_HOME="$ANDROID_HOME/ndk/27.2.12479018"
./native/build_android.sh
./gradlew :patches:buildAndroid --no-daemon
~~~

The native build downloads the pinned Frida Gadget 17.24.2 ARM64 release asset and verifies the SHA-256 digest published by the official GitHub release API, then verifies its ELF architecture. The downloaded library is ignored by Git and embedded as an APK asset by the patch.

GitHub Actions runs the Quick Ball test suite and JavaScript syntax checks, builds the native ARM64 bootstrap, and produces the Morphe .mpp patch bundle as a seven-day artifact. The bundle patches a clean official APK; it is not a standalone installable APK.

## Test on device

1. Patch a **clean, official Pokémon GO 0.433.1 ARM64 APK** and select only **Quick Catch + Enhanced Throw**.
2. Install the patched APK and launch to the map. The first start extracts the runtime to the app-private files directory and initializes Frida after application startup.
3. Capture filtered logs with USB debugging:

   ~~~sh
   adb logcat -c
   adb logcat -s PGORPCLOADER:I PGORPCFEATURE:I PGORPCPROBE:I '*:S'
   ~~~

4. Confirm the message "Frida Gadget initialized", then look for the events "metadata-ready", "speed-applied", and "throw-applied". If any component is unavailable, share only filtered error/event lines.

## Third-party notices

The adapted PGRazor JavaScript routines and test harness are licensed under MIT; see PGRazor-LICENSE.txt in the patch resources. The Frida Gadget binary is downloaded from the official Frida 17.24.2 release and its corresponding COPYING notice is bundled as FRIDA-COPYING.txt in the APK assets. The main project remains GPL-3.0; see LICENSE and NOTICE.
