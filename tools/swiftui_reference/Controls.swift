import AppKit
import SwiftUI

enum Controls {
    static let sizes: [(String, ControlSize)] = [("mini", .mini), ("small", .small), ("regular", .regular), ("large", .large), ("extra_large", .extraLarge)]

    static func run(_ store: Store) {
        for scheme in Scheme.allCases {
            buttons(store, scheme)
            toggles(store, scheme)
            sliders(store, scheme)
            steppers(store, scheme)
            pickers(store, scheme)
            menus(store, scheme)
            progress(store, scheme)
            fields(store, scheme)
            focus(store, scheme)
        }
    }

    static func styled<V: View>(_ view: V, _ style: String) -> AnyView {
        switch style {
        case "bordered": return AnyView(view.buttonStyle(.bordered))
        case "bordered_prominent": return AnyView(view.buttonStyle(.borderedProminent))
        case "borderless": return AnyView(view.buttonStyle(.borderless))
        case "plain": return AnyView(view.buttonStyle(.plain))
        default: return AnyView(view.buttonStyle(.automatic))
        }
    }

    static func buttons(_ store: Store, _ scheme: Scheme) {
        for style in ["automatic", "bordered", "bordered_prominent", "borderless", "plain"] {
            for (size, control) in sizes {
                let view = styled(Button {} label: { Text("Button").probe("label") }, style).controlSize(control).probe("control")
                let shot = Harness.capture(view, scheme: scheme)
                Measure.record(store, "button.\(style).\(size)", shot, radius: style != "plain" && style != "borderless", image: size == "regular" || size == "large" ? "button_\(style)_\(size)" : nil)
            }
        }
        for role in ["destructive", "cancel"] {
            let r: ButtonRole = role == "destructive" ? .destructive : .cancel
            for style in ["automatic", "bordered_prominent", "borderless"] {
                let view = styled(Button(role: r) {} label: { Text("Button").probe("label") }, style).probe("control")
                let shot = Harness.capture(view, scheme: scheme)
                Measure.record(store, "button.role_\(role).\(style)", shot, radius: style != "borderless", image: "button_\(role)_\(style)")
            }
        }
        let disabled = Button {} label: { Text("Button").probe("label") }.buttonStyle(.borderedProminent).disabled(true).probe("control")
        Measure.record(store, "button.disabled.bordered_prominent", Harness.capture(disabled, scheme: scheme), image: "button_disabled")
        let icon = Button {} label: { Label("Add", systemImage: "plus") }.buttonStyle(.bordered).probe("control")
        Measure.record(store, "button.label.bordered", Harness.capture(icon, scheme: scheme), label: nil)
    }

    static func toggles(_ store: Store, _ scheme: Scheme) {
        for (size, control) in sizes {
            for on in [true, false] {
                let state = on ? "on" : "off"
                let sw = Toggle("Toggle", isOn: .constant(on)).toggleStyle(.switch).labelsHidden().controlSize(control).probe("control")
                let shot = Harness.capture(sw, scheme: scheme)
                let edge = shot.frame("control")
                Measure.record(store, "toggle.switch.\(size).\(state)", shot, label: nil, fill: CGPoint(x: on ? edge.minX + edge.height * 0.4 : edge.maxX - edge.height * 0.4, y: edge.midY), image: size == "regular" ? "toggle_switch_\(state)" : nil)
                let box = shot.frame("control")
                store.colour("toggle.switch.\(size).\(state).track", scheme, shot.raster.sampleFlat(CGPoint(x: on ? box.minX + box.height * 0.45 : box.maxX - box.height * 0.45, y: box.midY)))
                let knobCentre = CGPoint(x: on ? box.maxX - box.height * 0.7 : box.minX + box.height * 0.7, y: box.midY)
                let knob = shot.raster.sampleFlat(knobCentre)
                store.colour("toggle.switch.\(size).\(state).knob", scheme, knob)
                let knobBox = shot.raster.span(from: knobCentre, tolerance: 0.06, limit: box.width)
                store.put("toggle.switch.\(size).\(state).knob_width", scheme, knobBox.width)
                store.put("toggle.switch.\(size).\(state).knob_height", scheme, knobBox.height)
                store.put("toggle.switch.\(size).\(state).knob_inset", scheme, on ? box.maxX - knobBox.maxX : knobBox.minX - box.minX)
                let check = Toggle("Toggle", isOn: .constant(on)).toggleStyle(.checkbox).labelsHidden().controlSize(control).probe("control")
                Measure.record(store, "toggle.checkbox.\(size).\(state)", Harness.capture(check, scheme: scheme), label: nil, fill: nil, image: size == "regular" ? "toggle_checkbox_\(state)" : nil)
                let button = Toggle(isOn: .constant(on)) { Text("Button").probe("label") }.toggleStyle(.button).controlSize(control).probe("control")
                Measure.record(store, "toggle.button.\(size).\(state)", Harness.capture(button, scheme: scheme), image: size == "regular" ? "toggle_button_\(state)" : nil)
            }
        }
        let labelled = HStack { Toggle(isOn: .constant(true)) { Text("Wi-Fi").probe("label") }.toggleStyle(.switch).probe("control") }
        let shot = Harness.capture(labelled, scheme: scheme)
        store.put("toggle.switch.labelled.width", scheme, shot.frame("control").width)
        store.put("toggle.switch.labelled.label_gap", scheme, shot.frame("control").maxX - shot.frame("label").maxX - (store.rows.first { $0.key == "toggle.switch.regular.on.width" }?.values[scheme]).flatMap(Double.init).map { CGFloat($0) }!)
        let checked = Toggle(isOn: .constant(true)) { Text("Remember").probe("label") }.toggleStyle(.checkbox).probe("control")
        let cs = Harness.capture(checked, scheme: scheme)
        store.put("toggle.checkbox.labelled.label_leading", scheme, cs.frame("label").minX - cs.frame("control").minX)
        store.image("toggle_checkbox_labelled", cs, cs.frame("control"))
    }

