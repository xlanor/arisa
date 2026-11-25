#!/bin/sh
set -eu
root="$(cd "$(dirname "$0")/../../.." && pwd)"
here="$root/tools/swiftui_reference/ios"
build="${TMPDIR:-/tmp}/arisa_glass_reference"
shots="$build/shots"
out="$root/tests/fixtures/swiftui_reference"
device="${ARISA_GLASS_DEVICE:-iPhone 17}"
bundle=dev.arisa.glassreference
mkdir -p "$build/GlassReference.app" "$shots" "$out"
nice -n 10 xcrun -sdk iphonesimulator swiftc -O -parse-as-library -target arm64-apple-ios27.0-simulator \
    -o "$build/GlassReference.app/GlassReference" "$here/GlassGeometry.swift" "$here/GlassApp.swift" "$here/PhotoPlan.swift"
cp "$here/Info.plist" "$build/GlassReference.app/Info.plist"
codesign -s - --force "$build/GlassReference.app" > /dev/null 2>&1
nice -n 10 swiftc -O -parse-as-library -o "$build/glass_measure" "$here/GlassGeometry.swift" "$here/GlassMeasure.swift"
udid="$(xcrun simctl list devices available | grep "    $device (" | head -1 | sed 's/.*(\([0-9A-F-]*\)).*/\1/')"
xcrun simctl boot "$udid" 2> /dev/null || true
xcrun simctl bootstatus "$udid" -b > /dev/null
xcrun simctl status_bar "$udid" override --time 9:41 > /dev/null 2>&1 || true
xcrun simctl install "$udid" "$build/GlassReference.app"
shoot() {
    xcrun simctl launch --terminate-running-process "$udid" "$bundle" -layout "$1" -backdrop "$2" -variant "$3" -scheme "$4" > /dev/null
    sleep "${ARISA_GLASS_SETTLE:-3}"
    xcrun simctl io "$udid" screenshot "$shots/$1_$2_$3_$4.png" > /dev/null 2>&1
}
for scheme in light dark; do
    for backdrop in photo ramp_x ramp_y grey black white stripes stripes_wide; do
        for variant in none regular clear tinted tinted_soft interactive; do
            shoot shapes "$backdrop" "$variant" "$scheme"
        done
    done
    for variant in regular clear tinted tinted_soft; do
        shoot sheet photo "$variant" "$scheme"
    done
    shoot system_sheet photo none "$scheme"
done
xcrun simctl terminate "$udid" "$bundle" > /dev/null 2>&1 || true
xcrun simctl shutdown "$udid" > /dev/null 2>&1 || true
runtime="$(xcrun simctl list runtimes | grep -m1 iOS | sed 's/ (.*//')"
"$build/glass_measure" "$shots" "$out" "$runtime" "$device"
