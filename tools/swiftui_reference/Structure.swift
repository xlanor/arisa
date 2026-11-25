import AppKit
import SwiftUI

enum Structure {
    static let rows = ["One", "Two", "Three", "Four"]

    static func run(_ store: Store) {
        for scheme in Scheme.allCases {
            for style in ["inset", "plain", "sidebar", "bordered", "automatic"] {
                list(store, scheme, style)
            }
            form(store, scheme)
            sidebar(store, scheme)
            scroll(store, scheme)
        }
        windows(store)
    }

    static func listed<V: View>(_ view: V, _ style: String) -> AnyView {
        switch style {
        case "inset": return AnyView(view.listStyle(.inset))
        case "plain": return AnyView(view.listStyle(.plain))
        case "sidebar": return AnyView(view.listStyle(.sidebar))
        case "bordered": return AnyView(view.listStyle(.bordered))
        default: return AnyView(view.listStyle(.automatic))
        }
    }

    static func baselineGaps(_ store: Store, _ scheme: Scheme, _ key: String, _ lines: [CGRect], names: [String]) {
        for (i, name) in names.enumerated() where i < lines.count {
            store.put("\(key).ink.\(name).leading", scheme, lines[i].minX)
            store.put("\(key).ink.\(name).baseline", scheme, lines[i].maxY)
            store.put("\(key).ink.\(name).cap_top", scheme, lines[i].minY)
        }
    }

    static func list(_ store: Store, _ scheme: Scheme, _ style: String) {
        let content = List {
            Section {
                ForEach(rows, id: \.self) { Text($0) }
            } header: {
                Text("Header")
            } footer: {
                Text("Footer")
            }
        }
        let view = listed(content, style).frame(width: 300, height: 260).probe("list")
        let shot = Harness.capture(view, scheme: scheme, settle: 0.3)
        let box = shot.frame("list")
        let key = "list.\(style)"
        let lines = shot.raster.inkLines(in: CGRect(x: box.minX, y: box.minY, width: box.width * 0.5, height: box.height), contrast: 0.3)
        let names = ["header", "one", "two", "three", "four", "footer"]
        if lines.count >= 5 {
            let shifted = lines.map { $0.offsetBy(dx: -box.minX, dy: -box.minY) }
            baselineGaps(store, scheme, key, shifted, names: names)
            store.put("\(key).row_pitch", scheme, lines[2].maxY - lines[1].maxY)
            store.put("\(key).row_ink_leading", scheme, lines[1].minX - box.minX)
            store.put("\(key).header_ink_leading", scheme, lines[0].minX - box.minX)
            store.put("\(key).header_baseline_to_row_baseline", scheme, lines[1].maxY - lines[0].maxY)
            if lines.count >= 6 {
                store.put("\(key).row_baseline_to_footer_baseline", scheme, lines[5].maxY - lines[4].maxY)
            }
            store.colour("\(key).header_colour", scheme, shot.raster.extreme(in: lines[0], from: shot.raster.sampleFlat(CGPoint(x: lines[0].maxX + 4, y: lines[0].midY))))
            let between = (lines[1].maxY + lines[2].minY) / 2
            var separator = shot.raster.sampleFlat(CGPoint(x: box.midX, y: lines[1].maxY + 1))
            var y = lines[1].maxY + 1
            let row = shot.raster.sampleFlat(CGPoint(x: box.midX, y: lines[1].maxY + 1))
            while y < lines[2].minY - 1 {
                let c = shot.raster.sampleFlat(CGPoint(x: box.midX, y: y))
                if c.distance(row) > separator.distance(row) { separator = c }
                y += 0.5
            }
            _ = between
            store.colour("\(key).separator", scheme, separator)
        }
        store.colour("\(key).background", scheme, shot.raster.sampleFlat(CGPoint(x: box.maxX - 6, y: box.maxY - 6)))
        store.image("list_\(style)", shot, box, pad: 0)
    }

