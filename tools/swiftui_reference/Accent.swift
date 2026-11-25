import AppKit
import SwiftUI

enum Accent {
    static func run(_ store: Store) {
        for scheme in Scheme.allCases {
            containers(store, scheme)
            selection(store, scheme)
            alert(store, scheme)
            toolbar(store, scheme)
        }
    }

    static func buttonColours(_ store: Store, _ key: String, _ shot: Shot) {
        let box = shot.frame("control")
        guard box.width > 0 else { return }
        let label = shot.frame("label")
        let fill = shot.raster.sampleFlat(CGPoint(x: box.midX, y: box.minY + min(2.5, box.height / 4)))
        store.colour("\(key).fill", shot.scheme, fill)
        store.colour("\(key).text", shot.scheme, shot.raster.extreme(in: label.insetBy(dx: 1, dy: 1), from: fill))
        store.put("\(key).height", shot.scheme, box.height)
        store.put("\(key).padding_leading", shot.scheme, label.minX - box.minX)
    }

    static func containers(_ store: Store, _ scheme: Scheme) {
        let plain = VStack { Button {} label: { Text("Button").probe("label") }.probe("control") }
        buttonColours(store, "accent.button_in_view", Harness.capture(plain, scheme: scheme))
        let sheet = VStack(spacing: 12) {
            Text("Sheet")
            HStack {
                Button("Cancel", role: .cancel) {}
                Button {} label: { Text("Button").probe("label") }.keyboardShortcut(.defaultAction).probe("control")
            }
        }.padding()
        buttonColours(store, "accent.default_action_button", Harness.capture(sheet, scheme: scheme))
        let rows = ["One", "Two"]
        let list = List {
            ForEach(rows, id: \.self) { Text($0) }
            Button("Button") {}
        }.listStyle(.inset).frame(width: 300, height: 140).probe("list")
        let ls = Harness.capture(list, scheme: scheme, settle: 0.3)
        let lines = ls.raster.inkLines(in: ls.frame("list"), contrast: 0.3)
        if lines.count >= 3 {
            let row = lines[2]
            let behind = ls.raster.sampleFlat(CGPoint(x: ls.frame("list").maxX - 8, y: row.midY))
            store.colour("accent.button_in_list_row.text", scheme, ls.raster.extreme(in: row, from: behind))
            store.colour("accent.button_in_list_row.fill", scheme, ls.raster.sampleFlat(CGPoint(x: row.minX - 6, y: row.midY)))
            store.put("accent.button_in_list_row.ink_leading", scheme, row.minX - ls.frame("list").minX)
        }
        store.image("accent_list_button", ls, ls.frame("list"), pad: 0)
        let form = Form {
            Section {
                Text("One")
                Button("Button") {}
            }
        }.formStyle(.grouped).frame(width: 300, height: 160).probe("form")
        let fs = Harness.capture(form, scheme: scheme, settle: 0.3)
        let flines = fs.raster.inkLines(in: fs.frame("form"), contrast: 0.3)
        if flines.count >= 2 {
            let row = flines[1]
            let behind = fs.raster.sampleFlat(CGPoint(x: row.minX - 3, y: row.maxY + 3))
            store.colour("accent.button_in_form_row.text", scheme, fs.raster.extreme(in: row, from: behind))
            store.put("accent.button_in_form_row.ink_width", scheme, row.width)
            let span = fs.raster.span(from: CGPoint(x: row.midX, y: row.minY - 1), tolerance: 0.02)
            store.put("accent.button_in_form_row.bezel_height", scheme, span.height)
        }
        store.image("accent_form_button", fs, fs.frame("form"), pad: 0)
    }

