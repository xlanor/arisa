import AppKit
import SwiftUI

enum Geometry {
    static let title = "A long title that truncates"
    static let paragraph = "A line of text long enough to wrap inside two hundred points"

    static func run(_ store: Store) {
        let scheme = Scheme.light
        priority(store, scheme)
        fixed(store, scheme)
        position(store, scheme)
        zOrder(store, scheme)
        relative(store, scheme)
        reader(store, scheme)
        named(store, scheme)
        wrapped(store, scheme)
        gridRow(store, scheme)
        listRow(store, scheme)
        crossAnchor(store, scheme)
        aspectFit(store, scheme)
        stackFlexibility(store, scheme)
    }

    static func priority(_ store: Store, _ scheme: Scheme) {
        let plain = Harness.capture(
            HStack {
                Text(title).lineLimit(1).probe("title")
                Text("$9.99").lineLimit(1).probe("price")
            }.frame(width: 160).probe("row"),
            scheme: scheme)
        store.put("priority.none.title_width", scheme, plain.frame("title").width)
        store.put("priority.none.price_width", scheme, plain.frame("price").width)
        let kept = Harness.capture(
            HStack {
                Text(title).lineLimit(1).probe("title")
                Text("$9.99").lineLimit(1).layoutPriority(1).probe("price")
            }.frame(width: 160).probe("row"),
            scheme: scheme)
        store.put("priority.price.title_width", scheme, kept.frame("title").width)
        store.put("priority.price.price_width", scheme, kept.frame("price").width)
        store.put("priority.price.price_x", scheme, kept.frame("price").minX - kept.frame("row").minX)
        let even = Harness.capture(
            HStack {
                Text(title).lineLimit(1).probe("first")
                Text(title).lineLimit(1).probe("second")
            }.frame(width: 200).probe("row"),
            scheme: scheme)
        store.put("priority.even.first_width", scheme, even.frame("first").width)
        store.put("priority.even.second_width", scheme, even.frame("second").width)
        let ranked = Harness.capture(
            HStack {
                Text(title).lineLimit(1).probe("first")
                Text(title).lineLimit(1).layoutPriority(1).probe("second")
            }.frame(width: 200).probe("row"),
            scheme: scheme)
        store.put("priority.ranked.first_width", scheme, ranked.frame("first").width)
        store.put("priority.ranked.second_width", scheme, ranked.frame("second").width)
        let flexible = Harness.capture(
            HStack(spacing: 0) {
                Color.gray.frame(maxWidth: .infinity).probe("low")
                Color.gray.frame(maxWidth: .infinity).layoutPriority(1).probe("high")
            }.frame(width: 200, height: 10),
            scheme: scheme)
        store.put("priority.flexible.low_width", scheme, flexible.frame("low").width)
        store.put("priority.flexible.high_width", scheme, flexible.frame("high").width)
        let bounded = Harness.capture(
            HStack(spacing: 0) {
                Color.gray.frame(minWidth: 20, maxWidth: .infinity).probe("low")
                Color.gray.frame(maxWidth: 120).layoutPriority(1).probe("high")
            }.frame(width: 200, height: 10),
            scheme: scheme)
        store.put("priority.bounded.low_width", scheme, bounded.frame("low").width)
        store.put("priority.bounded.high_width", scheme, bounded.frame("high").width)
        let column = Harness.capture(
            VStack(spacing: 0) {
                Text(paragraph).probe("low")
                Text(paragraph).layoutPriority(1).probe("high")
            }.frame(width: 120, height: 80),
            scheme: scheme)
        store.put("priority.vertical.low_height", scheme, column.frame("low").height)
        store.put("priority.vertical.high_height", scheme, column.frame("high").height)
    }

