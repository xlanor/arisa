import AppKit
import SwiftUI

enum Tables {
    struct Item: Identifiable {
        let id: Int
        let name: String
        let kind: String
        let size: String
    }

    static let items = [
        Item(id: 0, name: "Alpha", kind: "Folder", size: "12"),
        Item(id: 1, name: "Bravo", kind: "Image", size: "340"),
        Item(id: 2, name: "Charlie", kind: "Text", size: "5"),
        Item(id: 3, name: "Delta", kind: "Movie", size: "9000"),
    ]

    static func run(_ store: Store) {
        for scheme in Scheme.allCases {
            styles(store, scheme)
        }
        let scheme = Scheme.light
        widths(store, scheme)
        headers(store, scheme)
        sorted(store, scheme)
        badges(store, scheme)
        borderedList(store, scheme)
    }

    static func table(_ style: String) -> AnyView {
        let base = Table(items) {
            TableColumn("Name", value: \.name)
            TableColumn("Kind", value: \.kind)
            TableColumn("Size", value: \.size)
        }
        switch style {
        case "inset":
            return AnyView(base.tableStyle(.inset))
        case "bordered":
            return AnyView(base.tableStyle(.bordered))
        case "bordered_plain":
            return AnyView(base.tableStyle(.bordered(alternatesRowBackgrounds: false)))
        case "inset_plain":
            return AnyView(base.tableStyle(.inset(alternatesRowBackgrounds: false)))
        default:
            return AnyView(base)
        }
    }

    static func rowLines(_ shot: Shot, _ box: CGRect) -> [CGRect] {
        shot.raster.inkLines(in: CGRect(x: box.minX + 2, y: box.minY + 1, width: 90, height: box.height - 2), contrast: 0.3)
    }

    static func horizontalRules(_ shot: Shot, x: CGFloat, _ box: CGRect) -> [(CGFloat, RGBA)] {
        var found: [(CGFloat, RGBA)] = []
        var previous = shot.raster.sampleFlat(CGPoint(x: x, y: box.minY + 0.25))
        var y = box.minY + 0.5
        while y < box.maxY {
            let c = shot.raster.sampleFlat(CGPoint(x: x, y: y))
            if c.distance(previous) > 0.015 {
                found.append((y - box.minY, c))
            }
            previous = c
            y += 0.5
        }
        return found
    }

    static func styles(_ store: Store, _ scheme: Scheme) {
        for style in ["automatic", "inset", "bordered", "bordered_plain", "inset_plain"] {
            let view = table(style).frame(width: 360, height: 180).probe("table")
            let shot = Harness.capture(view, scheme: scheme, settle: 0.4)
            let box = shot.frame("table")
            let text = rowLines(shot, box)
            store.put("table.\(style).text_lines", scheme, Double(text.count), "")
            for (index, line) in text.prefix(6).enumerated() {
                store.put("table.\(style).line\(index).top", scheme, line.minY - box.minY)
                store.put("table.\(style).line\(index).bottom", scheme, line.maxY - box.minY)
                store.put("table.\(style).line\(index).leading", scheme, line.minX - box.minX)
            }
            if text.count >= 3 {
                store.put("table.\(style).row_pitch", scheme, text[2].maxY - text[1].maxY)
                store.put("table.\(style).header_to_row_baseline", scheme, text[1].maxY - text[0].maxY)
            }
            let rules = horizontalRules(shot, x: box.maxX - 30, box)
            store.put("table.\(style).rule_count", scheme, Double(rules.count), "")
            for (index, rule) in rules.prefix(14).enumerated() {
                store.put("table.\(style).rule\(index).y", scheme, rule.0)
                store.colour("table.\(style).rule\(index).colour", scheme, rule.1)
            }
            store.colour("table.\(style).header_fill", scheme, shot.raster.sampleFlat(CGPoint(x: box.maxX - 30, y: box.minY + 8)))
            if text.count >= 3 {
                store.colour("table.\(style).row0_fill", scheme, shot.raster.sampleFlat(CGPoint(x: box.maxX - 30, y: text[1].midY)))
                store.colour("table.\(style).row1_fill", scheme, shot.raster.sampleFlat(CGPoint(x: box.maxX - 30, y: text[2].midY)))
            }
            if let header = text.first {
                let y = header.midY
                var columns: [CGFloat] = []
                let row = shot.raster.inkLines(in: CGRect(x: box.minX + 1, y: y - 1, width: box.width - 2, height: 2), contrast: 0.3)
                _ = row
                var x = box.minX + 1
                var inside = false
                let base = shot.raster.sampleFlat(CGPoint(x: box.minX + 1, y: header.minY - 2))
                while x < box.maxX - 1 {
                    var ink = false
                    var yy = header.minY
                    while yy <= header.maxY {
                        if shot.raster.sampleFlat(CGPoint(x: x, y: yy)).distance(base) > 0.3 { ink = true; break }
                        yy += 0.5
                    }
                    if ink && !inside { columns.append(x - box.minX) }
                    inside = ink
                    x += 0.5
                }
                for (index, start) in columns.prefix(12).enumerated() {
                    store.put("table.\(style).header_ink\(index).x", scheme, start)
                }
            }
            store.image("table_\(style)", shot, box, pad: 0)
        }
    }

