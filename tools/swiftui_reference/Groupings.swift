import AppKit
import SwiftUI

enum Groupings {
    static let date = Date(timeIntervalSince1970: 1_763_251_200)

    static func run(_ store: Store) {
        let scheme = Scheme.light
        groupBox(store, scheme)
        gauges(store, scheme)
        controlGroup(store, scheme)
        labeled(store, scheme)
        columns(store, scheme)
        hidden(store, scheme)
        dates(store, scheme)
        colours(store, scheme)
        shapes(store, scheme)
        menus(store, scheme)
    }

    static func size(_ store: Store, _ key: String, _ scheme: Scheme, _ rect: CGRect) {
        store.put("\(key).width", scheme, rect.width)
        store.put("\(key).height", scheme, rect.height)
    }

    static func groupBox(_ store: Store, _ scheme: Scheme) {
        let titled = Harness.capture(
            GroupBox {
                Text("Content").frame(maxWidth: .infinity, alignment: .leading).probe("content")
            } label: {
                Text("Title").probe("label")
            }.probe("box").frame(width: 200),
            scheme: scheme)
        let box = titled.frame("box")
        let content = titled.frame("content")
        let label = titled.frame("label")
        size(store, "groupbox.titled", scheme, box)
        store.put("groupbox.titled.label_y", scheme, label.minY - box.minY)
        store.put("groupbox.titled.label_x", scheme, label.minX - box.minX)
        store.put("groupbox.titled.content_trailing", scheme, box.maxX - content.maxX)
        store.put("groupbox.titled.content_x", scheme, content.minX - box.minX)
        store.put("groupbox.titled.content_y", scheme, content.minY - box.minY)
        store.put("groupbox.titled.content_bottom", scheme, box.maxY - content.maxY)
        store.colour("groupbox.fill", scheme, titled.raster.sampleFlat(CGPoint(x: box.maxX - 8, y: box.maxY - 6)))
        let plain = Harness.capture(
            GroupBox {
                Text("Content").frame(maxWidth: .infinity, alignment: .leading).probe("content")
            }.probe("box").frame(width: 200),
            scheme: scheme)
        size(store, "groupbox.plain", scheme, plain.frame("box"))
        store.put("groupbox.plain.content_x", scheme, plain.frame("content").minX - plain.frame("box").minX)
        store.put("groupbox.plain.content_y", scheme, plain.frame("content").minY - plain.frame("box").minY)
    }

    static func gauges(_ store: Store, _ scheme: Scheme) {
        let styles: [(String, AnyView)] = [
            ("automatic", AnyView(Gauge(value: 0.4) { Text("Label") }.probe("gauge"))),
            ("linear_capacity", AnyView(Gauge(value: 0.4) { Text("Label") }.gaugeStyle(.linearCapacity).probe("gauge"))),
            ("accessory_linear", AnyView(Gauge(value: 0.4) { Text("Label") }.gaugeStyle(.accessoryLinear).probe("gauge"))),
            ("accessory_linear_capacity", AnyView(Gauge(value: 0.4) { Text("Label") }.gaugeStyle(.accessoryLinearCapacity).probe("gauge"))),
            ("accessory_circular", AnyView(Gauge(value: 0.4) { Text("Label") } currentValueLabel: { Text("40") }.gaugeStyle(.accessoryCircular).probe("gauge"))),
            ("accessory_circular_capacity", AnyView(Gauge(value: 0.4) { Text("Label") } currentValueLabel: { Text("40") }.gaugeStyle(.accessoryCircularCapacity).probe("gauge"))),
        ]
        for (name, view) in styles {
            let shot = Harness.capture(view.frame(width: 200), scheme: scheme)
            size(store, "gauge.\(name)", scheme, shot.frame("gauge"))
            store.image("gauge_\(name)", shot, shot.frame("gauge"))
        }
    }

    static func controlGroup(_ store: Store, _ scheme: Scheme) {
        let shot = Harness.capture(
            ControlGroup {
                Button("Cut") {}.probe("first")
                Button("Copy") {}
                Button("Paste") {}
            }.probe("group"),
            scheme: scheme)
        size(store, "control_group.automatic", scheme, shot.frame("group"))
        size(store, "control_group.first", scheme, shot.frame("first"))
        store.image("control_group_automatic", shot, shot.frame("group"))
    }

    static func labeled(_ store: Store, _ scheme: Scheme) {
        let shot = Harness.capture(
            LabeledContent {
                Text("Arisa").probe("value")
            } label: {
                Text("Name").probe("label")
            }.frame(width: 300).probe("row"),
            scheme: scheme)
        size(store, "labeled_content", scheme, shot.frame("row"))
        store.put("labeled_content.value_trailing", scheme, shot.frame("row").maxX - shot.frame("value").maxX)
        store.put("labeled_content.label_x", scheme, shot.frame("label").minX - shot.frame("row").minX)
    }