    static func sliders(_ store: Store, _ scheme: Scheme) {
        for (size, control) in sizes {
            let view = Slider(value: .constant(0.5)).frame(width: 200).controlSize(control).probe("control")
            let shot = Harness.capture(view, scheme: scheme)
            let box = shot.frame("control")
            store.put("slider.\(size).height", scheme, box.height)
            store.put("slider.\(size).track_thickness", scheme, shot.raster.relativeRun(column: box.minX + box.width * 0.85, from: box.minY, to: box.maxY))
            store.put("slider.\(size).knob_height", scheme, shot.raster.runLength(column: box.midX, from: box.minY - 4, to: box.maxY + 4, threshold: 0.5))
            let trackRow = box.midY
            var knobWidth = 0.0
            let knobColour = shot.raster.sampleFlat(CGPoint(x: box.midX, y: trackRow))
            let s = shot.raster.scale
            var x = box.midX
            while x > box.minX, shot.raster.flat(shot.raster.px(x), shot.raster.px(trackRow)).distance(knobColour) < 0.04 { x -= 0.5 }
            var y = box.midX
            while y < box.maxX, shot.raster.flat(shot.raster.px(y), shot.raster.px(trackRow)).distance(knobColour) < 0.04 { y += 0.5 }
            knobWidth = Double(y - x)
            _ = s
            store.put("slider.\(size).knob_width", scheme, knobWidth)
            store.colour("slider.\(size).fill", scheme, shot.raster.sampleFlat(CGPoint(x: box.minX + box.width * 0.15, y: trackRow)))
            store.colour("slider.\(size).track", scheme, shot.raster.sampleFlat(CGPoint(x: box.minX + box.width * 0.85, y: trackRow)))
            store.colour("slider.\(size).knob", scheme, knobColour)
            if size == "regular" { store.image("slider", shot, box) }
        }
    }

    static func steppers(_ store: Store, _ scheme: Scheme) {
        for (size, control) in sizes {
            let view = Stepper("Stepper", value: .constant(3)).labelsHidden().controlSize(control).probe("control")
            let shot = Harness.capture(view, scheme: scheme)
            Measure.record(store, "stepper.\(size)", shot, label: nil, image: size == "regular" ? "stepper" : nil)
        }
        let labelled = Stepper { Text("Quantity").probe("label") } onIncrement: {} onDecrement: {}.probe("control")
        let shot = Harness.capture(labelled, scheme: scheme)
        store.put("stepper.labelled.width", scheme, shot.frame("control").width)
        store.put("stepper.labelled.label_width", scheme, shot.frame("label").width)
        store.image("stepper_labelled", shot, shot.frame("control"))
    }

