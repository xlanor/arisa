import AppKit
import SwiftUI

enum Grids {
    static func run(_ store: Store) {
        let scheme = Scheme.light
        lazy(store, scheme)
        eager(store, scheme)
    }

    static func cells(_ count: Int, height: CGFloat = 20) -> some View {
        ForEach(0..<count, id: \.self) { index in
            Color.red.frame(height: height).probe("c\(index)")
        }
    }

    static func tracks(_ store: Store, _ scheme: Scheme, _ name: String, _ columns: [GridItem], width: CGFloat, count: Int, spacing: CGFloat? = nil) {
        let view = LazyVGrid(columns: columns, spacing: spacing) { cells(count) }.probe("g").frame(width: width).probe("o")
        let shot = Harness.capture(view, scheme: scheme)
        let outer = shot.frame("o")
        let grid = shot.frame("g")
        store.put("grid.lazy.\(name).grid_x", nil, grid.minX - outer.minX)
        store.put("grid.lazy.\(name).grid_width", nil, grid.width)
        store.put("grid.lazy.\(name).grid_height", nil, grid.height)
        for index in 0..<count {
            let cell = shot.frame("c\(index)")
            store.put("grid.lazy.\(name).c\(index).x", nil, cell.minX - grid.minX)
            store.put("grid.lazy.\(name).c\(index).y", nil, cell.minY - grid.minY)
            store.put("grid.lazy.\(name).c\(index).width", nil, cell.width)
        }
    }

    static func lazy(_ store: Store, _ scheme: Scheme) {
        tracks(store, scheme, "flexible3_w300", Array(repeating: GridItem(.flexible()), count: 3), width: 300, count: 4)
        tracks(store, scheme, "adaptive80_w300", [GridItem(.adaptive(minimum: 80))], width: 300, count: 4)
        tracks(store, scheme, "adaptive80_w350", [GridItem(.adaptive(minimum: 80))], width: 350, count: 5)
        tracks(store, scheme, "adaptive80_w170", [GridItem(.adaptive(minimum: 80))], width: 170, count: 3)
        tracks(store, scheme, "adaptive80_w60", [GridItem(.adaptive(minimum: 80))], width: 60, count: 2)
        tracks(store, scheme, "adaptive80max90_w400", [GridItem(.adaptive(minimum: 80, maximum: 90))], width: 400, count: 5)
        tracks(store, scheme, "fixed50_flex_fixed70_w300", [GridItem(.fixed(50)), GridItem(.flexible()), GridItem(.fixed(70))], width: 300, count: 3)
        tracks(store, scheme, "flexmin100_flexmax60_w300", [GridItem(.flexible(minimum: 100)), GridItem(.flexible(maximum: 60))], width: 300, count: 2)
        tracks(store, scheme, "flexmax60_flexmin100_w300", [GridItem(.flexible(maximum: 60)), GridItem(.flexible(minimum: 100))], width: 300, count: 2)
        tracks(store, scheme, "flexmin200_flexmin200_w300", [GridItem(.flexible(minimum: 200)), GridItem(.flexible(minimum: 200))], width: 300, count: 2)
        tracks(store, scheme, "adaptive50_fixed100_w300", [GridItem(.adaptive(minimum: 50)), GridItem(.fixed(100))], width: 300, count: 4)
        tracks(store, scheme, "flex_adaptive50_w300", [GridItem(.flexible()), GridItem(.adaptive(minimum: 50))], width: 300, count: 4)
        tracks(store, scheme, "spacing20_0_w300", [GridItem(.flexible(), spacing: 20), GridItem(.flexible(), spacing: 0), GridItem(.flexible())], width: 300, count: 3)
        tracks(store, scheme, "adaptive80spacing0_w300", [GridItem(.adaptive(minimum: 80), spacing: 0)], width: 300, count: 4)
        tracks(store, scheme, "fixed50x2_w300", [GridItem(.fixed(50)), GridItem(.fixed(50))], width: 300, count: 2)
        tracks(store, scheme, "rowspacing12_w300", Array(repeating: GridItem(.flexible()), count: 2), width: 300, count: 4, spacing: 12)
        tracks(store, scheme, "rowspacing_default_w300", Array(repeating: GridItem(.flexible()), count: 2), width: 300, count: 4)

        let aligned = LazyVGrid(columns: [GridItem(.flexible()), GridItem(.flexible(), alignment: .leading), GridItem(.flexible(), alignment: .bottomTrailing)]) {
            Color.red.frame(width: 20, height: 20).probe("a0")
            Color.red.frame(width: 20, height: 20).probe("a1")
            Color.red.frame(width: 20, height: 20).probe("a2")
            Color.red.frame(width: 20, height: 60).probe("a3")
        }.probe("g").frame(width: 300)
        let shot = Harness.capture(aligned, scheme: scheme)
        let grid = shot.frame("g")
        for index in 0..<4 {
            let cell = shot.frame("a\(index)")
            store.put("grid.lazy.item_alignment.a\(index).x", nil, cell.minX - grid.minX)
            store.put("grid.lazy.item_alignment.a\(index).y", nil, cell.minY - grid.minY)
        }
        let rows = LazyVGrid(columns: Array(repeating: GridItem(.flexible()), count: 2), alignment: .leading) {
            Color.red.frame(width: 20, height: 20).probe("b0")
            Color.red.frame(width: 20, height: 50).probe("b1")
            Color.red.frame(width: 20, height: 20).probe("b2")
        }.probe("g").frame(width: 300)
        let rowShot = Harness.capture(rows, scheme: scheme)
        let rowGrid = rowShot.frame("g")
        for index in 0..<3 {
            let cell = rowShot.frame("b\(index)")
            store.put("grid.lazy.grid_alignment_leading.b\(index).x", nil, cell.minX - rowGrid.minX)
            store.put("grid.lazy.grid_alignment_leading.b\(index).y", nil, cell.minY - rowGrid.minY)
        }
        let sideways = LazyHGrid(rows: [GridItem(.fixed(30)), GridItem(.fixed(40), spacing: 4), GridItem(.fixed(30))], spacing: 6) {
            ForEach(0..<5, id: \.self) { index in
                Color.red.frame(width: 20).probe("h\(index)")
            }
        }.probe("g")
        let hShot = Harness.capture(sideways, scheme: scheme)
        let hGrid = hShot.frame("g")
        store.put("grid.lazy_h.width", nil, hGrid.width)
        store.put("grid.lazy_h.height", nil, hGrid.height)
        for index in 0..<5 {
            let cell = hShot.frame("h\(index)")
            store.put("grid.lazy_h.h\(index).x", nil, cell.minX - hGrid.minX)
            store.put("grid.lazy_h.h\(index).y", nil, cell.minY - hGrid.minY)
            store.put("grid.lazy_h.h\(index).height", nil, cell.height)
        }
        let sectioned = LazyVGrid(columns: Array(repeating: GridItem(.flexible()), count: 3)) {
            Section(header: Color.blue.frame(height: 10).probe("s0h")) {
                ForEach(0..<4, id: \.self) { index in Color.red.frame(height: 20).probe("s0c\(index)") }
            }
            Section(header: Color.blue.frame(height: 10).probe("s1h")) {
                ForEach(0..<2, id: \.self) { index in Color.red.frame(height: 20).probe("s1c\(index)") }
            }
        }.probe("g").frame(width: 300)
        let sShot = Harness.capture(sectioned, scheme: scheme)
        let sGrid = sShot.frame("g")
        for name in ["s0h", "s0c0", "s0c3", "s1h", "s1c0", "s1c1"] {
            let cell = sShot.frame(name)
            store.put("grid.lazy.sections.\(name).x", nil, cell.minX - sGrid.minX)
            store.put("grid.lazy.sections.\(name).y", nil, cell.minY - sGrid.minY)
            store.put("grid.lazy.sections.\(name).width", nil, cell.width)
        }
    }

