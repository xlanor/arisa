import AppKit
import SwiftUI

enum Scrolling {
    static func run(_ store: Store) {
        let scheme = Scheme.light
        anchors(store, scheme)
        crossAxis(store, scheme)
        freeAxes(store, scheme)
        for scheme in Scheme.allCases {
            contentBackground(store, scheme)
        }
        search(store, scheme)
    }

    static func rows(_ count: Int) -> some View {
        VStack(spacing: 0) {
            ForEach(0..<count, id: \.self) { index in
                Color.gray.frame(width: 100, height: 30).probe("row\(index)")
            }
        }
    }

    static func anchors(_ store: Store, _ scheme: Scheme) {
        let bottom = Harness.capture(
            ScrollView { rows(20) }.defaultScrollAnchor(.bottom).frame(width: 300, height: 200).probe("view"),
            scheme: scheme, settle: 0.3)
        let view = bottom.frame("view")
        store.put("scroll.anchor.bottom.last_row_gap", scheme, view.maxY - bottom.frame("row19").maxY)
        store.put("scroll.anchor.bottom.first_row_y", scheme, bottom.frame("row0").minY - view.minY)

        let short = Harness.capture(
            ScrollView { rows(3) }.defaultScrollAnchor(.bottom).frame(width: 300, height: 200).probe("view"),
            scheme: scheme, settle: 0.3)
        let box = short.frame("view")
        store.put("scroll.anchor.bottom.short.last_row_gap", scheme, box.maxY - short.frame("row2").maxY)
        store.put("scroll.anchor.bottom.short.first_row_y", scheme, short.frame("row0").minY - box.minY)

        let aligned = Harness.capture(
            ScrollView { rows(3) }.defaultScrollAnchor(.bottom, for: .alignment).frame(width: 300, height: 200).probe("view"),
            scheme: scheme, settle: 0.3)
        let area = aligned.frame("view")
        store.put("scroll.anchor.alignment.short.first_row_y", scheme, aligned.frame("row0").minY - area.minY)

        let initial = Harness.capture(
            ScrollView { rows(3) }.defaultScrollAnchor(.bottom, for: .initialOffset).frame(width: 300, height: 200).probe("view"),
            scheme: scheme, settle: 0.3)
        let space = initial.frame("view")
        store.put("scroll.anchor.initial_offset.short.first_row_y", scheme, initial.frame("row0").minY - space.minY)

        let plain = Harness.capture(
            ScrollView { rows(3) }.frame(width: 300, height: 200).probe("view"),
            scheme: scheme, settle: 0.3)
        store.put("scroll.anchor.none.short.first_row_y", scheme, plain.frame("row0").minY - plain.frame("view").minY)
    }

    static func crossAxis(_ store: Store, _ scheme: Scheme) {
        let shot = Harness.capture(
            ScrollView { rows(3) }.frame(width: 300, height: 200).probe("view"),
            scheme: scheme, settle: 0.3)
        store.put("scroll.cross_axis.child_x", scheme, shot.frame("row0").minX - shot.frame("view").minX)
    }

    static func freeAxes(_ store: Store, _ scheme: Scheme) {
        let text = "A line of text long enough to wrap inside two hundred points"
        let free = Harness.capture(
            ScrollView([.horizontal, .vertical]) { Text(text).probe("text") }.frame(width: 200, height: 120).probe("view"),
            scheme: scheme, settle: 0.3)
        store.put("scroll.free.text_width", scheme, free.frame("text").width)
        store.put("scroll.free.text_height", scheme, free.frame("text").height)
        let vertical = Harness.capture(
            ScrollView { Text(text).probe("text") }.frame(width: 200, height: 120).probe("view"),
            scheme: scheme, settle: 0.3)
        store.put("scroll.vertical.text_width", scheme, vertical.frame("text").width)
        store.put("scroll.vertical.text_height", scheme, vertical.frame("text").height)
    }

    static func contentBackground(_ store: Store, _ scheme: Scheme) {
        for (name, visibility) in [("automatic", Visibility.automatic), ("visible", .visible), ("hidden", .hidden)] {
            let shot = Harness.capture(
                List { Text("One"); Text("Two") }
                    .listStyle(.inset)
                    .scrollContentBackground(visibility)
                    .frame(width: 300, height: 160)
                    .probe("list"),
                scheme: scheme, settle: 0.3)
            let box = shot.frame("list")
            let point = CGPoint(x: box.midX, y: box.maxY - 10)
            store.colour("scroll.content_background.\(name).fill", scheme, shot.raster.sampleFlat(point))
            store.colour("scroll.content_background.\(name).fill_alpha", scheme, shot.raster.sample(point))
        }
        let editor = Harness.capture(
            TextEditor(text: .constant("Notes")).scrollContentBackground(.hidden).frame(width: 200, height: 80).probe("editor"),
            scheme: scheme, settle: 0.3)
        let shown = Harness.capture(
            TextEditor(text: .constant("Notes")).frame(width: 200, height: 80).probe("editor"),
            scheme: scheme, settle: 0.3)
        let spot = CGPoint(x: editor.frame("editor").maxX - 10, y: editor.frame("editor").maxY - 10)
        store.colour("scroll.content_background.editor_hidden.fill_alpha", scheme, editor.raster.sample(spot))
        store.colour("scroll.content_background.editor_default.fill_alpha", scheme, shown.raster.sample(spot))
    }

    static func search(_ store: Store, _ scheme: Scheme) {
        let shot = Harness.capture(
            NavigationStack {
                List { Text("One"); Text("Two") }
                    .searchable(text: .constant(""), prompt: "Titles")
            }
            .frame(width: 400, height: 240)
            .probe("stack"),
            scheme: scheme, settle: 0.5)
        let box = shot.frame("stack")
        let first = shot.raster.inkLines(in: box, contrast: 0.3).first
        store.put("search.navigation.first_ink_y", scheme, Double((first?.minY ?? box.minY) - box.minY))
        let field = NSSearchField()
        field.placeholderString = "Titles"
        let size = field.intrinsicContentSize
        store.put("search.appkit.field_height", scheme, Double(size.height))
        field.frame = NSRect(x: 10, y: 10, width: 200, height: size.height)
        let raster = Harness.render(view: field, scheme: scheme, size: CGSize(width: 220, height: size.height + 20))
        let ink = raster.differingBounds(in: CGRect(x: 10, y: 10, width: 200, height: size.height), from: raster.background, tolerance: 0.05)
        store.put("search.appkit.field_ink_height", scheme, Double(ink?.height ?? 0))
    }
}