    static func pickers(_ store: Store, _ scheme: Scheme) {
        for (size, control) in sizes {
            let segmented = Picker("Picker", selection: .constant(1)) { Text("One").tag(0); Text("Two").tag(1); Text("Three").tag(2) }
                .pickerStyle(.segmented).labelsHidden().controlSize(control).probe("control")
            let shot = Harness.capture(segmented, scheme: scheme)
            let edge = shot.frame("control")
            Measure.record(store, "picker.segmented.\(size)", shot, label: nil, fill: CGPoint(x: edge.minX + 4, y: edge.midY), image: size == "regular" ? "picker_segmented" : nil)
            let box = shot.frame("control")
            store.colour("picker.segmented.\(size).selected", scheme, shot.raster.sampleFlat(CGPoint(x: box.midX, y: box.minY + 3)))
            let menu = Picker("Picker", selection: .constant(1)) { Text("One").tag(0); Text("Two").tag(1) }
                .pickerStyle(.menu).labelsHidden().controlSize(control).probe("control")
            Measure.record(store, "picker.menu.\(size)", Harness.capture(menu, scheme: scheme), label: nil, image: size == "regular" ? "picker_menu" : nil)
        }
        let radio = Picker("Picker", selection: .constant(1)) { Text("One").tag(0); Text("Two").tag(1); Text("Three").tag(2) }
            .pickerStyle(.radioGroup).labelsHidden().probe("control")
        let rs = Harness.capture(radio, scheme: scheme)
        store.put("picker.radio_group.width", scheme, rs.frame("control").width)
        store.put("picker.radio_group.height", scheme, rs.frame("control").height)
        store.put("picker.radio_group.row_pitch", scheme, rs.frame("control").height / 3)
        store.image("picker_radio_group", rs, rs.frame("control"))
        let inline = Picker("Picker", selection: .constant(1)) { Text("One").tag(0); Text("Two").tag(1); Text("Three").tag(2) }
            .pickerStyle(.inline).labelsHidden().probe("control")
        let ins = Harness.capture(inline, scheme: scheme)
        store.put("picker.inline.width", scheme, ins.frame("control").width)
        store.put("picker.inline.height", scheme, ins.frame("control").height)
        store.image("picker_inline", ins, ins.frame("control"))
        let labelled = Picker(selection: .constant(1)) { Text("One").tag(0); Text("Two").tag(1) } label: { Text("Flavour").probe("label") }.pickerStyle(.menu).probe("control")
        let ls = Harness.capture(labelled, scheme: scheme)
        store.put("picker.menu.labelled.label_gap", scheme, ls.frame("control").width - ls.frame("label").width - (Double(store.rows.first { $0.key == "picker.menu.regular.width" }?.values[scheme] ?? "0") ?? 0))
    }

    static func menus(_ store: Store, _ scheme: Scheme) {
        for (size, control) in sizes {
            let view = Menu { Button("One") {}; Button("Two") {} } label: { Text("Button").probe("label") }.fixedSize().controlSize(control).probe("control")
            Measure.record(store, "menu.\(size)", Harness.capture(view, scheme: scheme), image: size == "regular" ? "menu" : nil)
        }
        let borderless = Menu { Button("One") {} } label: { Text("Button").probe("label") }.menuStyle(.borderlessButton).fixedSize().probe("control")
        Measure.record(store, "menu.borderless", Harness.capture(borderless, scheme: scheme), radius: false, image: "menu_borderless")
    }

    static func progress(_ store: Store, _ scheme: Scheme) {
        for (size, control) in sizes {
            let circular = ProgressView().progressViewStyle(.circular).controlSize(control).probe("control")
            let cs = Harness.capture(circular, scheme: scheme)
            store.put("progress.circular.\(size).width", scheme, cs.frame("control").width)
            store.put("progress.circular.\(size).height", scheme, cs.frame("control").height)
            if size == "regular" { store.image("progress_circular", cs, cs.frame("control")) }
            let linear = ProgressView(value: 0.4).progressViewStyle(.linear).frame(width: 200).controlSize(control).probe("control")
            let ls = Harness.capture(linear, scheme: scheme)
            let box = ls.frame("control")
            store.put("progress.linear.\(size).height", scheme, box.height)
            store.put("progress.linear.\(size).bar_thickness", scheme, ls.raster.relativeRun(column: box.minX + box.width * 0.2, from: box.minY - 2, to: box.maxY + 2))
            if let bar = ls.raster.opaqueBounds(in: box.insetBy(dx: -2, dy: -2), threshold: max(0.02, ls.raster.sample(CGPoint(x: box.minX + box.width * 0.8, y: box.midY)).a * 0.5)) {
                store.put("progress.linear.\(size).radius", scheme, ls.raster.alphaRadius(of: bar))
                store.colour("progress.linear.\(size).fill", scheme, ls.raster.sampleFlat(CGPoint(x: box.minX + box.width * 0.2, y: bar.midY)))
                store.colour("progress.linear.\(size).track", scheme, ls.raster.sampleFlat(CGPoint(x: box.minX + box.width * 0.8, y: bar.midY)))
            }
            if size == "regular" { store.image("progress_linear", ls, box) }
        }
        let labelled = ProgressView(value: 0.4) { Text("Downloading").probe("label") }.frame(width: 200).probe("control")
        let ls = Harness.capture(labelled, scheme: scheme)
        store.put("progress.linear.labelled.height", scheme, ls.frame("control").height)
        store.put("progress.linear.labelled.label_height", scheme, ls.frame("label").height)
    }