    static func fixed(_ store: Store, _ scheme: Scheme) {
        let both = Harness.capture(
            VStack { Text(paragraph).fixedSize().probe("text") }.frame(width: 100, height: 60),
            scheme: scheme)
        store.put("fixed.both.text_width", scheme, both.frame("text").width)
        store.put("fixed.both.text_height", scheme, both.frame("text").height)
        let vertical = Harness.capture(
            VStack { Text(paragraph).fixedSize(horizontal: false, vertical: true).probe("text") }.frame(width: 100, height: 20),
            scheme: scheme)
        store.put("fixed.vertical.text_width", scheme, vertical.frame("text").width)
        store.put("fixed.vertical.text_height", scheme, vertical.frame("text").height)
        let free = Harness.capture(
            VStack { Text(paragraph).probe("text") }.frame(width: 100, height: 20),
            scheme: scheme)
        store.put("fixed.none.text_width", scheme, free.frame("text").width)
        store.put("fixed.none.text_height", scheme, free.frame("text").height)
        let ideal = Harness.capture(
            VStack { Color.gray.frame(idealWidth: 120, idealHeight: 30).fixedSize().probe("box") }.frame(width: 300, height: 100),
            scheme: scheme)
        store.put("fixed.ideal.width", scheme, ideal.frame("box").width)
        store.put("fixed.ideal.height", scheme, ideal.frame("box").height)
        let labels = Harness.capture(
            HStack(spacing: 0) {
                Text("Short").probe("short")
                Text("A longer label").probe("long")
            }.fixedSize().probe("row"),
            scheme: scheme)
        store.put("fixed.row.width", scheme, labels.frame("row").width)
        store.put("fixed.row.short_width", scheme, labels.frame("short").width)
        store.put("fixed.row.long_width", scheme, labels.frame("long").width)
    }

    static func position(_ store: Store, _ scheme: Scheme) {
        let shot = Harness.capture(
            Color.gray.frame(width: 20, height: 20).probe("dot").position(x: 50, y: 30).probe("place").frame(width: 200, height: 100).probe("box"),
            scheme: scheme)
        let box = shot.frame("box")
        store.put("position.dot.x", scheme, shot.frame("dot").minX - box.minX)
        store.put("position.dot.y", scheme, shot.frame("dot").minY - box.minY)
        store.put("position.wrapper.width", scheme, shot.frame("place").width)
        store.put("position.wrapper.height", scheme, shot.frame("place").height)
        let stacked = Harness.capture(
            VStack(spacing: 0) {
                Color.gray.frame(width: 20, height: 20).position(x: 10, y: 10).probe("place")
                Color.gray.frame(width: 20, height: 20).probe("after")
            }.frame(width: 100, height: 100).probe("box"),
            scheme: scheme)
        store.put("position.in_vstack.wrapper_height", scheme, stacked.frame("place").height)
        store.put("position.in_vstack.after_y", scheme, stacked.frame("after").minY - stacked.frame("box").minY)
    }

    static func zOrder(_ store: Store, _ scheme: Scheme) {
        let shot = Harness.capture(
            ZStack {
                Color.red.frame(width: 40, height: 40).zIndex(1)
                Color.blue.frame(width: 40, height: 40)
            }.probe("stack"),
            scheme: scheme)
        let box = shot.frame("stack")
        store.colour("z_index.top", scheme, shot.raster.sampleFlat(CGPoint(x: box.midX, y: box.midY)))
        let row = Harness.capture(
            HStack(spacing: -10) {
                Color.red.frame(width: 30, height: 30).zIndex(1).probe("first")
                Color.blue.frame(width: 30, height: 30).probe("second")
            },
            scheme: scheme)
        let overlap = row.frame("first").maxX - 5
        store.colour("z_index.overlap", scheme, row.raster.sampleFlat(CGPoint(x: overlap, y: row.frame("first").midY)))
        store.put("z_index.layout.second_x", scheme, row.frame("second").minX - row.frame("first").minX)
    }