    static func form(_ store: Store, _ scheme: Scheme) {
        let content = Form {
            Section {
                Text("One")
                Text("Two")
                Toggle("Three", isOn: .constant(true))
                LabeledContent("Four") { Text("Value") }
            } header: {
                Text("Header")
            } footer: {
                Text("Footer")
            }
            Section {
                Text("Five")
            }
        }.formStyle(.grouped)
        let view = content.frame(width: 360, height: 360).probe("form")
        let shot = Harness.capture(view, scheme: scheme, settle: 0.3)
        let box = shot.frame("form")
        let key = "form.grouped"
        let lines = shot.raster.inkLines(in: CGRect(x: box.minX, y: box.minY, width: box.width * 0.45, height: box.height), contrast: 0.3)
        let names = ["header", "one", "two", "three", "four", "footer", "five"]
        guard lines.count >= 7 else {
            store.put("\(key).ink_lines", scheme, Double(lines.count), "count")
            return
        }
        baselineGaps(store, scheme, key, lines.map { $0.offsetBy(dx: -box.minX, dy: -box.minY) }, names: names)
        store.put("\(key).row_pitch", scheme, lines[2].maxY - lines[1].maxY)
        store.put("\(key).toggle_row_pitch", scheme, lines[4].maxY - lines[3].maxY)
        store.put("\(key).header_baseline_to_row_baseline", scheme, lines[1].maxY - lines[0].maxY)
        store.put("\(key).row_baseline_to_footer_baseline", scheme, lines[5].maxY - lines[4].maxY)
        store.put("\(key).footer_baseline_to_next_row_baseline", scheme, lines[6].maxY - lines[5].maxY)
        store.put("\(key).header_font_size", scheme, Measure.fontSize(width: lines[0].width + 1, text: "Header", height: 0))
        let values = shot.raster.inkLines(in: CGRect(x: box.midX, y: lines[4].minY - 2, width: box.width / 2, height: lines[4].height + 4), contrast: 0.2)
        let outside = shot.raster.sampleFlat(CGPoint(x: box.minX + 3, y: lines[1].maxY + 4))
        let seed = CGPoint(x: lines[1].minX - 4, y: lines[1].maxY + 4)
        let inside = shot.raster.sampleFlat(seed)
        store.colour("\(key).background", scheme, outside)
        store.colour("\(key).group_fill", scheme, inside)
        let horizontal = shot.raster.span(from: seed, tolerance: 0.015)
        let vertical = shot.raster.span(from: CGPoint(x: horizontal.minX + 3, y: lines[1].maxY + 4), tolerance: 0.015)
        let group = CGRect(x: horizontal.minX, y: vertical.minY, width: horizontal.width, height: vertical.height)
        if let value = values.first {
            store.put("\(key).value_ink_trailing_in_group", scheme, group.maxX - value.maxX)
            store.colour("\(key).value_colour", scheme, shot.raster.extreme(in: value, from: inside))
        }
        store.put("\(key).group_inset", scheme, group.minX - box.minX)
        store.put("\(key).group_width", scheme, group.width)
        store.put("\(key).group_height", scheme, group.height)
        store.put("\(key).row_ink_leading_in_group", scheme, lines[1].minX - group.minX)
        store.put("\(key).first_row_baseline_in_group", scheme, lines[1].maxY - group.minY)
        store.put("\(key).last_row_baseline_to_group_bottom", scheme, group.maxY - lines[4].maxY)
        store.put("\(key).header_baseline_to_group", scheme, group.minY - lines[0].maxY)
        store.put("\(key).group_radius", scheme, shot.raster.regionRadius(of: group, inside: inside, outside: outside))
        store.put("\(key).header_ink_leading_from_group", scheme, lines[0].minX - group.minX)
        var separator = inside
        var y = lines[1].maxY + 2
        while y < lines[2].minY - 1 {
            let c = shot.raster.sampleFlat(CGPoint(x: group.midX, y: y))
            if c.distance(inside) > separator.distance(inside) { separator = c }
            y += 0.5
        }
        store.colour("\(key).separator", scheme, separator)
        store.colour("\(key).header_colour", scheme, shot.raster.extreme(in: lines[0], from: outside))
        store.colour("\(key).footer_colour", scheme, shot.raster.extreme(in: lines[5], from: outside))
        store.image("form_grouped", shot, box, pad: 0)
    }