    static func columns(_ store: Store, _ scheme: Scheme) {
        let shot = Harness.capture(
            Form {
                LabeledContent {
                    Text("Arisa").probe("one_value")
                } label: {
                    Text("Name").probe("one_label")
                }
                LabeledContent {
                    Text("Switch").probe("two_value")
                } label: {
                    Text("Console model").probe("two_label")
                }
            }.formStyle(.columns).frame(width: 400).probe("form"),
            scheme: scheme)
        let form = shot.frame("form")
        store.put("form.columns.one_label_trailing", scheme, shot.frame("one_label").maxX - form.minX)
        store.put("form.columns.two_label_trailing", scheme, shot.frame("two_label").maxX - form.minX)
        store.put("form.columns.one_value_x", scheme, shot.frame("one_value").minX - form.minX)
        store.put("form.columns.two_value_x", scheme, shot.frame("two_value").minX - form.minX)
        store.put("form.columns.column_gap", scheme, shot.frame("two_value").minX - shot.frame("two_label").maxX)
        store.put("form.columns.row_gap", scheme, shot.frame("two_label").minY - shot.frame("one_label").maxY)
    }

    static func hidden(_ store: Store, _ scheme: Scheme) {
        let toggle = Harness.capture(Toggle("Wi-Fi", isOn: .constant(true)).toggleStyle(.switch).labelsHidden().probe("control"), scheme: scheme)
        size(store, "labels_hidden.toggle", scheme, toggle.frame("control"))
        let picker = Harness.capture(Picker("Flavour", selection: .constant(1)) { Text("Vanilla").tag(1) }.labelsHidden().probe("control"), scheme: scheme)
        let shown = Harness.capture(Picker("Flavour", selection: .constant(1)) { Text("Vanilla").tag(1) }.probe("control"), scheme: scheme)
        size(store, "labels_hidden.picker", scheme, picker.frame("control"))
        size(store, "labels_shown.picker", scheme, shown.frame("control"))
    }

    static func dates(_ store: Store, _ scheme: Scheme) {
        let compact = Harness.capture(DatePicker("Date", selection: .constant(date), displayedComponents: .date).labelsHidden().probe("picker"), scheme: scheme)
        size(store, "date_picker.compact_date", scheme, compact.frame("picker"))
        let both = Harness.capture(DatePicker("Date", selection: .constant(date)).labelsHidden().probe("picker"), scheme: scheme)
        size(store, "date_picker.compact_both", scheme, both.frame("picker"))
        let graphical = Harness.capture(DatePicker("Date", selection: .constant(date), displayedComponents: .date).datePickerStyle(.graphical).labelsHidden().probe("picker"), scheme: scheme)
        size(store, "date_picker.graphical", scheme, graphical.frame("picker"))
        store.image("date_picker_graphical", graphical, graphical.frame("picker"))
    }

    static func colours(_ store: Store, _ scheme: Scheme) {
        let shot = Harness.capture(ColorPicker("Colour", selection: .constant(.blue)).labelsHidden().probe("picker"), scheme: scheme)
        size(store, "color_picker.swatch", scheme, shot.frame("picker"))
    }

    static func shapes(_ store: Store, _ scheme: Scheme) {
        let capsule = Harness.capture(Button("Button") {}.buttonStyle(.bordered).buttonBorderShape(.capsule).probe("control"), scheme: scheme)
        size(store, "button.border_capsule", scheme, capsule.frame("control"))
        let flexible = Harness.capture(Button("Button") {}.buttonStyle(.bordered).buttonSizing(.flexible).frame(width: 300).probe("control"), scheme: scheme)
        size(store, "button.sizing_flexible", scheme, flexible.frame("control"))
        let fitted = Harness.capture(Button("Button") {}.buttonStyle(.bordered).buttonSizing(.fitted).frame(width: 300).probe("control"), scheme: scheme)
        size(store, "button.sizing_fitted", scheme, fitted.frame("control"))
    }

    static func menus(_ store: Store, _ scheme: Scheme) {
        let shown = Harness.capture(Menu("Actions") { Button("One") {} }.fixedSize().probe("menu"), scheme: scheme)
        let hidden = Harness.capture(Menu("Actions") { Button("One") {} }.menuIndicator(.hidden).fixedSize().probe("menu"), scheme: scheme)
        size(store, "menu.indicator_visible", scheme, shown.frame("menu"))
        size(store, "menu.indicator_hidden", scheme, hidden.frame("menu"))
    }
}
