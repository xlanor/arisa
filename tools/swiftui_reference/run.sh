#!/bin/sh
set -eu
root="$(cd "$(dirname "$0")/../.." && pwd)"
build="${TMPDIR:-/tmp}/arisa_swiftui_reference"
mkdir -p "$build"
nice -n 10 swiftc -O -swift-version 5 -o "$build/swiftui_reference" "$root"/tools/swiftui_reference/*.swift 2>&1 | grep -v "warning:" || true
out="$root/tests/fixtures/swiftui_reference"
if [ "$#" -eq 0 ]; then
    rm -f "$out"/*.png "$out"/metrics*.tsv
fi
nice -n 10 "$build/swiftui_reference" "$out" "$@"
if [ "$#" -eq 0 ]; then
    python3 "$root/tools/swiftui_reference/render_doc.py" "$out/metrics.tsv" "$root/docs/swiftui-reference.md"
fi
