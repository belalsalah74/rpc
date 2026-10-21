adb shell pm path com.nianticlabs.pgs | head -1 | sed 's/package://; s#/base.apk##' | tr -d '\r'
adb shell ls -l "$P/lib/arm64/"
adb shell dumpsys package com.nianticlabs.pgs | grep -E "versionName|lastUpdateTime"



adb logcat -G 16M
adb logcat -c
adb shell am force-stop com.nianticlabs.pokemongo
adb logcat -v time | grep --line-buffered -iE "PGOHOOK|PogoWebSignIn|libpgo_hook|libmain_orig" | tee hook.txt


