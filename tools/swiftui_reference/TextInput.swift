import AppKit
import SwiftUI

enum TextInput {
    static func run(_ store: Store) {
        for scheme in Scheme.allCases {
            sliderLabels(store, scheme)
            prompt(store, scheme)
            vertical(store, scheme)
            editor(store, scheme)
            fieldStyles(store, scheme)
            selection(store, scheme)
        }
    }

    static func fieldStyles(_ store: Store, _ scheme: Scheme) {
        let cases: [(String, AnyView)] = [
            ("bordered", AnyView(TextField("Name", text: .constant("Text")).textFieldStyle(.bordered))),
            ("bordered_rounded", AnyView(TextField("Name", text: .constant("Text")).textFieldStyle(.bordered).textInputBorderShape(.roundedRectangle))),
            ("bordered_capsule", AnyView(TextField("Name", text: .constant("Text")).textFieldStyle(.bordered).textInputBorderShape(.capsule))),
            ("square_border", AnyView(TextField("Name", text: .constant("Text")).textFieldStyle(.squareBorder))),
        ]
        for (name, view) in cases {
            let shot = Harness.capture(view.frame(width: 200).probe("control"), scheme: scheme)
            let box = shot.frame("control")
            let fill = shot.raster.sampleFlat(CGPoint(x: box.maxX - 8, y: box.midY))
            store.put("text_field.\(name).height", scheme, box.height)
            store.colour("text_field.\(name).fill", scheme, fill)
            store.colour("text_field.\(name).edge", scheme, shot.raster.sampleFlat(CGPoint(x: box.midX, y: box.minY + 0.25)))
            let ink = shot.raster.differingBounds(in: box.insetBy(dx: -2, dy: -2), from: shot.raster.background, tolerance: 0.04) ?? box
            store.put("text_field.\(name).bezel_height", scheme, ink.height)
            store.put("text_field.\(name).radius", scheme, shot.raster.contrastRadius(of: ink, fill: shot.raster.sampleFlat(CGPoint(x: ink.minX + 0.5, y: ink.midY))))
            if let text = shot.raster.differingBounds(in: box.insetBy(dx: 2, dy: 3), from: fill, tolerance: 0.15) {
                store.put("text_field.\(name).text_leading", scheme, text.minX - box.minX)
            }
            store.image("text_field_\(name)", shot, box)
        }
    }

    static func selection(_ store: Store, _ scheme: Scheme) {
        let shot = Harness.capture(Text("Selectable words").textSelection(.enabled).frame(width: 200).probe("control"), scheme: scheme)
        store.put("text_selection.enabled_height", scheme, shot.frame("control").height)
        store.colour("text_selection.highlight", scheme, RGBA.of(.selectedTextBackgroundColor, appearance: scheme.appearance))
        store.colour("text_selection.insertion_point", scheme, RGBA.of(.textInsertionPointColor, appearance: scheme.appearance))
    }

    static func sliderLabels(_ store: Store, _ scheme: Scheme) {
        let view = Slider(value: .constant(0.5), in: 0...1) { Text("Volume") } minimumValueLabel: { Text("Low").probe("min") } maximumValueLabel: { Text("High").probe("max") }
            .labelsHidden().frame(width: 300).probe("control")
        let shot = Harness.capture(view, scheme: scheme)
        let control = shot.frame("control")
        let low = shot.frame("min")
        let high = shot.frame("max")
        let row = control.midY
        let background = shot.raster.background
        var start = control.midX - 30
        while start > control.minX, shot.raster.sampleFlat(CGPoint(x: start, y: row)).distance(background) >= 0.04 { start -= 0.5 }
        var end = control.midX + 30
        while end < control.maxX, shot.raster.sampleFlat(CGPoint(x: end, y: row)).distance(background) >= 0.04 { end += 0.5 }
        let lowInk = shot.raster.differingBounds(in: low, from: background, tolerance: 0.15) ?? low
        let highInk = shot.raster.differingBounds(in: high, from: background, tolerance: 0.15) ?? high
        store.put("slider.labels.min_frame_gap_to_track", scheme, start - low.maxX)
        store.put("slider.labels.track_frame_gap_to_max", scheme, high.minX - end)
        store.put("slider.labels.control_width", scheme, control.width)
        store.put("slider.labels.min_leading", scheme, low.minX - control.minX)
        store.put("slider.labels.max_trailing", scheme, control.maxX - high.maxX)
        store.put("slider.labels.min_ink_gap_to_track", scheme, start - lowInk.maxX)
        store.put("slider.labels.track_ink_gap_to_max", scheme, highInk.minX - end)
        store.put("slider.labels.height", scheme, control.height)
        store.image("slider_labels", shot, control)
    }

    static func prompt(_ store: Store, _ scheme: Scheme) {
        for (name, view) in [("title_only", AnyView(TextField("Name", text: .constant("")))), ("with_prompt", AnyView(TextField("Name", text: .constant(""), prompt: Text("Required"))))] {
            let shot = Harness.capture(view.textFieldStyle(.roundedBorder).frame(width: 200).probe("control"), scheme: scheme)
            let box = shot.frame("control")
            let fill = shot.raster.sampleFlat(CGPoint(x: box.maxX - 6, y: box.midY))
            if let ink = shot.raster.differingBounds(in: box.insetBy(dx: 2, dy: 3), from: fill, tolerance: 0.15) {
                store.put("text_field.\(name).placeholder_ink_width", scheme, ink.width)
            }
            store.colour("text_field.\(name).placeholder", scheme, shot.raster.extreme(in: box.insetBy(dx: 3, dy: 3), from: fill))
            store.image("text_field_\(name)", shot, box)
        }
    }

    static func vertical(_ store: Store, _ scheme: Scheme) {
        let single = Harness.capture(TextField("Notes", text: .constant("one")).textFieldStyle(.roundedBorder).frame(width: 200).probe("control"), scheme: scheme)
        let multi = Harness.capture(TextField("Notes", text: .constant("one\ntwo\nthree"), axis: .vertical).textFieldStyle(.roundedBorder).frame(width: 200).probe("control"), scheme: scheme)
        store.put("text_field.vertical.single_height", scheme, single.frame("control").height)
        store.put("text_field.vertical.three_line_height", scheme, multi.frame("control").height)
        store.image("text_field_vertical", multi, multi.frame("control"))
    }

    static func editor(_ store: Store, _ scheme: Scheme) {
        let shot = Harness.capture(TextEditor(text: .constant("First line\nSecond line")).frame(width: 200, height: 100).probe("control"), scheme: scheme)
        let box = shot.frame("control")
        let fill = shot.raster.sampleFlat(CGPoint(x: box.maxX - 6, y: box.maxY - 6))
        store.colour("text_editor.fill", scheme, fill)
        store.colour("text_editor.edge", scheme, shot.raster.sampleFlat(CGPoint(x: box.minX + 0.5, y: box.midY)))
        if let ink = shot.raster.differingBounds(in: box.insetBy(dx: 1, dy: 1), from: fill, tolerance: 0.15) {
            store.put("text_editor.text_inset_leading", scheme, ink.minX - box.minX)
            store.put("text_editor.text_inset_top", scheme, ink.minY - box.minY)
        }
        store.image("text_editor", shot, box)
    }
}