    static func relative(_ store: Store, _ scheme: Scheme) {
        let three = Harness.capture(
            ScrollView(.horizontal) {
                HStack(spacing: 10) {
                    ForEach(0..<5, id: \.self) { index in
                        Color.gray.containerRelativeFrame(.horizontal, count: 3, spacing: 10).probe("c\(index)")
                    }
                }
            }.frame(width: 320, height: 50).probe("view"),
            scheme: scheme, settle: 0.3)
        store.put("relative.count3.width", scheme, three.frame("c0").width)
        store.put("relative.count3.height", scheme, three.frame("c0").height)
        store.put("relative.count3.second_x", scheme, three.frame("c1").minX - three.frame("view").minX)
        let spanned = Harness.capture(
            ScrollView(.horizontal) {
                HStack(spacing: 10) {
                    Color.gray.containerRelativeFrame(.horizontal, count: 3, span: 2, spacing: 10).probe("c0")
                    Color.gray.containerRelativeFrame(.horizontal, count: 3, spacing: 10).probe("c1")
                }
            }.frame(width: 320, height: 50),
            scheme: scheme, settle: 0.3)
        store.put("relative.span2.width", scheme, spanned.frame("c0").width)
        let margins = Harness.capture(
            ScrollView(.horizontal) {
                HStack(spacing: 10) {
                    ForEach(0..<4, id: \.self) { index in
                        Color.gray.containerRelativeFrame(.horizontal, count: 3, spacing: 10).probe("c\(index)")
                    }
                }
            }.contentMargins(.horizontal, 20).frame(width: 320, height: 50).probe("view"),
            scheme: scheme, settle: 0.3)
        store.put("relative.margins.width", scheme, margins.frame("c0").width)
        store.put("relative.margins.first_x", scheme, margins.frame("c0").minX - margins.frame("view").minX)
        let closure = Harness.capture(
            ScrollView(.horizontal) {
                Color.gray.containerRelativeFrame(.horizontal) { length, _ in length * 0.5 }.frame(height: 20).probe("c")
            }.frame(width: 300, height: 200).probe("view"),
            scheme: scheme, settle: 0.3)
        store.put("relative.closure.width", scheme, closure.frame("c").width)
        store.put("relative.closure.height", scheme, closure.frame("c").height)
        store.put("relative.closure.x", scheme, closure.frame("c").minX - closure.frame("view").minX)
        let aligned = Harness.capture(
            ScrollView(.horizontal) {
                Text("Hi").probe("text").containerRelativeFrame(.horizontal, alignment: .leading).probe("t")
            }.frame(width: 300, height: 100).probe("view"),
            scheme: scheme, settle: 0.3)
        store.put("relative.aligned.width", scheme, aligned.frame("t").width)
        store.put("relative.aligned.x", scheme, aligned.frame("t").minX - aligned.frame("view").minX)
        store.put("relative.aligned.text_x", scheme, aligned.frame("text").minX - aligned.frame("view").minX)
    }

    static func reader(_ store: Store, _ scheme: Scheme) {
        let half = Harness.capture(
            GeometryReader { proxy in
                Color.gray.frame(width: proxy.size.width / 2, height: 10).probe("child")
            }.frame(width: 200, height: 100).probe("reader"),
            scheme: scheme)
        let box = half.frame("reader")
        store.put("reader.half.child_width", scheme, half.frame("child").width)
        store.put("reader.half.child_x", scheme, half.frame("child").minX - box.minX)
        store.put("reader.half.child_y", scheme, half.frame("child").minY - box.minY)
        let stacked = Harness.capture(
            VStack(spacing: 0) {
                Color.gray.frame(width: 40, height: 30).probe("top")
                GeometryReader { _ in Color.clear }.probe("reader")
            }.frame(width: 200, height: 100).probe("box"),
            scheme: scheme)
        store.put("reader.in_vstack.width", scheme, stacked.frame("reader").width)
        store.put("reader.in_vstack.height", scheme, stacked.frame("reader").height)
        let scrolled = Harness.capture(
            ScrollView { GeometryReader { _ in Color.clear }.probe("reader") }.frame(width: 200, height: 100),
            scheme: scheme, settle: 0.3)
        store.put("reader.in_scroll.width", scheme, scrolled.frame("reader").width)
        store.put("reader.in_scroll.height", scheme, scrolled.frame("reader").height)
    }