    static func selection(_ store: Store, _ scheme: Scheme) {
        let rows = ["One", "Two", "Three"]
        let view = List(selection: .constant(Set(["Two"]))) {
            ForEach(rows, id: \.self) { Text($0).tag($0) }
        }.listStyle(.inset).frame(width: 300, height: 120).probe("list")
        for focused in [false, true] {
            let sink = FrameSink()
            let root = view.padding(Harness.margin).coordinateSpace(.named("reference.root"))
                .onPreferenceChange(ProbeFrames.self) { sink.frames = $0 }
                .environment(\.colorScheme, scheme.colorScheme)
            let window = KeyWindow(contentRect: NSRect(x: -10000, y: -10000, width: 324, height: 144), styleMask: [.borderless], backing: .buffered, defer: true)
            window.isOpaque = false
            window.backgroundColor = .clear
            window.appearance = scheme.appearance
            let host = NSHostingView(rootView: root)
            host.appearance = scheme.appearance
            window.contentView = host
            host.frame = NSRect(x: 0, y: 0, width: 324, height: 144)
            host.layoutSubtreeIfNeeded()
            RunLoop.main.run(until: Date().addingTimeInterval(0.3))
            if focused, let table = find(NSTableView.self, in: host) {
                window.makeFirstResponder(table)
            } else {
                window.makeFirstResponder(nil)
            }
            RunLoop.main.run(until: Date().addingTimeInterval(0.15))
            let raster = Raster.render(host, size: host.bounds.size, scale: Harness.scale, scheme: scheme)
            let box = sink.frames["list"] ?? .zero
            let key = "accent.list_selection.\(focused ? "focused" : "unfocused")"
            let lines = raster.inkLines(in: box, contrast: 0.08, reference: raster.sampleFlat(CGPoint(x: box.maxX - 3, y: box.maxY - 3)))
            if let band = lines.first(where: { $0.height > 18 }) {
                let fill = raster.sampleFlat(CGPoint(x: band.maxX - 8, y: band.midY))
                store.colour("\(key).fill", scheme, fill)
                store.colour("\(key).text", scheme, raster.extreme(in: band.insetBy(dx: 10, dy: 4), from: fill))
                store.put("\(key).height", scheme, band.height)
                store.put("\(key).radius", scheme, raster.regionRadius(of: band, inside: fill, outside: raster.sampleFlat(CGPoint(x: box.maxX - 3, y: box.maxY - 3))))
            }
            let shot = Shot(raster: raster, frames: sink.frames, size: host.bounds.size, scheme: scheme)
            store.image("accent_list_selection_\(focused ? "focused" : "unfocused")", shot, box, pad: 0)
            window.contentView = nil
        }
    }

    static func find<T: NSView>(_ type: T.Type, in view: NSView) -> T? {
        if let match = view as? T { return match }
        for child in view.subviews {
            if let found = find(type, in: child) { return found }
        }
        return nil
    }

    static func alert(_ store: Store, _ scheme: Scheme) {
        scheme.appearance.performAsCurrentDrawingAppearance {
            let alert = NSAlert()
            alert.messageText = "Title"
            alert.informativeText = "Text"
            alert.addButton(withTitle: "OK")
            alert.addButton(withTitle: "Cancel")
            alert.window.appearance = scheme.appearance
            alert.layout()
            let content = alert.window.contentView!
            content.layoutSubtreeIfNeeded()
            let raster = Raster.render(content, size: content.bounds.size, scale: Harness.scale, scheme: scheme)
            for (index, name) in ["default", "other"].enumerated() where index < alert.buttons.count {
                let button = alert.buttons[index]
                var frame = button.convert(button.bounds, to: content)
                if !content.isFlipped { frame.origin.y = content.bounds.height - frame.maxY }
                let fill = raster.sampleFlat(CGPoint(x: frame.midX, y: frame.minY + 3))
                store.colour("accent.alert_\(name)_button.fill", scheme, fill)
                store.colour("accent.alert_\(name)_button.text", scheme, raster.extreme(in: frame.insetBy(dx: 30, dy: 7), from: fill))
            }
        }
    }

    static func toolbar(_ store: Store, _ scheme: Scheme) {
        let button = NSButton(title: "Mode", target: nil, action: nil)
        button.bezelStyle = .toolbar
        button.sizeToFit()
        let size = CGSize(width: button.frame.width + 20, height: button.frame.height + 20)
        button.frame.origin = CGPoint(x: 10, y: 10)
        let raster = Harness.render(view: button, scheme: scheme, size: size)
        let frame = CGRect(x: 10, y: 10, width: button.frame.width, height: button.frame.height)
        let fill = raster.sampleFlat(CGPoint(x: frame.minX + 4, y: frame.midY))
        store.put("accent.toolbar_button.width", scheme, frame.width)
        store.put("accent.toolbar_button.height", scheme, frame.height)
        store.colour("accent.toolbar_button.fill", scheme, fill)
        store.colour("accent.toolbar_button.text", scheme, raster.extreme(in: frame.insetBy(dx: 8, dy: 5), from: fill))
        if let bezel = raster.opaqueBounds(in: frame.insetBy(dx: -2, dy: -2), threshold: max(0.02, raster.sample(CGPoint(x: frame.minX + 4, y: frame.midY)).a * 0.5)) {
            store.put("accent.toolbar_button.radius", scheme, raster.cornerRadius(of: bezel) { min(1, raster.alpha($0, $1) / max(raster.sample(CGPoint(x: frame.minX + 4, y: frame.midY)).a, 0.02)) })
        }
        let shot = Shot(raster: raster, frames: [:], size: size, scheme: scheme)
        store.image("accent_toolbar_button", shot, frame, pad: 4)
    }
}
