import AppKit
import SwiftUI

let application = ActiveApplication.shared
application.setActivationPolicy(.prohibited)

let arguments = CommandLine.arguments
let output = URL(fileURLWithPath: arguments.count > 1 ? arguments[1] : "tests/fixtures/swiftui_reference")
let only = arguments.count > 2 ? Set(arguments[2].split(separator: ",").map(String.init)) : nil
let store = Store(imageDirectory: output)

let sections: [(String, (Store) -> Void)] = [
    ("typography", Typography.run),
    ("layout", Layout.run),
    ("controls", Controls.run),
    ("structure", Structure.run),
    ("presentation", Presentation.run),
    ("appkit", AppKitMetrics.run),
    ("motion", Motion.run),
    ("accent", Accent.run),
    ("glass", Glass.run),
    ("chrome", Chrome.run),
    ("paths", Paths.run),
    ("disabled", Enablement.run),
    ("blend", Motion.blend),
    ("text_input", TextInput.run),
    ("text_fit", TextFit.run),
    ("grid", Grids.run),
    ("loading", Loading.run),
    ("lists", Lists.run),
    ("scrolling", Scrolling.run),
    ("geometry", Geometry.run),
    ("dataflow", DataFlow.run),
    ("presentations", Presentations.run),
    ("navigation", Navigation.run),
    ("input", Input.run),
    ("groupings", Groupings.run),
    ("effects", Effects.run),
    ("shapes", Shapes.run),
    ("animators", Animators.run),
    ("custom_layout", CustomLayout.run),
    ("rich_text", RichText.run),
    ("localization", Localization.run),
    ("pointer", Pointer.run),
    ("tables", Tables.run),
    ("symbols", Symbols.run),
    ("formats", Formats.run),
    ("type_features", TypeFeatures.run),
    ("layout_scroll", LayoutScroll.run),
    ("canvas", Canvases.run),
    ("motion_parity", MotionParity.run),
    ("accessibility", AccessibilityParity.run),
]

for (name, run) in sections where only?.contains(name) ?? true {
    FileHandle.standardError.write("measuring \(name)\n".data(using: .utf8)!)
    run(store)
}

let version = ProcessInfo.processInfo.operatingSystemVersionString
let accent = UserDefaults.standard.object(forKey: "AppleAccentColor").map { "\($0)" } ?? "multicolor (default)"
let scale = NSScreen.main?.backingScaleFactor ?? 0
let suffix = only.map { "_" + $0.sorted().joined(separator: "_") } ?? ""
store.write(output.appendingPathComponent("metrics\(suffix).tsv"), header: [
    "os macOS \(version)",
    "accent \(accent)",
    "render_scale \(Store.format(Harness.scale))",
    "main_screen_scale \(Store.format(scale))",
])
let total = store.images.reduce(0) { $0 + $1.1 }
FileHandle.standardError.write("rows \(store.rows.count) images \(store.images.count) bytes \(total)\n".data(using: .utf8)!)
exit(0)