    static func named(_ store: Store, _ scheme: Scheme) {
        let shot = Harness.capture(
            VStack(spacing: 0) {
                Color.clear.frame(width: 10, height: 30)
                GeometryReader { proxy in
                    let inner = proxy.frame(in: .named("outer"))
                    Color.gray.frame(width: inner.minY + 1, height: 10).probe("measure")
                }.frame(width: 100, height: 20)
            }.padding(7).coordinateSpace(.named("outer")),
            scheme: scheme)
        store.put("named.reader.min_y", scheme, shot.frame("measure").width - 1)
    }

    static func wrapped(_ store: Store, _ scheme: Scheme) {
        let shot = Harness.capture(
            Text(paragraph).probe("text").frame(width: 300).probe("box"),
            scheme: scheme)
        store.put("wrapped.wide_frame.text_width", scheme, shot.frame("text").width)
        store.put("wrapped.wide_frame.text_x", scheme, shot.frame("text").minX - shot.frame("box").minX)
        let narrow = Harness.capture(
            Text(paragraph).probe("text").frame(width: 200).probe("box"),
            scheme: scheme)
        store.put("wrapped.narrow_frame.text_width", scheme, narrow.frame("text").width)
        store.put("wrapped.narrow_frame.text_x", scheme, narrow.frame("text").minX - narrow.frame("box").minX)
        let capped = Harness.capture(
            VStack(spacing: 0) {
                Text(paragraph).probe("text").frame(maxWidth: 300).probe("frame")
                Color.gray.frame(width: 400, height: 4)
            }.probe("box"),
            scheme: scheme)
        store.put("wrapped.max_frame.frame_width", scheme, capped.frame("frame").width)
        store.put("wrapped.max_frame.text_width", scheme, capped.frame("text").width)
        store.put("wrapped.max_frame.text_x", scheme, capped.frame("text").minX - capped.frame("frame").minX)
        let short = Harness.capture(
            VStack(spacing: 0) {
                Text("Hi").probe("text").frame(maxWidth: 300).probe("frame")
                Color.gray.frame(width: 400, height: 4)
            }.probe("box"),
            scheme: scheme)
        store.put("wrapped.max_frame_short.frame_width", scheme, short.frame("frame").width)
        let stacked = Harness.capture(
            VStack(alignment: .leading, spacing: 0) {
                Text(paragraph).probe("text")
                Color.gray.frame(width: 200, height: 4).probe("bar")
            }.frame(width: 200).probe("box"),
            scheme: scheme)
        store.put("wrapped.vstack.text_width", scheme, stacked.frame("text").width)
        store.put("wrapped.vstack.box_width", scheme, stacked.frame("box").width)
    }

