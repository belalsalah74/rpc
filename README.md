# Pokémon GO RPC Runtime Probe

A clean Morphe patch project initialized from the [official Morphe patch template](https://github.com/MorpheApp/morphe-patches-template). This repository contains **one patch only**: RPC runtime probe for Pokémon GO 0.433.1 / 0.431.0 ARM64.

## Patch

**RPC runtime probe** collects read-only IL2CPP metadata for selected RPC-related classes: method signatures, code RVAs, field offsets, and selected static references. It does not invoke managed methods or send game RPC requests. The patch is disabled by default.

This is a diagnostic prototype, not a working authenticated `GameSession` adapter for Shiny Atlas. Runtime logs must be checked on a device before deciding the next adapter changes.

## Build

On Fedora/Linux, install Java 21, Android SDK/NDK, CMake, and Android platform tools, then configure the SDK:

```sh
export ANDROID_HOME="$HOME/Android/Sdk"
export ANDROID_NDK_HOME="$ANDROID_HOME/ndk/27.2.12479018"
./native/build_android.sh
./gradlew :patches:buildAndroid --no-daemon
```

GitHub Actions builds the ARM64 native payloads and the Morphe `.mpp` bundle, then uploads the bundle as a seven-day artifact.

## Test on device

1. Patch a **clean, official Pokémon GO 0.433.1 (or 0.431.0) ARM64 APK** and select only **RPC runtime probe**.
2. Do not use an APK that already has `libmain_orig.so`.
3. Launch the game to the map screen.
4. With USB debugging enabled on a computer, capture:

   ```sh
   adb logcat -c
   adb logcat -s PGORPCPROBE:I PGORPCLOADER:I '*:S'
   ```
   The same lines are also written to `/data/data/com.nianticlabs.pokemongo/files/rpc_probe.txt` (logcat drops lines under heavy output). Pull it with `adb shell run-as` (not possible on a release build) or from Termux with root/Shizuku: `rish -c "cat /data/data/com.nianticlabs.pokemongo/files/rpc_probe.txt"`.

5. Share only the filtered `PGORPCPROBE` lines around `CLASS`, `CLASS_CANDIDATE`, `METHOD`, `PARAM`, `FIELD`, `STATIC_REF`, `OWNER_FIELD`, and `RPC metadata probe complete`. Do not share credentials or unrelated logs.

## Repository

https://github.com/belalsalah74/rpc

## License

GPL-3.0; see [LICENSE](LICENSE) and [NOTICE](NOTICE).