    static func box(_ name: String, _ width: CGFloat, _ height: CGFloat = 20) -> some View {
        Color.red.frame(width: width, height: height).probe(name)
    }

    static func record(_ store: Store, _ shot: Shot, _ prefix: String, _ names: [String]) {
        let grid = shot.frame("g")
        store.put("\(prefix).grid_width", nil, grid.width)
        store.put("\(prefix).grid_height", nil, grid.height)
        for name in names {
            let cell = shot.frame(name)
            store.put("\(prefix).\(name).x", nil, cell.minX - grid.minX)
            store.put("\(prefix).\(name).y", nil, cell.minY - grid.minY)
            store.put("\(prefix).\(name).width", nil, cell.width)
            store.put("\(prefix).\(name).height", nil, cell.height)
        }
    }

    static func eager(_ store: Store, _ scheme: Scheme) {
        let basic = Grid {
            GridRow { box("r0c0", 30); box("r0c1", 80) }
            GridRow { box("r1c0", 60, 30); box("r1c1", 20) }
            GridRow { box("r2c0", 10) }
        }.probe("g")
        record(store, Harness.capture(basic, scheme: scheme), "grid.eager.basic", ["r0c0", "r0c1", "r1c0", "r1c1", "r2c0"])

        let spaced = Grid(alignment: .topLeading, horizontalSpacing: 4, verticalSpacing: 6) {
            GridRow { box("r0c0", 30); box("r0c1", 80) }
            GridRow { box("r1c0", 60, 30); box("r1c1", 20) }
        }.probe("g")
        record(store, Harness.capture(spaced, scheme: scheme), "grid.eager.spaced", ["r0c0", "r0c1", "r1c0", "r1c1"])

        let columnAligned = Grid {
            GridRow { box("r0c0", 30).gridColumnAlignment(.leading); box("r0c1", 80).gridColumnAlignment(.trailing) }
            GridRow { box("r1c0", 60); box("r1c1", 20) }
        }.probe("g")
        record(store, Harness.capture(columnAligned, scheme: scheme), "grid.eager.column_alignment", ["r0c0", "r0c1", "r1c0", "r1c1"])

        let rowAligned = Grid {
            GridRow(alignment: .top) { box("r0c0", 30, 50); box("r0c1", 30) }
            GridRow(alignment: .bottom) { box("r1c0", 30); box("r1c1", 30, 50) }
        }.probe("g")
        record(store, Harness.capture(rowAligned, scheme: scheme), "grid.eager.row_alignment", ["r0c0", "r0c1", "r1c0", "r1c1"])

        let spanning = Grid {
            GridRow { box("r0c0", 30); box("r0c1", 30); box("r0c2", 30) }
            GridRow { box("r1span", 50).gridCellColumns(2); box("r1c2", 30) }
            GridRow { box("r2wide", 150).gridCellColumns(2); box("r2c2", 30) }
        }.probe("g")
        record(store, Harness.capture(spanning, scheme: scheme), "grid.eager.spanning", ["r0c0", "r0c1", "r0c2", "r1span", "r1c2", "r2wide", "r2c2"])

        let anchored = Grid {
            GridRow { box("r0c0", 80, 60); box("r0c1", 80, 60) }
            GridRow { box("r1c0", 20).gridCellAnchor(.topLeading); box("r1c1", 20).gridCellAnchor(UnitPoint(x: 0.75, y: 1)) }
        }.probe("g")
        record(store, Harness.capture(anchored, scheme: scheme), "grid.eager.anchor", ["r0c0", "r0c1", "r1c0", "r1c1"])

        let divided = Grid {
            GridRow { box("r0c0", 30); box("r0c1", 40) }
            Divider().probe("divider")
            Color.blue.frame(height: 4).probe("bar")
            GridRow { box("r1c0", 30); box("r1c1", 40) }
        }.probe("g")
        record(store, Harness.capture(divided, scheme: scheme), "grid.eager.divided", ["r0c0", "r0c1", "divider", "bar", "r1c0", "r1c1"])

        let dividedWide = Grid {
            GridRow { box("r0c0", 30); box("r0c1", 40) }
            Divider().probe("divider")
            Color.blue.frame(height: 4).probe("bar")
            GridRow { box("r1c0", 30); box("r1c1", 40) }
        }.probe("g").frame(width: 300)
        record(store, Harness.capture(dividedWide, scheme: scheme), "grid.eager.divided_wide", ["r0c0", "r0c1", "divider", "bar", "r1c0", "r1c1"])

        let mixed = Grid {
            GridRow { Text("Name").probe("t0"); box("b0", 20) }
            GridRow { Text("Resolution").probe("t1"); Text("1080p").probe("t2") }
            GridRow { Text("Mode").probe("t3"); Text("Docked").probe("t4") }
        }.probe("g")
        record(store, Harness.capture(mixed, scheme: scheme), "grid.eager.mixed", ["t0", "b0", "t1", "t2", "t3", "t4"])

        let unsized = Grid {
            GridRow { box("r0c0", 30); box("r0c1", 40) }
            Color.blue.frame(height: 4).gridCellUnsizedAxes(.horizontal).probe("bar")
            GridRow { Color.blue.gridCellUnsizedAxes([.horizontal, .vertical]).probe("fill"); box("r1c1", 40) }
        }.probe("g").frame(width: 300)
        record(store, Harness.capture(unsized, scheme: scheme), "grid.eager.unsized", ["r0c0", "r0c1", "bar", "fill", "r1c1"])

        let flexible = Grid {
            GridRow { box("r0c0", 30); Color.blue.frame(height: 20).probe("r0c1"); box("r0c2", 40) }
            GridRow { box("r1c0", 50); box("r1c1", 20); box("r1c2", 20) }
        }.probe("g").frame(width: 300)
        record(store, Harness.capture(flexible, scheme: scheme), "grid.eager.flexible", ["r0c0", "r0c1", "r0c2", "r1c0", "r1c1", "r1c2"])

        let texts = Grid {
            GridRow { Text("Name").probe("t0"); Text("Value").probe("t1") }
            GridRow { Text("Resolution").probe("t2"); Text("1080p").probe("t3") }
        }.probe("g")
        record(store, Harness.capture(texts, scheme: scheme), "grid.eager.texts", ["t0", "t1", "t2", "t3"])
        store.image("grid_eager_texts", Harness.capture(texts, scheme: scheme), Harness.capture(texts, scheme: scheme).frame("g"))
    }
}
