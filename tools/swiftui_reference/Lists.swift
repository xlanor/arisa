import AppKit
import SwiftUI

enum Lists {
    static let names = ["Alpha", "Bravo", "Charlie", "Delta"]

    static func run(_ store: Store) {
        for scheme in Scheme.allCases {
            badges(store, scheme)
            separators(store, scheme)
            backgrounds(store, scheme)
            prominence(store, scheme)
            disclosure(store, scheme)
            outline(store, scheme)
        }
        let scheme = Scheme.light
        insets(store, scheme)
        heights(store, scheme)
        alternating(store, scheme)
    }

    static func rows(_ shot: Shot, _ box: CGRect) -> [CGRect] {
        shot.raster.inkLines(in: CGRect(x: box.minX + 4, y: box.minY, width: box.width * 0.5, height: box.height), contrast: 0.3)
    }

    static func lines(_ shot: Shot, column x: CGFloat, from top: CGFloat, to bottom: CGFloat) -> [(CGFloat, RGBA)] {
        let base = shot.raster.sampleFlat(CGPoint(x: x, y: top))
        var found: [(CGFloat, RGBA)] = []
        var y = top
        var inside = false
        while y < bottom {
            let c = shot.raster.sampleFlat(CGPoint(x: x, y: y))
            let differs = c.distance(base) > 0.02
            if differs && !inside {
                found.append((y, c))
            }
            inside = differs
            y += 0.5
        }
        return found
    }

    static func badges(_ store: Store, _ scheme: Scheme) {
        let view = List {
            Text("Inbox").badge(5).probe("count")
            Text("Drafts").badge("New").probe("text")
            Text("Sent").badge(0).probe("zero")
        }
        .listStyle(.inset)
        .frame(width: 300, height: 140)
        .probe("list")
        let shot = Harness.capture(view, scheme: scheme, settle: 0.3)
        let box = shot.frame("list")
        for name in ["count", "text", "zero"] {
            let row = shot.frame(name)
            let right = CGRect(x: box.midX, y: row.minY, width: box.maxX - box.midX - 2, height: row.height)
            let background = shot.raster.sampleFlat(CGPoint(x: box.midX, y: row.minY + 1))
            if let ink = shot.raster.differingBounds(in: right, from: background, tolerance: 0.25) {
                store.put("list.badge.\(name).ink_trailing_gap", scheme, box.maxX - ink.maxX)
                store.put("list.badge.\(name).ink_height", scheme, ink.height)
                store.put("list.badge.\(name).ink_width", scheme, ink.width)
                store.colour("list.badge.\(name).ink", scheme, shot.raster.extreme(in: ink, from: background))
            } else {
                store.put("list.badge.\(name).ink_width", scheme, 0)
            }
            store.put("list.badge.\(name).row_width", scheme, row.width)
        }
        store.image("list_badges", shot, box, pad: 0)
    }

    static func separatorList(_ hide: Visibility, _ edges: VerticalEdge.Set, tint: Color?) -> some View {
        List {
            ForEach(names, id: \.self) { name in
                if name == "Bravo" {
                    Text(name).listRowSeparator(hide, edges: edges).listRowSeparatorTint(tint, edges: edges)
                } else {
                    Text(name)
                }
            }
        }
        .listStyle(.inset)
        .frame(width: 300, height: 150)
        .probe("list")
    }

