#!/bin/sh
set -eu
root="$(cd "$(dirname "$0")/../../.." && pwd)"
build="${TMPDIR:-/tmp}/arisa_dynamic_type"
mkdir -p "$build"
device="$(xcrun simctl list devices available | grep -m1 -E 'iPhone' | sed -E 's/.*\(([0-9A-F-]{36})\).*/\1/')"
nice -n 10 xcrun --sdk iphonesimulator swiftc -O -target arm64-apple-ios27.0-simulator -o "$build/dynamic_type" "$root/tools/swiftui_reference/dynamic_type/DynamicType.swift"
booted=0
if ! xcrun simctl list devices booted | grep -q "$device"; then
    xcrun simctl boot "$device"
    booted=1
fi
xcrun simctl spawn "$device" "$build/dynamic_type" > "$root/tests/fixtures/swiftui_reference/dynamic_type.tsv"
if [ "$booted" -eq 1 ]; then
    xcrun simctl shutdown "$device"
fi