    static func sidebar(_ store: Store, _ scheme: Scheme) {
        let view = NavigationSplitView {
            List(selection: .constant(Set(["Two"]))) {
                Section {
                    ForEach(rows, id: \.self) { item in
                        Label(item, systemImage: "folder").tag(item)
                    }
                } header: {
                    Text("Section")
                }
            }
        } detail: {
            Text("Detail").probe("detail")
        }
        .frame(width: 720, height: 360)
        .probe("split")
        let shot = Harness.capture(view, scheme: scheme, settle: 0.4)
        let split = shot.frame("split")
        let key = "sidebar"
        let strip = CGRect(x: split.minX, y: split.minY + split.height * 0.6, width: split.width * 0.6, height: 1)
        let edge = shot.raster.inkLines(in: CGRect(x: split.minX, y: split.maxY - 20, width: split.width * 0.7, height: 4), contrast: 0.02)
        _ = strip
        let sidebarBackground = shot.raster.sampleFlat(CGPoint(x: split.minX + 6, y: split.maxY - 10))
        var x = split.minX + 6
        while x < split.maxX - 10, shot.raster.sampleFlat(CGPoint(x: x, y: split.maxY - 10)).distance(sidebarBackground) < 0.02 { x += 0.5 }
        let width = x - split.minX
        _ = edge
        store.put("\(key).width", scheme, width)
        store.colour("\(key).background", scheme, sidebarBackground)
        let area = CGRect(x: split.minX, y: split.minY, width: width, height: split.height)
        let lines = shot.raster.inkLines(in: area.insetBy(dx: 2, dy: 0), contrast: 0.3, reference: sidebarBackground)
        if lines.count >= 5 {
            let shifted = lines.map { $0.offsetBy(dx: -split.minX, dy: -split.minY) }
            baselineGaps(store, scheme, key, shifted, names: ["header", "one", "selection", "three", "four"])
            store.put("\(key).row_pitch", scheme, lines[4].maxY - lines[3].maxY)
            store.put("\(key).row_ink_leading", scheme, lines[1].minX - split.minX)
            store.put("\(key).header_ink_leading", scheme, lines[0].minX - split.minX)
            store.put("\(key).header_baseline_to_row_baseline", scheme, lines[1].maxY - lines[0].maxY)
            store.put("\(key).header_font_size", scheme, Measure.fontSize(width: lines[0].width + 1, text: "Section", height: 0))
            store.colour("\(key).header_colour", scheme, shot.raster.extreme(in: lines[0], from: sidebarBackground))
            let selection = lines[2]
            store.put("\(key).selection_inset_leading", scheme, selection.minX - split.minX)
            store.put("\(key).selection_inset_trailing", scheme, split.minX + width - selection.maxX)
            store.put("\(key).selection_height", scheme, selection.height)
            store.put("\(key).selection_top_from_previous_baseline", scheme, selection.minY - lines[1].maxY)
            let fill = shot.raster.sampleFlat(CGPoint(x: selection.maxX - 6, y: selection.midY))
            store.put("\(key).selection_radius", scheme, shot.raster.regionRadius(of: selection, inside: fill, outside: sidebarBackground))
            let icons = shot.raster.inkLines(in: CGRect(x: lines[1].minX - 1, y: lines[1].minY - 4, width: 30, height: lines[1].height + 8), contrast: 0.3, reference: sidebarBackground)
            if let icon = icons.first { store.put("\(key).icon_ink_height", scheme, icon.height) }
        }
        store.colour("\(key).selection_emphasized", scheme, RGBA.of(.selectedContentBackgroundColor, appearance: scheme.appearance))
        store.colour("\(key).selection_unemphasized", scheme, RGBA.of(.unemphasizedSelectedContentBackgroundColor, appearance: scheme.appearance))
        store.image("sidebar", shot, area, pad: 0)
        store.image("split_view", shot, split, pad: 0)
    }

