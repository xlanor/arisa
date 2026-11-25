import AppKit
import SwiftUI

enum Navigation {
    struct ThreeColumn: View {
        let style: Int
        let contentWidth: CGFloat?

        var body: some View {
            let split = NavigationSplitView {
                List {
                    Text("Inbox")
                    Text("Sent")
                }
                .probe("sidebar")
            } content: {
                Group {
                    if let contentWidth {
                        Color.clear.frame(maxWidth: .infinity, maxHeight: .infinity).probe("content").navigationSplitViewColumnWidth(contentWidth)
                    } else {
                        Color.clear.frame(maxWidth: .infinity, maxHeight: .infinity).probe("content")
                    }
                }
            } detail: {
                Color.clear.frame(maxWidth: .infinity, maxHeight: .infinity).probe("detail")
            }
            .coordinateSpace(.named("reference.root"))
            switch style {
            case 1: split.navigationSplitViewStyle(.balanced)
            case 2: split.navigationSplitViewStyle(.prominentDetail)
            default: split
            }
        }
    }

    struct Bars: View {
        var body: some View {
            NavigationStack {
                Color.clear
                    .navigationTitle("Title")
                    .navigationSubtitle("Subtitle")
                    .toolbar {
                        ToolbarItem(placement: .primaryAction) {
                            Button {} label: { Image(systemName: "plus") }
                        }
                        ToolbarSpacer(.fixed, placement: .primaryAction)
                        ToolbarItem(placement: .primaryAction) {
                            Button {} label: { Image(systemName: "trash") }
                        }
                    }
            }
        }
    }

    static func run(_ store: Store) {
        let scheme = Scheme.light
        split(store, scheme, style: 0, contentWidth: nil, key: "split.automatic")
        split(store, scheme, style: 1, contentWidth: nil, key: "split.balanced")
        split(store, scheme, style: 2, contentWidth: nil, key: "split.prominent_detail")
        split(store, scheme, style: 0, contentWidth: 300, key: "split.content_300")
        insets(store, scheme)
        bars(store)
        tabsPicker(store, scheme)
    }

    static func tabsPicker(_ store: Store, _ scheme: Scheme) {
        let tabs = Picker("Picker", selection: .constant(1)) { Text("One").tag(0); Text("Two").tag(1); Text("Three").tag(2) }
            .pickerStyle(.tabs).labelsHidden().probe("control")
        let shot = Harness.capture(tabs, scheme: scheme)
        store.put("picker.tabs.regular.width", scheme, shot.frame("control").width)
        store.put("picker.tabs.regular.height", scheme, shot.frame("control").height)
        let segmented = Picker("Picker", selection: .constant(1)) { Text("One").tag(0); Text("Two").tag(1); Text("Three").tag(2) }
            .pickerStyle(.segmented).labelsHidden().probe("control")
        let plain = Harness.capture(segmented, scheme: scheme)
        store.put("picker.tabs.segmented_width", scheme, plain.frame("control").width)
        store.put("picker.tabs.segmented_height", scheme, plain.frame("control").height)
    }

    static func split(_ store: Store, _ scheme: Scheme, style: Int, contentWidth: CGFloat?, key: String) {
        let sink = FrameSink()
        let root = ThreeColumn(style: style, contentWidth: contentWidth)
            .onPreferenceChange(ProbeFrames.self) { sink.frames = $0 }
        let window = KeyWindow(contentRect: NSRect(x: -10000, y: -10000, width: 1024, height: 576), styleMask: [.titled, .closable, .resizable, .fullSizeContentView], backing: .buffered, defer: false)
        window.contentView = NSHostingView(rootView: root)
        window.contentView?.layoutSubtreeIfNeeded()
        RunLoop.main.run(until: Date().addingTimeInterval(0.8))
        window.contentView?.layoutSubtreeIfNeeded()
        RunLoop.main.run(until: Date().addingTimeInterval(0.2))
        let wrappers = Chrome.named(Chrome.all(window.contentView!.superview!), "_NSSplitViewItemViewWrapper")
        store.put("\(key).columns", scheme, Double(wrappers.count), "count")
        store.put("\(key).sidebar_width", scheme, Double(sink.frames["sidebar"]?.width ?? 0))
        store.put("\(key).content_width", scheme, Double(sink.frames["content"]?.width ?? 0))
        store.put("\(key).content_x", scheme, Double(sink.frames["content"]?.minX ?? 0))
        store.put("\(key).detail_x", scheme, Double(sink.frames["detail"]?.minX ?? 0))
        store.put("\(key).detail_width", scheme, Double(sink.frames["detail"]?.width ?? 0))
        window.contentView = nil
    }