    static func fields(_ store: Store, _ scheme: Scheme) {
        for (size, control) in sizes {
            for style in ["rounded_border", "plain", "automatic"] {
                let field: AnyView
                switch style {
                case "rounded_border": field = AnyView(TextField("Placeholder", text: .constant("")).textFieldStyle(.roundedBorder))
                case "plain": field = AnyView(TextField("Placeholder", text: .constant("")).textFieldStyle(.plain))
                default: field = AnyView(TextField("Placeholder", text: .constant("")))
                }
                let view = field.frame(width: 200).controlSize(control).probe("control")
                let shot = Harness.capture(view, scheme: scheme)
                Measure.record(store, "text_field.\(style).\(size)", shot, label: nil, radius: style != "plain", image: size == "regular" ? "text_field_\(style)" : nil)
                let box = shot.frame("control")
                store.colour("text_field.\(style).\(size).placeholder", scheme, shot.raster.extreme(in: box.insetBy(dx: 3, dy: 3), from: shot.raster.sampleFlat(CGPoint(x: box.maxX - 6, y: box.midY))))
                if let ink = shot.raster.differingBounds(in: CGRect(x: box.minX + 1, y: box.minY + 3, width: box.width / 2, height: box.height - 6), from: shot.raster.sampleFlat(CGPoint(x: box.maxX - 6, y: box.midY)), tolerance: 0.15) {
                    store.put("text_field.\(style).\(size).text_inset", scheme, ink.minX - box.minX)
                }
            }
        }
        let secure = SecureField("Password", text: .constant("")).textFieldStyle(.roundedBorder).frame(width: 200).probe("control")
        Measure.record(store, "secure_field.rounded_border.regular", Harness.capture(secure, scheme: scheme), label: nil, image: "secure_field")
        let labelled = TextField(text: .constant("")) { Text("Name") }.frame(width: 200).probe("control")
        let shot = Harness.capture(Form { labelled }.formStyle(.columns), scheme: scheme)
        store.put("text_field.in_columns_form.height", scheme, shot.frame("control").height)
    }

    static func focus(_ store: Store, _ scheme: Scheme) {
        let size = CGSize(width: 120, height: 60)
        let view = FocusRingView(frame: NSRect(origin: .zero, size: size))
        let raster = Harness.render(view: view, scheme: scheme, size: size)
        let inner = CGRect(x: 30, y: 15, width: 60, height: 30)
        if let ring = raster.opaqueBounds(in: inner.insetBy(dx: -10, dy: -10), threshold: 0.1) {
            store.put("focus_ring.outset", scheme, inner.minX - ring.minX)
            store.put("focus_ring.outset_bottom", scheme, ring.maxY - inner.maxY)
        }
        var width = 0.0
        var x = inner.minX - 8
        var peak = RGBA(r: 0, g: 0, b: 0, a: 0)
        while x < inner.minX + 8 {
            let c = raster.sample(CGPoint(x: x, y: inner.midY))
            if c.a > 0.25 { width += 0.5 }
            if c.a > peak.a { peak = c }
            x += 0.5
        }
        store.put("focus_ring.width", scheme, width)
        store.colour("focus_ring.colour", scheme, peak)
        store.colour("focus_ring.indicator_colour", scheme, RGBA.of(.keyboardFocusIndicatorColor, appearance: scheme.appearance))
        let ringRect = CGRect(x: inner.minX - 5, y: inner.minY - 5, width: inner.width + 10, height: inner.height + 10)
        store.put("focus_ring.outer_radius", scheme, raster.cornerRadius(of: raster.opaqueBounds(in: ringRect, threshold: 0.25) ?? ringRect) { min(1, raster.alpha($0, $1) / max(peak.a, 0.01)) })
        store.put("focus_ring.shape_radius", nil, 6)
        let shot = Shot(raster: raster, frames: [:], size: size, scheme: scheme)
        store.image("focus_ring", shot, inner.insetBy(dx: -8, dy: -8), pad: 0)
    }
}

final class FocusRingView: NSView {
    override var isFlipped: Bool { true }

    override func draw(_ dirtyRect: NSRect) {
        let path = NSBezierPath(roundedRect: NSRect(x: 30, y: 15, width: 60, height: 30), xRadius: 6, yRadius: 6)
        NSGraphicsContext.saveGraphicsState()
        NSFocusRingPlacement.only.set()
        path.fill()
        NSGraphicsContext.restoreGraphicsState()
    }
}
