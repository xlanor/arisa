import pathlib
import sys

TITLES = {
    "text": "Text styles",
    "stack": "Stacks",
    "padding": "Padding",
    "spacer": "Spacer",
    "divider": "Divider",
    "symbol": "Image(systemName:) sizing",
    "label": "Label",
    "shadow": "Shadows",
    "shape": "Shapes (estimator calibration)",
    "button": "Button",
    "toggle": "Toggle",
    "slider": "Slider",
    "stepper": "Stepper",
    "picker": "Picker",
    "menu": "Menu",
    "progress": "ProgressView",
    "text_field": "TextField",
    "secure_field": "SecureField",
    "focus_ring": "Focus ring",
    "list": "List",
    "form": "Form",
    "sidebar": "NavigationSplitView sidebar",
    "scroll_indicator": "Scroll indicators",
    "navigation": "NavigationStack and toolbar",
    "presentation": "Alerts, dialogs and sheets",
    "popover": "Popover",
    "scrim": "Scrim",
    "appkit": "AppKit control metrics",
    "motion": "Motion",
    "accent": "Accent usage by container",
    "glass": "Liquid Glass",
    "chrome": "Tahoe window chrome",
    "path": "Paths and MeshGradient",
    "text_fit": "Text and image fit",
}

KEEP = "## arisa comparison"


def load(path):
    header = []
    rows = []
    for line in pathlib.Path(path).read_text().splitlines():
        if line.startswith("#\t"):
            header.append(line[2:])
            continue
        parts = line.split("\t")
        if parts[0] == "key" or len(parts) < 4:
            continue
        rows.append(parts)
    return header, rows


def table(rows):
    lines = ["| key | light | dark | unit |", "|---|---|---|---|"]
    for key, light, dark, unit in rows:
        lines.append(f"| `{key}` | {light} | {dark} | {unit} |")
    return lines


def motion(rows):
    samples = {}
    other = []
    for row in rows:
        key = row[0]
        if ".sample." in key:
            series, time = key.rsplit(".sample.", 1)
            samples.setdefault(series[len("motion."):], {})[int(time)] = row[1]
        else:
            other.append(row)
    lines = table(other)
    names = list(samples)
    times = sorted({t for series in samples.values() for t in series})
    lines += ["", "Progress toward the target (0 to 1) at each time, from `Spring.value(target:time:)` (`.spring_value`) and from `Animation.animate(value:time:context:)` (`.animation`), sampled every 20 ms.", ""]
    lines.append("| t (ms) | " + " | ".join(f"`{n}`" for n in names) + " |")
    lines.append("|---|" + "---|" * len(names))
    for t in times:
        lines.append(f"| {t} | " + " | ".join(samples[n].get(t, "") for n in names) + " |")
    return lines


def main():
    source, target = sys.argv[1], pathlib.Path(sys.argv[2])
    header, rows = load(source)
    kept = ""
    if target.exists():
        text = target.read_text()
        if KEEP in text:
            kept = text[text.index(KEEP):]
    groups = {}
    for row in rows:
        groups.setdefault(row[0].split(".", 1)[0], []).append(row)
    out = [
        "# SwiftUI reference metrics",
        "",
        "Every number below was measured live on this Mac by `tools/swiftui_reference/run.sh`: each SwiftUI view is hosted offscreen in an `NSHostingView` inside a borderless window that is never ordered in (subclassed to report key and active appearance so controls draw in their active state), laid out, and rendered with `cacheDisplay` at 2x into an sRGB bitmap. Frames come from `GeometryReader` probes in a named coordinate space; where SwiftUI hosts content in AppKit tables (List, Form, sidebar) the probes do not report, so positions are measured from rendered ink instead (`ink.*` keys: leading edge, cap top and baseline of each text line). Radii come from the rendered alpha (or contrast against the surrounding colour) along the corner diagonal, reported as the equivalent circular radius; the estimator reads 11.95 for a 12 pt circular corner (see Shapes). Colours are sampled from the render and composited over `windowBackgroundColor` unless the key ends in `_alpha`. Motion comes from the public `Spring` API and from SwiftUI's own `Animation.animate(value:time:context:)`, called from inside a custom animation so the context is real. Reference PNGs are in `tests/fixtures/swiftui_reference/`.",
        "",
        "Captured:",
        "",
    ]
    out += [f"- {line}" for line in header]
    out += ["", "Known limits of offscreen capture: Liquid Glass materials that depend on a backdrop (the sidebar selection capsule, its dark variant) render black, so the sidebar selection colour is taken from `NSColor.selectedContentBackgroundColor` / `unemphasizedSelectedContentBackgroundColor`; window chrome (title bar, toolbar, sheet and popover frames, the popover arrow, window corner radius) is drawn by the window server and cannot be captured without ordering a window on screen, so only AppKit's geometry for it is recorded. macOS presents sheets and alerts without a scrim (opacity 0). An offscreen window never lays out its toolbar, so toolbar buttons are measured as `NSButton` with the toolbar bezel; an `NSAlert` window is never key offscreen, so its default button renders grey, and the default-action colour is measured from a SwiftUI button with `keyboardShortcut(.defaultAction)` in a key window instead.", ""]
    for prefix, title in TITLES.items():
        if prefix not in groups:
            continue
        out += [f"## {title}", ""]
        out += motion(groups[prefix]) if prefix == "motion" else table(groups[prefix])
        out.append("")
    leftovers = [p for p in groups if p not in TITLES]
    for prefix in leftovers:
        out += [f"## {prefix}", ""] + table(groups[prefix]) + [""]
    text = "\n".join(out)
    if kept:
        text += "\n" + kept
    target.write_text(text if text.endswith("\n") else text + "\n")


main()