    static func widths(_ store: Store, _ scheme: Scheme) {
        let cases: [(String, AnyView)] = [
            ("fixed", AnyView(Table(items) {
                TableColumn("Name", value: \.name).width(80)
                TableColumn("Kind", value: \.kind).width(120)
                TableColumn("Size", value: \.size)
            })),
            ("range", AnyView(Table(items) {
                TableColumn("Name", value: \.name).width(min: 50, ideal: 100, max: 140)
                TableColumn("Kind", value: \.kind)
                TableColumn("Size", value: \.size).width(60)
            })),
            ("two", AnyView(Table(items) {
                TableColumn("Name", value: \.name)
                TableColumn("Kind", value: \.kind)
            })),
        ]
        for (name, base) in cases {
            for style in ["inset", "bordered"] {
                let styled = style == "inset" ? AnyView(base.tableStyle(.inset)) : AnyView(base.tableStyle(.bordered))
                let view = styled.frame(width: 360, height: 150).probe("table")
                let shot = Harness.capture(view, scheme: scheme, settle: 0.4)
                let box = shot.frame("table")
                let text = rowLines(shot, box)
                if let header = text.first {
                    var x = box.minX + 1
                    var inside = false
                    var columns: [CGFloat] = []
                    let base = shot.raster.sampleFlat(CGPoint(x: box.minX + 1, y: header.minY - 2))
                    while x < box.maxX - 1 {
                        var ink = false
                        var yy = header.minY
                        while yy <= header.maxY {
                            if shot.raster.sampleFlat(CGPoint(x: x, y: yy)).distance(base) > 0.3 { ink = true; break }
                            yy += 0.5
                        }
                        if ink && !inside { columns.append(x - box.minX) }
                        inside = ink
                        x += 0.5
                    }
                    for (index, start) in columns.prefix(12).enumerated() {
                        store.put("table.width.\(name).\(style).header_ink\(index).x", scheme, start)
                    }
                }
                store.image("table_width_\(name)_\(style)", shot, box, pad: 0)
            }
        }
    }

    static func headers(_ store: Store, _ scheme: Scheme) {
        let view = Table(items) {
            TableColumn("Name", value: \.name)
            TableColumn("Kind", value: \.kind)
        }
        .tableColumnHeaders(.hidden)
        .tableStyle(.inset)
        .frame(width: 360, height: 130)
        .probe("table")
        let shot = Harness.capture(view, scheme: scheme, settle: 0.4)
        let box = shot.frame("table")
        let text = rowLines(shot, box)
        for (index, line) in text.prefix(4).enumerated() {
            store.put("table.headers_hidden.line\(index).top", scheme, line.minY - box.minY)
            store.put("table.headers_hidden.line\(index).bottom", scheme, line.maxY - box.minY)
        }
        store.image("table_headers_hidden", shot, box, pad: 0)
    }