    static func gridRow(_ store: Store, _ scheme: Scheme) {
        let plain = Harness.capture(
            Grid(horizontalSpacing: 0, verticalSpacing: 0) {
                GridRow {
                    Color.gray.frame(width: 30, height: 20).probe("a")
                    Color.gray.frame(width: 40, height: 20).probe("b")
                }
                GridRow {
                    Color.gray.frame(width: 30, height: 20).probe("c")
                    Color.gray.frame(width: 40, height: 20).probe("d")
                }
            }.probe("grid"),
            scheme: scheme)
        let g = plain.frame("grid")
        store.put("grid_row.plain.grid_width", scheme, g.width)
        store.put("grid_row.plain.grid_height", scheme, g.height)
        let padded = Harness.capture(
            Grid(horizontalSpacing: 0, verticalSpacing: 0) {
                GridRow {
                    Color.gray.frame(width: 30, height: 20).probe("a")
                    Color.gray.frame(width: 40, height: 20).probe("b")
                }.padding(10).probe("row")
                GridRow {
                    Color.gray.frame(width: 30, height: 20).probe("c")
                    Color.gray.frame(width: 40, height: 20).probe("d")
                }
            }.probe("grid"),
            scheme: scheme)
        let p = padded.frame("grid")
        store.put("grid_row.padded.grid_width", scheme, p.width)
        store.put("grid_row.padded.grid_height", scheme, p.height)
        store.put("grid_row.padded.a_x", scheme, padded.frame("a").minX - p.minX)
        store.put("grid_row.padded.a_y", scheme, padded.frame("a").minY - p.minY)
        store.put("grid_row.padded.b_x", scheme, padded.frame("b").minX - p.minX)
        store.put("grid_row.padded.c_x", scheme, padded.frame("c").minX - p.minX)
        store.put("grid_row.padded.c_y", scheme, padded.frame("c").minY - p.minY)
        store.put("grid_row.padded.d_x", scheme, padded.frame("d").minX - p.minX)
        store.put("grid_row.padded.row_width", scheme, padded.frame("row").width)
        store.put("grid_row.padded.row_height", scheme, padded.frame("row").height)
        let framed = Harness.capture(
            Grid(horizontalSpacing: 0, verticalSpacing: 0) {
                GridRow {
                    Color.gray.frame(width: 30, height: 20).probe("a")
                    Color.gray.frame(width: 40, height: 20).probe("b")
                }.frame(height: 50).probe("row")
                GridRow {
                    Color.gray.frame(width: 30, height: 20).probe("c")
                    Color.gray.frame(width: 40, height: 20).probe("d")
                }
            }.probe("grid"),
            scheme: scheme)
        let f = framed.frame("grid")
        store.put("grid_row.framed.grid_width", scheme, f.width)
        store.put("grid_row.framed.grid_height", scheme, f.height)
        store.put("grid_row.framed.a_x", scheme, framed.frame("a").minX - f.minX)
        store.put("grid_row.framed.a_y", scheme, framed.frame("a").minY - f.minY)
        store.put("grid_row.framed.b_x", scheme, framed.frame("b").minX - f.minX)
        store.put("grid_row.framed.c_y", scheme, framed.frame("c").minY - f.minY)
        store.put("grid_row.framed.row_height", scheme, framed.frame("row").height)
    }

    static func listRow(_ store: Store, _ scheme: Scheme) {
        let shot = Harness.capture(
            List {
                Text("Text row").probe("text")
                Button("Button row") {}.probe("button")
                HStack { Color.gray.frame(width: 20, height: 20); Color.gray.frame(width: 20, height: 20) }.probe("pair")
            }
            .listStyle(.inset)
            .frame(width: 300, height: 140)
            .probe("list"),
            scheme: scheme, settle: 0.3)
        let box = shot.frame("list")
        store.put("list_row.text.x", scheme, shot.frame("text").minX - box.minX)
        store.put("list_row.button.x", scheme, shot.frame("button").minX - box.minX)
        store.put("list_row.pair.x", scheme, shot.frame("pair").minX - box.minX)
        store.put("list_row.pair.width", scheme, shot.frame("pair").width)
    }