    static func separators(_ store: Store, _ scheme: Scheme) {
        let cases: [(String, Visibility, VerticalEdge.Set, Color?)] = [
            ("default", .automatic, .all, nil),
            ("hidden_all", .hidden, .all, nil),
            ("hidden_top", .hidden, .top, nil),
            ("hidden_bottom", .hidden, .bottom, nil),
            ("tint_all", .automatic, .all, .red),
            ("tint_top", .automatic, .top, .red),
            ("tint_bottom", .automatic, .bottom, .red),
        ]
        for (name, hide, edges, tint) in cases {
            let shot = Harness.capture(separatorList(hide, edges, tint: tint), scheme: scheme, settle: 0.3)
            let box = shot.frame("list")
            let text = rows(shot, box)
            guard text.count >= 4 else { continue }
            let x = box.maxX - 40
            let found = lines(shot, column: x, from: text[0].minY, to: text[3].maxY)
            store.put("list.separator.\(name).count", scheme, Double(found.count), "")
            for (index, line) in found.enumerated() {
                let after = text.lastIndex { $0.maxY < line.0 } ?? -1
                store.put("list.separator.\(name).line\(index).after_row", scheme, Double(after), "")
                store.colour("list.separator.\(name).line\(index).colour", scheme, line.1)
            }
            if name == "default", let first = found.first {
                let y = first.0 + 0.25
                var left = box.minX
                let base = shot.raster.sampleFlat(CGPoint(x: x, y: text[0].minY))
                while left < x, shot.raster.sampleFlat(CGPoint(x: left, y: y)).distance(base) <= 0.02 { left += 0.5 }
                var right = box.maxX - 1
                while right > x, shot.raster.sampleFlat(CGPoint(x: right, y: y)).distance(base) <= 0.02 { right -= 0.5 }
                store.put("list.separator.default.leading", scheme, left - box.minX)
                store.put("list.separator.default.trailing_gap", scheme, box.maxX - right)
                store.put("list.separator.default.text_leading", scheme, text[0].minX - box.minX)
            }
            store.image("list_separator_\(name)", shot, box, pad: 0)
        }
    }

    static func backgrounds(_ store: Store, _ scheme: Scheme) {
        let view = List {
            Text("Alpha")
            Text("Bravo").listRowBackground(Color.red).probe("row")
            Text("Charlie")
        }
        .listStyle(.inset)
        .frame(width: 300, height: 120)
        .probe("list")
        let shot = Harness.capture(view, scheme: scheme, settle: 0.3)
        let box = shot.frame("list")
        let row = shot.frame("row")
        let fill = shot.raster.span(from: CGPoint(x: box.maxX - 20, y: row.midY), tolerance: 0.05)
        store.put("list.row_background.x", scheme, fill.minX - box.minX)
        store.put("list.row_background.width", scheme, fill.width)
        store.put("list.row_background.height", scheme, fill.height)
        store.put("list.row_background.row_height", scheme, row.height)
        store.colour("list.row_background.colour", scheme, shot.raster.sampleFlat(CGPoint(x: box.maxX - 20, y: row.midY)))
        store.image("list_row_background", shot, box, pad: 0)
    }

    static func prominence(_ store: Store, _ scheme: Scheme) {
        for (name, prominence) in [("standard", Prominence.standard), ("increased", Prominence.increased)] {
            let view = List {
                Section("Header") {
                    Text("Alpha")
                }
                .headerProminence(prominence)
            }
            .listStyle(.inset)
            .frame(width: 300, height: 100)
            .probe("list")
            let shot = Harness.capture(view, scheme: scheme, settle: 0.3)
            let box = shot.frame("list")
            let text = rows(shot, box)
            guard text.count >= 2 else { continue }
            store.put("list.prominence.\(name).header_ink_height", scheme, text[0].height)
            store.put("list.prominence.\(name).header_ink_width", scheme, text[0].width)
            store.put("list.prominence.\(name).header_leading", scheme, text[0].minX - box.minX)
            store.put("list.prominence.\(name).header_to_row_baseline", scheme, text[1].maxY - text[0].maxY)
            store.colour("list.prominence.\(name).header_colour", scheme, shot.raster.extreme(in: text[0], from: shot.raster.sampleFlat(CGPoint(x: text[0].maxX + 4, y: text[0].midY))))
            store.image("list_prominence_\(name)", shot, box, pad: 0)
        }
    }

