import AppKit
import SwiftUI

enum TypeFeatures {
    static func run(_ store: Store) {
        let scheme = Scheme.light
        lineHeights(store, scheme)
        leading(store, scheme)
        smallCaps(store, scheme)
        patterns(store, scheme)
        runs(store, scheme)
        inline(store, scheme)
    }

    static func size<V: View>(_ view: V, _ scheme: Scheme) -> CGSize {
        Harness.capture(view.probe("t"), scheme: scheme).frame("t").size
    }

    static let lines = "One\nTwo\nThree"

    static func lineHeights(_ store: Store, _ scheme: Scheme) {
        let font = Font.system(size: 20)
        store.put("type.line_height.default", nil, size(Text(lines).font(font), scheme).height)
        let cases: [(String, AttributedString.LineHeight)] = [
            ("normal", .normal), ("variable", .variable), ("tight", .tight), ("loose", .loose),
            ("multiple_1_5", .multiple(factor: 1.5)), ("leading_4", .leading(increase: 4)), ("exact_30", .exact(points: 30)),
        ]
        for (name, value) in cases {
            store.put("type.line_height.\(name)", nil, size(Text(lines).font(font).lineHeight(value), scheme).height)
        }
        store.put("type.line_height.single_exact_30", nil, size(Text("One").font(font).lineHeight(.exact(points: 30)), scheme).height)
        store.put("type.line_height.single_default", nil, size(Text("One").font(font), scheme).height)
    }

    static func leading(_ store: Store, _ scheme: Scheme) {
        for (name, value) in [("standard", Font.Leading.standard), ("tight", .tight), ("loose", .loose)] {
            store.put("type.leading.\(name)", nil, size(Text(lines).font(.system(size: 20).leading(value)), scheme).height)
            store.put("type.leading.body_\(name)", nil, size(Text(lines).font(.body.leading(value)), scheme).height)
            store.put("type.leading.body_single_\(name)", nil, size(Text("One").font(.body.leading(value)), scheme).height)
            store.put("type.leading.title_\(name)", nil, size(Text(lines).font(.title.leading(value)), scheme).height)
        }
    }

    static func smallCaps(_ store: Store, _ scheme: Scheme) {
        let font = Font.system(size: 40)
        let plain = Harness.capture(Text("H").font(font).probe("t"), scheme: scheme)
        let small = Harness.capture(Text("h").font(font.lowercaseSmallCaps()).probe("t"), scheme: scheme)
        let upper = Harness.capture(Text("H").font(font.uppercaseSmallCaps()).probe("t"), scheme: scheme)
        let both = Harness.capture(Text("Hh").font(font.smallCaps()).probe("t"), scheme: scheme)
        for (name, shot) in [("cap", plain), ("lower_small", small), ("upper_small", upper), ("both", both)] {
            let box = shot.frame("t")
            if let ink = shot.raster.opaqueBounds(in: box, threshold: 0.3) {
                store.put("type.small_caps.\(name)_height", nil, ink.height)
                store.put("type.small_caps.\(name)_width", nil, ink.width)
            }
        }
        store.put("type.small_caps.word_plain", nil, size(Text("Small Caps").font(.system(size: 20)), scheme).width)
        store.put("type.small_caps.word_small", nil, size(Text("Small Caps").font(.system(size: 20).smallCaps()), scheme).width)
        store.put("type.small_caps.word_lower", nil, size(Text("Small Caps").font(.system(size: 20).lowercaseSmallCaps()), scheme).width)
    }

    static func patterns(_ store: Store, _ scheme: Scheme) {
        let font = Font.system(size: 20)
        let text = "MMMMMMMMMMMM"
        let plain = Harness.capture(Text(text).font(font).probe("t"), scheme: scheme)
        let box = plain.frame("t")
        for (name, pattern) in [("solid", Text.LineStyle.Pattern.solid), ("dot", .dot), ("dash", .dash), ("dash_dot", .dashDot), ("dash_dot_dot", .dashDotDot)] {
            let shot = Harness.capture(Text(text).font(font).underline(pattern: pattern, color: .red).probe("t"), scheme: scheme)
            let raster = shot.raster
            var row = -1
            for y in raster.px(box.minY)..<raster.px(box.maxY) + 4 where y < raster.height {
                var red = 0
                for x in raster.px(box.minX)..<raster.px(box.maxX) {
                    let c = raster.flat(x, y)
                    if c.r > 0.6 && c.g < 0.5 { red += 1 }
                }
                if red > 0 && row < 0 { row = y }
            }
            guard row >= 0 else { continue }
            var segments: [(Int, Int)] = []
            var start = -1
            for x in raster.px(box.minX)..<raster.px(box.maxX) + 2 {
                let c = raster.flat(x, row)
                let on = c.r > 0.6 && c.g < 0.6
                if on && start < 0 { start = x }
                if !on && start >= 0 { segments.append((start, x)); start = -1 }
            }
            let lengths = segments.map { Double($0.1 - $0.0) / Double(raster.scale) }
            let gaps = zip(segments, segments.dropFirst()).map { Double($1.0 - $0.1) / Double(raster.scale) }
            store.put("type.pattern.\(name).segments", nil, Double(segments.count), "count")
            store.put("type.pattern.\(name).lengths", nil, text: lengths.prefix(6).map { Store.format($0) }.joined(separator: " "), "pt")
            store.put("type.pattern.\(name).gaps", nil, text: gaps.prefix(6).map { Store.format($0) }.joined(separator: " "), "pt")
            store.image("type_pattern_\(name)", shot, box)
        }
    }

    static func runs(_ store: Store, _ scheme: Scheme) {
        var attributed = AttributedString("Big small")
        if let range = attributed.range(of: "Big") {
            attributed[range].font = .system(size: 34)
        }
        store.put("type.runs.mixed_size_height", nil, size(Text(attributed).font(.system(size: 17)), scheme).height)
        store.put("type.runs.small_height", nil, size(Text("Big small").font(.system(size: 17)), scheme).height)
        store.put("type.runs.big_height", nil, size(Text("Big small").font(.system(size: 34)), scheme).height)
        var kerned = AttributedString("Kerned")
        kerned.kern = 4
        store.put("type.runs.kern_width", nil, size(Text(kerned).font(.system(size: 17)), scheme).width)
        store.put("type.runs.plain_width", nil, size(Text("Kerned").font(.system(size: 17)), scheme).width)
        var raised = AttributedString("Raised")
        raised.baselineOffset = 6
        store.put("type.runs.raised_height", nil, size(Text(raised).font(.system(size: 17)), scheme).height)
        let concatenated = size(Text("Title").font(.title) + Text(" body"), scheme)
        store.put("type.runs.concat_height", nil, concatenated.height)
        store.put("type.runs.title_height", nil, size(Text("Title").font(.title), scheme).height)
    }

    static func inline(_ store: Store, _ scheme: Scheme) {
        let font = Font.system(size: 20)
        store.put("type.inline.symbol_text_width", nil, size(Text("A \(Image(systemName: "star.fill")) B").font(font), scheme).width)
        store.put("type.inline.plain_width", nil, size(Text("A  B").font(font), scheme).width)
        store.put("type.inline.symbol_height", nil, size(Text("A \(Image(systemName: "star.fill")) B").font(font), scheme).height)
        store.put("type.inline.text_height", nil, size(Text("A B").font(font), scheme).height)
    }
}