    static func crossAnchor(_ store: Store, _ scheme: Scheme) {
        for (name, role) in [("all", nil), ("alignment", ScrollAnchorRole.alignment), ("initial_offset", ScrollAnchorRole.initialOffset), ("size_changes", ScrollAnchorRole.sizeChanges)] as [(String, ScrollAnchorRole?)] {
            let content = ScrollView { Scrolling.rows(3) }
            let anchored: AnyView = role.map { AnyView(content.defaultScrollAnchor(.bottomLeading, for: $0)) } ?? AnyView(content.defaultScrollAnchor(.bottomLeading))
            let shot = Harness.capture(
                anchored.frame(width: 300, height: 200).probe("view"),
                scheme: scheme, settle: 0.3)
            store.put("cross_anchor.\(name).row_x", scheme, shot.frame("row0").minX - shot.frame("view").minX)
            store.put("cross_anchor.\(name).row_y", scheme, shot.frame("row0").minY - shot.frame("view").minY)
        }
        let trailing = Harness.capture(
            ScrollView(.horizontal) {
                HStack(spacing: 0) { Color.gray.frame(width: 30, height: 30).probe("cell") }
            }.defaultScrollAnchor(.bottomTrailing).frame(width: 300, height: 200).probe("view"),
            scheme: scheme, settle: 0.3)
        store.put("cross_anchor.horizontal.cell_y", scheme, trailing.frame("cell").minY - trailing.frame("view").minY)
        store.put("cross_anchor.horizontal.cell_x", scheme, trailing.frame("cell").minX - trailing.frame("view").minX)
    }

    static let bitmap = NSImage(size: NSSize(width: 40, height: 20), flipped: false) { rect in
        NSColor.systemBlue.setFill()
        rect.fill()
        return true
    }

    static func picture() -> Image { Image(nsImage: bitmap) }

    static func size(_ store: Store, _ key: String, _ scheme: Scheme, _ shot: Shot, _ name: String = "image") {
        store.put("\(key)_width", scheme, shot.frame(name).width)
        store.put("\(key)_height", scheme, shot.frame(name).height)
    }

    static func aspectFit(_ store: Store, _ scheme: Scheme) {
        let variants: [(String, AnyView)] = [
            ("ratio_fit", AnyView(picture().resizable().aspectRatio(0.5, contentMode: .fit))),
            ("ratio_fill", AnyView(picture().resizable().aspectRatio(0.5, contentMode: .fill))),
            ("scaled_fit", AnyView(picture().resizable().scaledToFit())),
            ("scaled_fill", AnyView(picture().resizable().scaledToFill())),
        ]
        for (name, view) in variants {
            let column = Harness.capture(
                ScrollView { VStack(spacing: 0) { view.probe("image") } }.frame(width: 200, height: 300),
                scheme: scheme, settle: 0.3)
            size(store, "aspect.column.\(name)", scheme, column)
            let row = Harness.capture(
                ScrollView(.horizontal) { HStack(spacing: 0) { view.probe("image") } }.frame(width: 300, height: 100),
                scheme: scheme, settle: 0.3)
            size(store, "aspect.row.\(name)", scheme, row)
            let both = Harness.capture(
                ScrollView([.horizontal, .vertical]) { view.probe("image") }.frame(width: 200, height: 300),
                scheme: scheme, settle: 0.3)
            size(store, "aspect.both.\(name)", scheme, both)
            let fixed = Harness.capture(
                VStack(spacing: 0) { view.fixedSize().probe("image") }.frame(width: 200, height: 300),
                scheme: scheme)
            size(store, "aspect.fixed.\(name)", scheme, fixed)
            let bounded = Harness.capture(
                VStack(spacing: 0) { view.probe("image") }.frame(width: 200, height: 300),
                scheme: scheme)
            size(store, "aspect.bounded.\(name)", scheme, bounded)
            let framed = Harness.capture(
                VStack(spacing: 0) { view.probe("image").frame(width: 60, height: 20).probe("frame") }.frame(width: 200, height: 300),
                scheme: scheme)
            size(store, "aspect.framed.\(name)", scheme, framed)
            store.put("aspect.framed.\(name)_x", scheme, framed.frame("image").minX - framed.frame("frame").minX)
            store.put("aspect.framed.\(name)_y", scheme, framed.frame("image").minY - framed.frame("frame").minY)
        }
    }