    static func disclosure(_ store: Store, _ scheme: Scheme) {
        for (name, expanded) in [("collapsed", false), ("expanded", true)] {
            let view = DisclosureGroup(isExpanded: .constant(expanded)) {
                Text("Child").probe("child")
            } label: {
                Text("Label").probe("label")
            }
            .frame(width: 220, alignment: .leading)
            .probe("group")
            let shot = Harness.capture(view, scheme: scheme, settle: 0.3)
            let group = shot.frame("group")
            let label = shot.frame("label")
            store.put("disclosure.\(name).height", scheme, group.height)
            store.put("disclosure.\(name).label_x", scheme, label.minX - group.minX)
            store.put("disclosure.\(name).label_height", scheme, label.height)
            let background = shot.raster.background
            let left = CGRect(x: group.minX, y: group.minY, width: label.minX - group.minX - 1, height: label.height)
            if let chevron = shot.raster.differingBounds(in: left, from: background, tolerance: 0.2) {
                store.put("disclosure.\(name).chevron_x", scheme, chevron.minX - group.minX)
                store.put("disclosure.\(name).chevron_width", scheme, chevron.width)
                store.put("disclosure.\(name).chevron_height", scheme, chevron.height)
                store.colour("disclosure.\(name).chevron", scheme, shot.raster.extreme(in: chevron, from: background))
            }
            if expanded {
                let child = shot.frame("child")
                store.put("disclosure.expanded.child_x", scheme, child.minX - group.minX)
                store.put("disclosure.expanded.child_y", scheme, child.minY - label.maxY)
            }
            store.image("disclosure_\(name)", shot, group)
        }
    }

    struct Node: Identifiable {
        let id: String
        let children: [Node]?
    }

    static func outline(_ store: Store, _ scheme: Scheme) {
        let tree = [Node(id: "Root", children: [Node(id: "Leaf", children: nil)])]
        let view = List {
            OutlineGroup(tree, children: \.children) { node in
                Text(node.id).probe(node.id)
            }
        }
        .listStyle(.inset)
        .frame(width: 300, height: 120)
        .probe("list")
        let shot = Harness.capture(view, scheme: scheme, settle: 0.4)
        let box = shot.frame("list")
        store.put("outline.root_x", scheme, shot.frame("Root").minX - box.minX)
        store.put("outline.leaf_x", scheme, shot.frame("Leaf").minX - box.minX)
        store.put("outline.leaf_present", scheme, shot.frame("Leaf") == .zero ? 0 : 1, "")
        store.image("outline", shot, box, pad: 0)
    }

    static func insets(_ store: Store, _ scheme: Scheme) {
        let view = List {
            Text("Plain").probe("plain")
            Text("Inset").listRowInsets(EdgeInsets(top: 10, leading: 40, bottom: 10, trailing: 0)).probe("inset")
        }
        .listStyle(.inset)
        .frame(width: 300, height: 120)
        .probe("list")
        let shot = Harness.capture(view, scheme: scheme, settle: 0.3)
        let box = shot.frame("list")
        store.put("list.insets.plain_x", scheme, shot.frame("plain").minX - box.minX)
        store.put("list.insets.inset_x", scheme, shot.frame("inset").minX - box.minX)
        store.put("list.insets.plain_height", scheme, shot.frame("plain").height)
        store.put("list.insets.inset_height", scheme, shot.frame("inset").height)
        let text = rows(shot, box)
        if text.count >= 2 {
            store.put("list.insets.row_pitch", scheme, text[1].maxY - text[0].maxY)
        }
    }

    static func heights(_ store: Store, _ scheme: Scheme) {
        for (name, minimum) in [("default", CGFloat(0)), ("60", CGFloat(60))] {
            let base = List {
                Text("Alpha")
                Text("Bravo")
            }
            .listStyle(.inset)
            .frame(width: 300, height: 200)
            .probe("list")
            let view = minimum > 0 ? AnyView(base.environment(\.defaultMinListRowHeight, minimum)) : AnyView(base)
            let shot = Harness.capture(view, scheme: scheme, settle: 0.3)
            let text = rows(shot, shot.frame("list"))
            if text.count >= 2 {
                store.put("list.min_row_height.\(name).row_pitch", scheme, text[1].maxY - text[0].maxY)
            }
        }
    }

    static func alternating(_ store: Store, _ scheme: Scheme) {
        let view = List {
            ForEach(names, id: \.self) { Text($0).probe($0) }
        }
        .listStyle(.inset)
        .alternatingRowBackgrounds()
        .frame(width: 300, height: 150)
        .probe("list")
        let shot = Harness.capture(view, scheme: scheme, settle: 0.3)
        let box = shot.frame("list")
        for name in names {
            let row = shot.frame(name)
            store.colour("list.alternating.\(name.lowercased())", scheme, shot.raster.sampleFlat(CGPoint(x: box.maxX - 20, y: row.midY)))
        }
        store.image("list_alternating", shot, box, pad: 0)
    }
}
