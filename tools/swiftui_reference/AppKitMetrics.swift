import AppKit

enum AppKitMetrics {
    static let sizes: [(String, NSControl.ControlSize)] = [("mini", .mini), ("small", .small), ("regular", .regular), ("large", .large), ("extra_large", .extraLarge)]

    static func record(_ store: Store, _ key: String, _ control: NSView) {
        let intrinsic = control.intrinsicContentSize
        let fitting = control.fittingSize
        let insets = control.alignmentRectInsets
        store.put("\(key).intrinsic_width", nil, intrinsic.width == NSView.noIntrinsicMetric ? -1 : intrinsic.width)
        store.put("\(key).intrinsic_height", nil, intrinsic.height == NSView.noIntrinsicMetric ? -1 : intrinsic.height)
        store.put("\(key).fitting_width", nil, fitting.width)
        store.put("\(key).fitting_height", nil, fitting.height)
        store.put("\(key).alignment_inset_top", nil, insets.top)
        store.put("\(key).alignment_inset_left", nil, insets.left)
        store.put("\(key).alignment_inset_bottom", nil, insets.bottom)
        store.put("\(key).alignment_inset_right", nil, insets.right)
    }

    static func run(_ store: Store) {
        for (name, size) in sizes {
            store.put("appkit.font.system_size.\(name)", nil, NSFont.systemFontSize(for: size))
            let button = NSButton(title: "Button", target: nil, action: nil)
            button.controlSize = size
            record(store, "appkit.button.push.\(name)", button)
            let sw = NSSwitch()
            sw.controlSize = size
            record(store, "appkit.switch.\(name)", sw)
            let check = NSButton(checkboxWithTitle: "", target: nil, action: nil)
            check.controlSize = size
            record(store, "appkit.checkbox.\(name)", check)
            let slider = NSSlider(value: 0.5, minValue: 0, maxValue: 1, target: nil, action: nil)
            slider.controlSize = size
            record(store, "appkit.slider.\(name)", slider)
            store.put("appkit.slider.\(name).knob_thickness", nil, slider.knobThickness)
            let stepper = NSStepper()
            stepper.controlSize = size
            record(store, "appkit.stepper.\(name)", stepper)
            let segmented = NSSegmentedControl(labels: ["One", "Two", "Three"], trackingMode: .selectOne, target: nil, action: nil)
            segmented.controlSize = size
            record(store, "appkit.segmented.\(name)", segmented)
            let popup = NSPopUpButton(frame: .zero, pullsDown: false)
            popup.addItems(withTitles: ["One", "Two"])
            popup.controlSize = size
            record(store, "appkit.popup.\(name)", popup)
            let field = NSTextField(string: "")
            field.bezelStyle = .roundedBezel
            field.controlSize = size
            field.font = NSFont.systemFont(ofSize: NSFont.systemFontSize(for: size))
            record(store, "appkit.text_field.\(name)", field)
            let bar = NSProgressIndicator()
            bar.style = .bar
            bar.isIndeterminate = false
            bar.controlSize = size
            record(store, "appkit.progress_bar.\(name)", bar)
            let spinner = NSProgressIndicator()
            spinner.style = .spinning
            spinner.controlSize = size
            record(store, "appkit.spinner.\(name)", spinner)
        }
        store.put("appkit.font.label_size", nil, NSFont.labelFontSize)
        store.put("appkit.font.menu_size", nil, NSFont.menuFont(ofSize: 0).pointSize)
        let menu = NSMenu()
        for t in ["One", "Two", "Three", "Four"] { menu.addItem(withTitle: t, action: nil, keyEquivalent: "") }
        let four = menu.size
        menu.addItem(withTitle: "Five", action: nil, keyEquivalent: "")
        let five = menu.size
        store.put("appkit.menu.item_height", nil, five.height - four.height)
        store.put("appkit.menu.vertical_padding", nil, four.height - 4 * (five.height - four.height))
        store.put("appkit.menu.width_four_items", nil, four.width)
        menu.addItem(.separator())
        store.put("appkit.menu.separator_height", nil, menu.size.height - five.height)
    }
}