    static func stackFlexibility(_ store: Store, _ scheme: Scheme) {
        let column = Harness.capture(
            ScrollView { VStack { Color.red.frame(maxHeight: .infinity).probe("child") }.probe("stack") }.frame(width: 200, height: 300),
            scheme: scheme, settle: 0.3)
        size(store, "flex.scroll_vstack.stack", scheme, column, "stack")
        size(store, "flex.scroll_vstack.child", scheme, column, "child")
        let layered = Harness.capture(
            ScrollView { ZStack { Color.red.frame(maxHeight: .infinity).probe("child") }.probe("stack") }.frame(width: 200, height: 300),
            scheme: scheme, settle: 0.3)
        size(store, "flex.scroll_zstack.stack", scheme, layered, "stack")
        size(store, "flex.scroll_zstack.child", scheme, layered, "child")
        let colour = Harness.capture(
            ScrollView { VStack { Color.red.probe("child") }.probe("stack") }.frame(width: 200, height: 300),
            scheme: scheme, settle: 0.3)
        size(store, "flex.scroll_colour.stack", scheme, colour, "stack")
        let sideways = Harness.capture(
            ScrollView(.horizontal) { HStack { VStack { Color.red.frame(maxWidth: .infinity).probe("child") }.probe("stack") } }.frame(width: 200, height: 100),
            scheme: scheme, settle: 0.3)
        size(store, "flex.hscroll_vstack.stack", scheme, sideways, "stack")
        let across = Harness.capture(
            HStack(spacing: 0) {
                VStack { Color.red.frame(maxWidth: .infinity) }.probe("stack")
                Color.gray.frame(width: 50)
            }.frame(width: 200, height: 40),
            scheme: scheme)
        size(store, "flex.row_vstack.stack", scheme, across, "stack")
        let along = Harness.capture(
            VStack(spacing: 0) {
                Color.gray.frame(height: 30)
                ZStack { Color.red.frame(maxHeight: .infinity) }.probe("stack")
            }.frame(width: 100, height: 100),
            scheme: scheme)
        size(store, "flex.column_zstack.stack", scheme, along, "stack")
        let mixed = Harness.capture(
            HStack(spacing: 0) {
                VStack { Color.gray.frame(width: 30, height: 10) }.probe("rigid")
                VStack { Color.red.frame(maxWidth: .infinity) }.probe("stack")
            }.frame(width: 200, height: 40),
            scheme: scheme)
        size(store, "flex.mixed.rigid", scheme, mixed, "rigid")
        size(store, "flex.mixed.stack", scheme, mixed, "stack")
        let nested = Harness.capture(
            VStack(spacing: 0) {
                Color.gray.frame(height: 30)
                VStack { HStack { Color.red.frame(maxHeight: .infinity) } }.probe("stack")
            }.frame(width: 100, height: 100),
            scheme: scheme)
        size(store, "flex.nested.stack", scheme, nested, "stack")
        let spaced = Harness.capture(
            VStack(spacing: 0) {
                HStack(spacing: 0) { Color.gray.frame(width: 30, height: 10); Spacer() }.probe("stack")
            }.frame(width: 200, height: 100),
            scheme: scheme)
        size(store, "flex.spacer.stack", scheme, spaced, "stack")
        let divided = Harness.capture(
            VStack(spacing: 0) {
                HStack(spacing: 0) { Color.gray.frame(width: 30, height: 10); Divider() }.probe("stack")
            }.frame(width: 200, height: 100),
            scheme: scheme)
        size(store, "flex.divider.stack", scheme, divided, "stack")
        let pressed = Harness.capture(
            VStack(spacing: 0) {
                HStack(spacing: 0) { Button {} label: { Color.gray.frame(maxWidth: .infinity).frame(height: 10) }.buttonStyle(.plain) }.probe("stack")
            }.frame(width: 200, height: 100),
            scheme: scheme)
        size(store, "flex.button.stack", scheme, pressed, "stack")
    }
}
