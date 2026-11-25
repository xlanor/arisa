import AppKit
import SwiftUI

enum Symbols {
    static func run(_ store: Store) {
        let scheme = Scheme.light
        labels(store, scheme)
        buttons(store, scheme)
        pickers(store, scheme)
        sliders(store, scheme)
    }

    static func label(_ spacing: CGFloat?, _ reserved: CGFloat?) -> some View {
        Label {
            Text("Title").probe("title")
        } icon: {
            Image(systemName: "star").probe("icon")
        }
        .labelIconToTitleSpacingIfSet(spacing)
        .labelReservedIconWidthIfSet(reserved)
        .fixedSize()
        .probe("control")
    }

    static func labels(_ store: Store, _ scheme: Scheme) {
        for (name, spacing, reserved) in [("default", nil, nil), ("spacing12", CGFloat(12), nil), ("reserved40", nil, CGFloat(40))] as [(String, CGFloat?, CGFloat?)] {
            let shot = Harness.capture(label(spacing, reserved), scheme: scheme)
            let control = shot.frame("control")
            store.put("label.\(name).icon_to_title", scheme, shot.frame("title").minX - shot.frame("icon").maxX)
            store.put("label.\(name).title_x", scheme, shot.frame("title").minX - control.minX)
            store.put("label.\(name).icon_width", scheme, shot.frame("icon").width)
            store.put("label.\(name).width", scheme, control.width)
        }
    }

    static func button(_ style: String) -> AnyView {
        let base = Button {} label: { Text("Filter").probe("label") }
        switch style {
        case "accessory_bar":
            return AnyView(base.buttonStyle(.accessoryBar))
        case "accessory_bar_action":
            return AnyView(base.buttonStyle(.accessoryBarAction))
        case "link":
            return AnyView(base.buttonStyle(.link))
        default:
            return AnyView(base)
        }
    }

    static func buttons(_ store: Store, _ scheme: Scheme) {
        for style in ["automatic", "accessory_bar", "accessory_bar_action", "link"] {
            let shot = Harness.capture(button(style).fixedSize().probe("control"), scheme: scheme)
            let control = shot.frame("control")
            let label = shot.frame("label")
            store.put("button.\(style).width", scheme, control.width)
            store.put("button.\(style).height", scheme, control.height)
            store.put("button.\(style).label_x", scheme, label.minX - control.minX)
            store.put("button.\(style).label_y", scheme, label.minY - control.minY)
            store.image("symbols_button_\(style)", shot, control)
        }
    }

    static func pickers(_ store: Store, _ scheme: Scheme) {
        let segmented = Picker("Picker", selection: .constant(1)) { Text("One").tag(0); Text("Two").tag(1); Text("Three").tag(2) }
            .pickerStyle(.segmented).labelsHidden().fixedSize().probe("control")
        let ss = Harness.capture(segmented, scheme: scheme)
        store.put("picker.segmented.three.width", scheme, ss.frame("control").width)
        store.put("picker.segmented.three.height", scheme, ss.frame("control").height)
        if #available(macOS 27.0, *) {
            let tabs = Picker("Picker", selection: .constant(1)) { Text("One").tag(0); Text("Two").tag(1); Text("Three").tag(2) }
                .pickerStyle(.tabs).labelsHidden().fixedSize().probe("control")
            let ts = Harness.capture(tabs, scheme: scheme)
            store.put("picker.tabs.three.width", scheme, ts.frame("control").width)
            store.put("picker.tabs.three.height", scheme, ts.frame("control").height)
            store.image("picker_tabs", ts, ts.frame("control"))
        }
    }

    static func sliders(_ store: Store, _ scheme: Scheme) {
        let plain = Slider(value: .constant(0.5), in: 0...1).labelsHidden().frame(width: 240).probe("control")
        let ps = Harness.capture(plain, scheme: scheme)
        store.put("slider.plain.height", scheme, ps.frame("control").height)
        if #available(macOS 26.0, *) {
            let ticked = Slider(value: .constant(0.5), in: 0...1) {
                Text("Balance")
            } ticks: {
                SliderTick(0.0)
                SliderTick(0.5)
                SliderTick(1.0)
            }
            .labelsHidden().frame(width: 240).probe("control")
            let ts = Harness.capture(ticked, scheme: scheme, settle: 0.3)
            let box = ts.frame("control")
            store.put("slider.ticks.height", scheme, box.height)
            store.image("slider_ticks", ts, box)
            let labelled = Slider(value: .constant(0.5), in: 0...1) {
                Text("Balance")
            } ticks: {
                SliderTick(0.0) { Text("Left") }
                SliderTick(1.0) { Text("Right") }
            }
            .labelsHidden().frame(width: 240).probe("control")
            let ls = Harness.capture(labelled, scheme: scheme, settle: 0.3)
            store.put("slider.tick_labels.height", scheme, ls.frame("control").height)
            store.image("slider_tick_labels", ls, ls.frame("control"))
            let thumbless = Slider(value: .constant(0.5), in: 0...1).labelsHidden().sliderThumbVisibility(.hidden).frame(width: 240).probe("control")
            let hs = Harness.capture(thumbless, scheme: scheme)
            store.put("slider.thumbless.height", scheme, hs.frame("control").height)
            store.image("slider_thumbless", hs, hs.frame("control"))
        }
    }
}

extension View {
    @ViewBuilder
    func labelIconToTitleSpacingIfSet(_ value: CGFloat?) -> some View {
        if let value, #available(macOS 26.0, *) {
            labelIconToTitleSpacing(value)
        } else {
            self
        }
    }

    @ViewBuilder
    func labelReservedIconWidthIfSet(_ value: CGFloat?) -> some View {
        if let value, #available(macOS 26.0, *) {
            labelReservedIconWidth(value)
        } else {
            self
        }
    }
}
