import AppKit
import SwiftUI

enum Chrome {
    struct Window: View {
        @State var selection: String? = "Two"

        var body: some View {
            NavigationSplitView {
                List(selection: $selection) {
                    Section("Section") {
                        ForEach(["One", "Two", "Three", "Four"], id: \.self) { item in
                            Label(item, systemImage: "folder").tag(Optional(item))
                        }
                    }
                }
            } detail: {
                Text("Detail")
                    .navigationTitle("Title")
                    .toolbar {
                        ToolbarItem(placement: .navigation) {
                            Button {} label: { Image(systemName: "chevron.left") }
                        }
                        ToolbarItemGroup(placement: .primaryAction) {
                            Button {} label: { Image(systemName: "square.and.arrow.up") }
                            Button {} label: { Image(systemName: "plus") }
                        }
                        ToolbarItem(placement: .primaryAction) {
                            Button("Dark mode") {}
                        }
                    }
            }
        }
    }

    static func run(_ store: Store) {
        let window = KeyWindow(contentRect: NSRect(x: -10000, y: -10000, width: 900, height: 500), styleMask: [.titled, .closable, .miniaturizable, .resizable, .fullSizeContentView], backing: .buffered, defer: false)
        window.contentView = NSHostingView(rootView: Window())
        window.contentView?.layoutSubtreeIfNeeded()
        RunLoop.main.run(until: Date().addingTimeInterval(0.8))
        guard let frame = window.contentView?.superview else { return }
        frame.layoutSubtreeIfNeeded()
        RunLoop.main.run(until: Date().addingTimeInterval(0.3))
        let height = window.frame.height
        let top = { (view: NSView) -> CGRect in
            let rect = view.convert(view.bounds, to: nil)
            return CGRect(x: rect.minX, y: height - rect.maxY, width: rect.width, height: rect.height)
        }
        let views = all(frame)
        toolbar(store, views, top)
        sidebar(store, views, top)
        if window.responds(to: NSSelectorFromString("_cornerRadius")), let radius = window.value(forKey: "_cornerRadius") as? Double {
            store.put("chrome.window.corner_radius_offscreen", nil, radius)
        }
        window.contentView = nil
    }

    static func all(_ view: NSView) -> [NSView] {
        [view] + view.subviews.flatMap { all($0) }
    }

    static func named(_ views: [NSView], _ fragment: String) -> [NSView] {
        views.filter { String(describing: type(of: $0)).contains(fragment) }
    }

    static func toolbar(_ store: Store, _ views: [NSView], _ top: (NSView) -> CGRect) {
        if let bar = named(views, "NSToolbarView").first {
            store.put("chrome.toolbar.bar_height", nil, top(bar).height)
        }
        if let title = named(views, "_NSToolbarTitleField").first as? NSTextField, let font = title.font {
            store.put("chrome.toolbar.title_size", nil, font.pointSize)
            store.put("chrome.toolbar.title_weight", nil, Typography.weight(of: font), "")
            store.put("chrome.toolbar.title_leading_from_platter", nil, top(title).minX - (named(views, "NSToolbarPlatterView").map(top).filter { $0.maxX <= top(title).minX }.map(\.maxX).max() ?? 0))
        }
        let platters = named(views, "NSToolbarPlatterView").map { view in (view, top(view)) }.sorted { $0.1.minX < $1.1.minX }
        guard let first = platters.first else { return }
        store.put("chrome.toolbar.platter_height", nil, first.1.height)
        store.put("chrome.toolbar.platter_top_inset", nil, first.1.minY)
        if let glass = all(first.0).compactMap({ $0 as? NSGlassEffectView }).first {
            store.put("chrome.toolbar.platter_radius", nil, glass.cornerRadius)
        }
        let trailing = platters.filter { $0.1.minX > 400 }
        if trailing.count >= 2 {
            store.put("chrome.toolbar.platter_gap", nil, trailing[1].1.minX - trailing[0].1.maxX)
        }
        for (view, rect) in platters {
            let items = named(all(view), "ToolbarItemHostingView").map(top).sorted { $0.minX < $1.minX }
            guard let lead = items.first, let last = items.last else { continue }
            if items.count >= 2 {
                store.put("chrome.toolbar.group_item_spacing", nil, items[1].minX - items[0].maxX)
                store.put("chrome.toolbar.group_items", nil, Double(items.count), "count")
                store.put("chrome.toolbar.group_width", nil, rect.width)
                store.put("chrome.toolbar.platter_padding_leading", nil, lead.minX - rect.minX)
                store.put("chrome.toolbar.platter_padding_trailing", nil, rect.maxX - last.maxX)
                store.put("chrome.toolbar.icon_item_width", nil, lead.width)
                store.put("chrome.toolbar.item_height", nil, lead.height)
            }
            if items.count == 1, rect.width > 60 {
                let label = NSAttributedString(string: "Dark mode", attributes: [.font: NSFont.systemFont(ofSize: NSFont.systemFontSize)])
                store.put("chrome.toolbar.text_item_width", nil, lead.width)
                store.put("chrome.toolbar.text_item_padding", nil, (lead.width - ceil(label.size().width)) / 2)
            }
        }
    }

    static func sidebar(_ store: Store, _ views: [NSView], _ top: (NSView) -> CGRect) {
        if let wrapper = named(views, "_NSSplitViewItemViewWrapper").map(top).min(by: { $0.width < $1.width }) {
            store.put("chrome.sidebar.width", nil, wrapper.width)
        }
        if let glass = views.compactMap({ $0 as? NSGlassEffectView }).first(where: { top($0).minX == 0 && top($0).height > 300 }) {
            store.put("chrome.sidebar.glass_inset_offscreen", nil, top(glass).minX)
            store.put("chrome.sidebar.glass_radius_offscreen", nil, glass.cornerRadius)
        }
        let rows = named(views, "ListTableRowView").map(top).sorted { $0.minY < $1.minY }
        if rows.count >= 3 {
            store.put("chrome.sidebar.header_row_height", nil, rows[0].height)
            store.put("chrome.sidebar.row_height", nil, rows[1].height)
            store.put("chrome.sidebar.row_pitch", nil, rows[2].minY - rows[1].minY)
        }
        if let header = named(views, "ListTableHeaderView").first {
            store.put("chrome.sidebar.header_leading", nil, top(header).minX)
        }
        if let selection = views.first(where: { $0 is NSVisualEffectView && ($0.layer?.cornerRadius ?? 0) > 0 && top($0).height < 60 }) {
            store.put("chrome.sidebar.selection_inset", nil, top(selection).minX)
            store.put("chrome.sidebar.selection_height", nil, top(selection).height)
            store.put("chrome.sidebar.selection_radius", nil, Double(selection.layer?.cornerRadius ?? 0))
        }
        guard let cell = named(views, "CellHostingView").first, let layers = cell.layer?.sublayers else { return }
        let origin = top(cell).minX
        store.put("chrome.sidebar.cell_leading", nil, origin)
        let image = layers.first { String(describing: type(of: $0)).contains("Image") }
        let text = layers.first { String(describing: type(of: $0)).contains("Drawing") }
        if let image {
            store.put("chrome.sidebar.icon_leading", nil, origin + image.frame.minX)
            store.put("chrome.sidebar.icon_width", nil, image.frame.width)
            store.put("chrome.sidebar.icon_height", nil, image.frame.height)
        }
        if let text {
            store.put("chrome.sidebar.label_leading", nil, origin + text.frame.minX)
            if let image {
                store.put("chrome.sidebar.icon_label_gap", nil, text.frame.minX - image.frame.maxX)
            }
        }
    }
}