    static func insets(_ store: Store, _ scheme: Scheme) {
        let plain = Harness.capture(
            Color.clear.frame(maxWidth: .infinity, maxHeight: .infinity).probe("content")
                .safeAreaInset(edge: .bottom) { Color.gray.frame(height: 40).probe("bar") }
                .frame(width: 200, height: 300),
            scheme: scheme)
        store.put("safe_area_inset.default.content_height", scheme, plain.frame("content").height)
        store.put("safe_area_inset.default.spacing", scheme, plain.frame("bar").minY - plain.frame("content").maxY)
        store.put("safe_area_inset.default.bar_width", scheme, plain.frame("bar").width)
        let spaced = Harness.capture(
            Color.clear.frame(maxWidth: .infinity, maxHeight: .infinity).probe("content")
                .safeAreaInset(edge: .bottom, spacing: 10) { Color.gray.frame(height: 40).probe("bar") }
                .frame(width: 200, height: 300),
            scheme: scheme)
        store.put("safe_area_inset.spacing_10.content_height", scheme, spaced.frame("content").height)
        let aligned = Harness.capture(
            Color.clear.frame(maxWidth: .infinity, maxHeight: .infinity).probe("content")
                .safeAreaInset(edge: .bottom, alignment: .trailing, spacing: 0) { Color.gray.frame(width: 50, height: 40).probe("bar") }
                .frame(width: 200, height: 300),
            scheme: scheme)
        store.put("safe_area_inset.trailing.bar_x", scheme, aligned.frame("bar").minX - aligned.frame("content").minX)
        let leading = Harness.capture(
            Color.clear.frame(maxWidth: .infinity, maxHeight: .infinity).probe("content")
                .safeAreaInset(edge: .leading, spacing: 0) { Color.gray.frame(width: 50).probe("bar") }
                .frame(width: 200, height: 300),
            scheme: scheme)
        store.put("safe_area_inset.leading.content_width", scheme, leading.frame("content").width)
        store.put("safe_area_inset.leading.bar_height", scheme, leading.frame("bar").height)
        let scrolled = Harness.capture(
            ScrollView {
                VStack(spacing: 0) {
                    ForEach(0..<20) { index in
                        Color.gray.frame(height: 30).probe("row\(index)")
                    }
                }
            }
            .probe("scroll")
            .safeAreaInset(edge: .top, spacing: 0) { Color.gray.frame(height: 40).probe("bar") }
            .frame(width: 200, height: 300),
            scheme: scheme)
        store.put("safe_area_inset.scroll.first_row_y", scheme, scrolled.frame("row0").minY - scrolled.frame("scroll").minY)
        store.put("safe_area_inset.scroll.scroll_height", scheme, scrolled.frame("scroll").height)
        let bar = Harness.capture(
            Color.clear.frame(maxWidth: .infinity, maxHeight: .infinity).probe("content")
                .safeAreaBar(edge: .bottom) { Color.gray.frame(height: 40).probe("bar") }
                .frame(width: 200, height: 300),
            scheme: scheme)
        store.put("safe_area_bar.default.content_height", scheme, bar.frame("content").height)
        store.put("safe_area_bar.default.spacing", scheme, bar.frame("bar").minY - bar.frame("content").maxY)
    }

    static func bars(_ store: Store) {
        let window = KeyWindow(contentRect: NSRect(x: -10000, y: -10000, width: 900, height: 500), styleMask: [.titled, .closable, .resizable, .fullSizeContentView], backing: .buffered, defer: false)
        window.contentView = NSHostingView(rootView: Bars())
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
        let views = Chrome.all(frame)
        let fields = views.compactMap { $0 as? NSTextField }.filter { !$0.stringValue.isEmpty }
        if let title = fields.first(where: { $0.stringValue == "Title" }), let font = title.font {
            store.put("navigation.subtitle.title_size", nil, font.pointSize)
            store.put("navigation.subtitle.title_y", nil, top(title).minY)
        }
        if let subtitle = fields.first(where: { $0.stringValue == "Subtitle" }), let font = subtitle.font {
            store.put("navigation.subtitle.size", nil, font.pointSize)
            store.put("navigation.subtitle.weight", nil, Typography.weight(of: font), "")
            store.put("navigation.subtitle.y", nil, top(subtitle).minY)
            if let title = fields.first(where: { $0.stringValue == "Title" }) {
                store.put("navigation.subtitle.gap", nil, top(subtitle).minY - top(title).maxY)
            }
        }
        let platters = Chrome.named(views, "NSToolbarPlatterView").map(top).sorted { $0.minX < $1.minX }
        store.put("toolbar.spacer.platters", nil, Double(platters.count), "count")
        if platters.count >= 2 {
            let last = platters[platters.count - 1]
            let previous = platters[platters.count - 2]
            store.put("toolbar.spacer.fixed_gap", nil, last.minX - previous.maxX)
        }
        window.contentView = nil
    }
}
