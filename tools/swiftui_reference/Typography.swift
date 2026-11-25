import AppKit
import SwiftUI

enum Typography {
    static let styles: [(String, Font, NSFont.TextStyle)] = [
        ("large_title", .largeTitle, .largeTitle),
        ("title", .title, .title1),
        ("title2", .title2, .title2),
        ("title3", .title3, .title3),
        ("headline", .headline, .headline),
        ("subheadline", .subheadline, .subheadline),
        ("body", .body, .body),
        ("callout", .callout, .callout),
        ("footnote", .footnote, .footnote),
        ("caption", .caption, .caption1),
        ("caption2", .caption2, .caption2),
    ]

    static let sample = "The quick brown fox"

    static func weight(of font: NSFont) -> Double {
        let traits = font.fontDescriptor.object(forKey: .traits) as? [NSFontDescriptor.TraitKey: Any]
        let w = (traits?[.weight] as? Double) ?? 0
        let table: [(Double, Double)] = [(-0.8, 100), (-0.6, 200), (-0.4, 300), (0, 400), (0.23, 500), (0.3, 600), (0.4, 700), (0.56, 800), (0.62, 900)]
        return table.min { abs($0.0 - w) < abs($1.0 - w) }!.1
    }

    static func run(_ store: Store) {
        for (name, font, style) in styles {
            let ns = NSFont.preferredFont(forTextStyle: style)
            let key = "text.\(name)"
            store.put("\(key).size", nil, ns.pointSize)
            store.put("\(key).weight", nil, weight(of: ns), "")
            store.put("\(key).ascender", nil, ns.ascender)
            store.put("\(key).descender", nil, -ns.descender)
            store.put("\(key).leading", nil, ns.leading)
            store.put("\(key).cap_height", nil, ns.capHeight)
            store.put("\(key).x_height", nil, ns.xHeight)
            let manager = NSLayoutManager()
            store.put("\(key).default_line_height", nil, manager.defaultLineHeight(for: ns))
            let width = (sample as NSString).size(withAttributes: [.font: ns]).width
            store.put("\(key).sample_width", nil, width)
            store.put("\(key).em_ratio", nil, width / ns.pointSize, "ratio")
            for scheme in Scheme.allCases {
                let view = VStack(alignment: .leading, spacing: 24) {
                    Text(sample).font(font).probe("one")
                    Text("\(sample)\n\(sample)").font(font).probe("two")
                }
                let shot = Harness.capture(view, scheme: scheme)
                let one = shot.frame("one"), two = shot.frame("two")
                store.put("\(key).line_height", scheme, one.height)
                store.put("\(key).line_advance", scheme, two.height - one.height)
                store.put("\(key).swiftui_width", scheme, one.width)
                store.colour("\(key).colour", scheme, shot.raster.extreme(in: one, from: shot.raster.background))
                store.image("text_\(name)", shot, one, pad: 2)
            }
        }
        for scheme in Scheme.allCases {
            let view = VStack(alignment: .leading, spacing: 0) {
                Text("Primary").probe("primary")
                Text("Secondary").foregroundStyle(.secondary).probe("secondary")
                Text("Tertiary").foregroundStyle(.tertiary).probe("tertiary")
                Text("Quaternary").foregroundStyle(.quaternary).probe("quaternary")
            }
            let shot = Harness.capture(view, scheme: scheme)
            for level in ["primary", "secondary", "tertiary", "quaternary"] {
                store.colour("text.hierarchy.\(level)", scheme, shot.raster.extreme(in: shot.frame(level), from: shot.raster.background))
            }
        }
    }
}