    struct SortedTable: View {
        @State var order = [KeyPathComparator(\Item.name, order: .reverse)]
        var body: some View {
            Table(items.sorted(using: order), sortOrder: $order) {
                TableColumn("Name", value: \.name)
                TableColumn("Kind", value: \.kind)
            }
            .tableStyle(.inset)
        }
    }

    static func sorted(_ store: Store, _ scheme: Scheme) {
        let view = SortedTable().frame(width: 360, height: 130).probe("table")
        let shot = Harness.capture(view, scheme: scheme, settle: 0.4)
        let box = shot.frame("table")
        let text = rowLines(shot, box)
        if let header = text.first {
            let right = CGRect(x: box.minX + 60, y: header.minY - 2, width: 120, height: header.height + 4)
            let background = shot.raster.sampleFlat(CGPoint(x: box.minX + 170, y: header.minY - 3))
            if let ink = shot.raster.differingBounds(in: right, from: background, tolerance: 0.25) {
                store.put("table.sort.indicator.x", scheme, ink.minX - box.minX)
                store.put("table.sort.indicator.width", scheme, ink.width)
                store.put("table.sort.indicator.height", scheme, ink.height)
                store.colour("table.sort.indicator.colour", scheme, shot.raster.extreme(in: ink, from: background))
            }
        }
        store.image("table_sorted", shot, box, pad: 0)
    }

    static func badges(_ store: Store, _ scheme: Scheme) {
        for (name, prominence) in [("decreased", BadgeProminence.decreased), ("standard", .standard), ("increased", .increased)] {
            let view = List {
                Text("Inbox").badge(5).probe("row")
            }
            .badgeProminence(prominence)
            .listStyle(.inset)
            .frame(width: 300, height: 60)
            .probe("list")
            let shot = Harness.capture(view, scheme: scheme, settle: 0.3)
            let box = shot.frame("list")
            let row = shot.frame("row")
            let right = CGRect(x: box.midX, y: row.minY - 4, width: box.maxX - box.midX - 2, height: row.height + 8)
            let background = shot.raster.sampleFlat(CGPoint(x: box.midX, y: row.minY - 3))
            if let ink = shot.raster.differingBounds(in: right, from: background, tolerance: 0.1) {
                store.put("list.badge_prominence.\(name).ink_trailing_gap", scheme, box.maxX - ink.maxX)
                store.put("list.badge_prominence.\(name).ink_height", scheme, ink.height)
                store.put("list.badge_prominence.\(name).ink_width", scheme, ink.width)
                store.colour("list.badge_prominence.\(name).ink", scheme, shot.raster.extreme(in: ink, from: background))
            }
            store.image("list_badge_prominence_\(name)", shot, box, pad: 0)
        }
    }

    static func borderedList(_ store: Store, _ scheme: Scheme) {
        for (name, alternates) in [("plain", false), ("alternating", true)] {
            let view = List {
                Text("Alpha")
                Text("Bravo")
                Text("Charlie")
            }
            .listStyle(.bordered(alternatesRowBackgrounds: alternates))
            .frame(width: 300, height: 120)
            .probe("list")
            let shot = Harness.capture(view, scheme: scheme, settle: 0.3)
            let box = shot.frame("list")
            let text = shot.raster.inkLines(in: CGRect(x: box.minX + 4, y: box.minY + 2, width: box.width * 0.5, height: box.height - 4), contrast: 0.3)
            for (index, line) in text.prefix(3).enumerated() {
                store.put("list.bordered_style.\(name).line\(index).top", scheme, line.minY - box.minY)
                store.put("list.bordered_style.\(name).line\(index).leading", scheme, line.minX - box.minX)
                store.colour("list.bordered_style.\(name).row\(index)_fill", scheme, shot.raster.sampleFlat(CGPoint(x: box.maxX - 20, y: line.midY)))
            }
            store.colour("list.bordered_style.\(name).edge", scheme, shot.raster.sampleFlat(CGPoint(x: box.minX + 0.25, y: box.midY)))
            store.image("list_bordered_style_\(name)", shot, box, pad: 0)
        }
    }
}