    static func scroll(_ store: Store, _ scheme: Scheme) {
        for (name, style) in [("overlay", NSScroller.Style.overlay), ("legacy", NSScroller.Style.legacy)] {
            for (size, control) in [("regular", NSControl.ControlSize.regular), ("small", NSControl.ControlSize.small)] {
                store.put("scroll_indicator.\(name).\(size).width", nil, NSScroller.scrollerWidth(for: control, scrollerStyle: style))
            }
        }
        let scroller = NSScroller(frame: NSRect(x: 0, y: 0, width: 16, height: 200))
        scroller.scrollerStyle = .overlay
        scroller.knobProportion = 0.3
        scroller.doubleValue = 0.0
        scroller.isEnabled = true
        let raster = Harness.render(view: scroller, scheme: scheme, size: CGSize(width: 16, height: 200))
        if let knob = raster.opaqueBounds(in: CGRect(x: 0, y: 0, width: 16, height: 200), threshold: 0.2) {
            store.put("scroll_indicator.overlay.knob_thickness", scheme, knob.width)
            store.put("scroll_indicator.overlay.knob_trailing", scheme, 16 - knob.maxX)
            store.put("scroll_indicator.overlay.knob_radius", scheme, raster.alphaRadius(of: knob))
            store.colour("scroll_indicator.overlay.knob", scheme, raster.sample(CGPoint(x: knob.midX, y: knob.midY)))
        }
    }

    static func windows(_ store: Store) {
        store.put("navigation.title_font_size", nil, NSFont.titleBarFont(ofSize: 0).pointSize)
        store.put("navigation.title_font_weight", nil, Typography.weight(of: NSFont.titleBarFont(ofSize: 0)), "")
        for (name, style) in [("automatic", NSWindow.ToolbarStyle.automatic), ("unified", .unified), ("unified_compact", .unifiedCompact), ("expanded", .expanded)] {
            let window = KeyWindow(contentRect: NSRect(x: -10000, y: -10000, width: 600, height: 400), styleMask: [.titled, .closable, .miniaturizable, .resizable, .fullSizeContentView], backing: .buffered, defer: true)
            window.title = "Title"
            window.toolbarStyle = style
            window.toolbar = NSToolbar(identifier: "reference.\(name)")
            store.put("navigation.toolbar_\(name).bar_height", nil, window.frame.height - window.contentLayoutRect.height)
        }
        let plain = KeyWindow(contentRect: NSRect(x: -10000, y: -10000, width: 600, height: 400), styleMask: [.titled, .closable], backing: .buffered, defer: true)
        store.put("navigation.titlebar_only.bar_height", nil, plain.frame.height - plain.contentLayoutRect.height)
        let hosted = KeyWindow(contentRect: NSRect(x: -10000, y: -10000, width: 600, height: 400), styleMask: [.titled, .closable, .resizable, .fullSizeContentView], backing: .buffered, defer: true)
        hosted.contentView = NSHostingView(rootView: NavigationStack {
            Text("Content").navigationTitle("Title").toolbar {
                ToolbarItem { Button("A") {} }
                ToolbarItem { Button("B") {} }
            }
        })
        hosted.contentView?.layoutSubtreeIfNeeded()
        RunLoop.main.run(until: Date().addingTimeInterval(0.3))
        store.put("navigation.swiftui_stack.bar_height", nil, hosted.frame.height - hosted.contentLayoutRect.height)
        store.put("navigation.swiftui_stack.toolbar_items", nil, Double(hosted.toolbar?.items.count ?? 0), "count")
        let frames = (hosted.toolbar?.items ?? []).compactMap { $0.view.map { $0.convert($0.bounds, to: nil) } }
        if frames.count >= 2 {
            let sorted = frames.sorted { $0.minX < $1.minX }
            store.put("navigation.swiftui_stack.toolbar_item_spacing", nil, sorted[1].minX - sorted[0].maxX)
            store.put("navigation.swiftui_stack.toolbar_item_height", nil, sorted[0].height)
        }
        hosted.contentView = nil
    }
}
