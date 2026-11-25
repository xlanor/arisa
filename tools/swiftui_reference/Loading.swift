import AppKit
import SwiftUI

enum Loading {
    static func run(_ store: Store) {
        for scheme in Scheme.allCases {
            redaction(store, scheme)
            unavailable(store, scheme)
        }
        let scheme = Scheme.light
        scales(store, scheme)
        rendering(store, scheme)
        symbols(store, scheme)
    }

    static func lines(_ shot: Shot, _ name: String) -> [CGRect] {
        let box = shot.frame(name).insetBy(dx: -2, dy: -2)
        return shot.raster.inkLines(in: box, contrast: 0.02, reference: shot.raster.background)
    }

    static func redaction(_ store: Store, _ scheme: Scheme) {
        let plain = Harness.capture(Text("Placeholder text").probe("t"), scheme: scheme)
        let redacted = Harness.capture(Text("Placeholder text").redacted(reason: .placeholder).probe("t"), scheme: scheme)
        let frame = redacted.frame("t")
        store.put("redacted.text.frame_width", scheme, frame.width)
        store.put("redacted.text.frame_height", scheme, frame.height)
        store.put("redacted.text.plain_width", scheme, plain.frame("t").width)
        if let bar = redacted.raster.differingBounds(in: frame.insetBy(dx: -2, dy: -2), from: redacted.raster.background, tolerance: 0.01) {
            store.put("redacted.text.bar_x", scheme, bar.minX - frame.minX)
            store.put("redacted.text.bar_y", scheme, bar.minY - frame.minY)
            store.put("redacted.text.bar_width", scheme, bar.width)
            store.put("redacted.text.bar_height", scheme, bar.height)
            let middle = CGPoint(x: bar.midX, y: bar.midY)
            let fill = redacted.raster.sampleFlat(middle)
            store.colour("redacted.text.bar_fill", scheme, fill)
            store.colour("redacted.text.bar_fill_alpha", scheme, redacted.raster.sample(middle))
            store.put("redacted.text.bar_radius", scheme, redacted.raster.regionRadius(of: bar, inside: fill, outside: redacted.raster.background))
            store.image("redacted_text", redacted, frame)
        }
        if let ink = plain.raster.differingBounds(in: plain.frame("t").insetBy(dx: -2, dy: -2), from: plain.raster.background, tolerance: 0.25) {
            store.put("redacted.text.plain_ink_y", scheme, ink.minY - plain.frame("t").minY)
            store.put("redacted.text.plain_ink_height", scheme, ink.height)
        }

        let wrapped = Harness.capture(Text("The quick brown fox jumps over the lazy dog").frame(width: 150, alignment: .leading).redacted(reason: .placeholder).probe("t"), scheme: scheme)
        let rows = lines(wrapped, "t")
        store.put("redacted.wrapped.frame_height", scheme, wrapped.frame("t").height)
        store.put("redacted.wrapped.bars", scheme, Double(rows.count), "")
        for (index, row) in rows.enumerated() {
            store.put("redacted.wrapped.bar\(index).y", scheme, row.minY - wrapped.frame("t").minY)
            store.put("redacted.wrapped.bar\(index).width", scheme, row.width)
            store.put("redacted.wrapped.bar\(index).height", scheme, row.height)
        }
        store.image("redacted_wrapped", wrapped, wrapped.frame("t"))

        let title = Harness.capture(Text("Title").font(.title).redacted(reason: .placeholder).probe("t"), scheme: scheme)
        if let bar = title.raster.differingBounds(in: title.frame("t").insetBy(dx: -2, dy: -2), from: title.raster.background, tolerance: 0.01) {
            store.put("redacted.title.frame_height", scheme, title.frame("t").height)
            store.put("redacted.title.bar_y", scheme, bar.minY - title.frame("t").minY)
            store.put("redacted.title.bar_height", scheme, bar.height)
            store.put("redacted.title.bar_radius", scheme, title.raster.regionRadius(of: bar, inside: title.raster.sampleFlat(CGPoint(x: bar.midX, y: bar.midY)), outside: title.raster.background))
        }

        let red = Harness.capture(Text("Placeholder").foregroundStyle(Color.red).redacted(reason: .placeholder).probe("t"), scheme: scheme)
        if let bar = red.raster.differingBounds(in: red.frame("t").insetBy(dx: -2, dy: -2), from: red.raster.background, tolerance: 0.01) {
            store.colour("redacted.red_text.bar_fill", scheme, red.raster.sampleFlat(CGPoint(x: bar.midX, y: bar.midY)))
            store.colour("redacted.red_text.bar_fill_alpha", scheme, red.raster.sample(CGPoint(x: bar.midX, y: bar.midY)))
        }

        let symbol = Harness.capture(Image(systemName: "star.fill").redacted(reason: .placeholder).probe("t"), scheme: scheme)
        store.put("redacted.symbol.frame_width", scheme, symbol.frame("t").width)
        store.put("redacted.symbol.frame_height", scheme, symbol.frame("t").height)
        if let bar = symbol.raster.differingBounds(in: symbol.frame("t").insetBy(dx: -2, dy: -2), from: symbol.raster.background, tolerance: 0.01) {
            store.put("redacted.symbol.ink_width", scheme, bar.width)
            store.put("redacted.symbol.ink_height", scheme, bar.height)
            store.colour("redacted.symbol.fill", scheme, symbol.raster.sampleFlat(CGPoint(x: bar.midX, y: bar.midY)))
            store.put("redacted.symbol.radius", scheme, symbol.raster.regionRadius(of: bar, inside: symbol.raster.sampleFlat(CGPoint(x: bar.midX, y: bar.midY)), outside: symbol.raster.background))
        }

        let bitmap = NSImage(size: NSSize(width: 40, height: 20), flipped: false) { rect in
            NSColor.systemBlue.setFill()
            rect.fill()
            return true
        }
        let picture = Harness.capture(Image(nsImage: bitmap).redacted(reason: .placeholder).probe("t"), scheme: scheme)
        store.put("redacted.image.frame_width", scheme, picture.frame("t").width)
        store.put("redacted.image.frame_height", scheme, picture.frame("t").height)
        let pictureMiddle = CGPoint(x: picture.frame("t").midX, y: picture.frame("t").midY)
        store.colour("redacted.image.fill", scheme, picture.raster.sampleFlat(pictureMiddle))
        if let bar = picture.raster.differingBounds(in: picture.frame("t").insetBy(dx: -2, dy: -2), from: picture.raster.background, tolerance: 0.01) {
            store.put("redacted.image.ink_width", scheme, bar.width)
            store.put("redacted.image.ink_height", scheme, bar.height)
            store.put("redacted.image.radius", scheme, picture.raster.regionRadius(of: bar, inside: picture.raster.sampleFlat(pictureMiddle), outside: picture.raster.background))
        }

        let shape = Harness.capture(Rectangle().fill(Color.red).frame(width: 40, height: 20).redacted(reason: .placeholder).probe("t"), scheme: scheme)
        store.colour("redacted.shape.fill", scheme, shape.raster.sampleFlat(CGPoint(x: shape.frame("t").midX, y: shape.frame("t").midY)))

        let kept = Harness.capture(VStack(alignment: .leading, spacing: 4) {
            Text("Hidden").probe("a")
            Text("Shown").unredacted().probe("b")
        }.redacted(reason: .placeholder), scheme: scheme)
        let keptInk = kept.raster.extreme(in: kept.frame("b").insetBy(dx: 0.5, dy: 0.5), from: kept.raster.background)
        store.colour("redacted.unredacted.ink", scheme, keptInk)
        store.colour("redacted.unredacted.hidden_ink", scheme, kept.raster.extreme(in: kept.frame("a").insetBy(dx: 0.5, dy: 0.5), from: kept.raster.background))

        let privacy = Harness.capture(VStack(alignment: .leading, spacing: 4) {
            Text("Public").probe("a")
            Text("Secret").privacySensitive().probe("b")
        }.redacted(reason: .privacy), scheme: scheme)
        store.put("redacted.privacy.public_lines", scheme, Double(lines(privacy, "a").count), "")
        store.colour("redacted.privacy.public_ink", scheme, privacy.raster.extreme(in: privacy.frame("a").insetBy(dx: 0.5, dy: 0.5), from: privacy.raster.background))
        store.colour("redacted.privacy.secret_ink", scheme, privacy.raster.extreme(in: privacy.frame("b").insetBy(dx: 0.5, dy: 0.5), from: privacy.raster.background))
        if let bar = privacy.raster.differingBounds(in: privacy.frame("b").insetBy(dx: -2, dy: -2), from: privacy.raster.background, tolerance: 0.01) {
            store.put("redacted.privacy.secret_bar_width", scheme, bar.width)
            store.put("redacted.privacy.secret_bar_height", scheme, bar.height)
            store.colour("redacted.privacy.secret_fill", scheme, privacy.raster.sampleFlat(CGPoint(x: bar.midX, y: bar.midY)))
        }
        store.image("redacted_privacy", privacy, privacy.frame("a").union(privacy.frame("b")))

        let unredactedSensitive = Harness.capture(Text("Secret").privacySensitive().probe("b"), scheme: scheme)
        store.colour("redacted.privacy_unset.secret_ink", scheme, unredactedSensitive.raster.extreme(in: unredactedSensitive.frame("b").insetBy(dx: 0.5, dy: 0.5), from: unredactedSensitive.raster.background))

        let placeholderSensitive = Harness.capture(Text("Secret").privacySensitive().redacted(reason: .placeholder).probe("b"), scheme: scheme)
        if let bar = placeholderSensitive.raster.differingBounds(in: placeholderSensitive.frame("b").insetBy(dx: -2, dy: -2), from: placeholderSensitive.raster.background, tolerance: 0.01) {
            store.colour("redacted.placeholder_sensitive.fill", scheme, placeholderSensitive.raster.sampleFlat(CGPoint(x: bar.midX, y: bar.midY)))
        }

        let button = Harness.capture(Button("Button") {}.redacted(reason: .placeholder).probe("t"), scheme: scheme)
        store.colour("redacted.button.ink", scheme, button.raster.extreme(in: button.frame("t").insetBy(dx: 0.5, dy: 0.5), from: button.raster.background))
    }

    static func unavailable(_ store: Store, _ scheme: Scheme) {
        let view = ContentUnavailableView {
            Label("No Games", systemImage: "gamecontroller").probe("label")
        } description: {
            Text("Description line").probe("description")
        } actions: {
            Button("Retry") {}.probe("action")
        }
        let shot = Harness.capture(view.probe("view").frame(width: 400, height: 300).probe("frame"), scheme: scheme)
        let outer = shot.frame("frame")
        let box = shot.frame("view")
        store.put("unavailable.view.x", scheme, box.minX - outer.minX)
        store.put("unavailable.view.y", scheme, box.minY - outer.minY)
        store.put("unavailable.view.width", scheme, box.width)
        store.put("unavailable.view.height", scheme, box.height)
        for name in ["label", "description", "action"] {
            let frame = shot.frame(name)
            store.put("unavailable.\(name).x", scheme, frame.minX - outer.minX)
            store.put("unavailable.\(name).y", scheme, frame.minY - outer.minY)
            store.put("unavailable.\(name).width", scheme, frame.width)
            store.put("unavailable.\(name).height", scheme, frame.height)
        }
        let rows = shot.raster.inkLines(in: outer, contrast: 0.08, reference: shot.raster.background)
        store.put("unavailable.ink_rows", scheme, Double(rows.count), "")
        for (index, row) in rows.enumerated() {
            store.put("unavailable.row\(index).x", scheme, row.minX - outer.minX)
            store.put("unavailable.row\(index).y", scheme, row.minY - outer.minY)
            store.put("unavailable.row\(index).width", scheme, row.width)
            store.put("unavailable.row\(index).height", scheme, row.height)
            store.colour("unavailable.row\(index).ink", scheme, shot.raster.extreme(in: row, from: shot.raster.background))
        }
        store.image("content_unavailable", shot, outer)

        let simple = Harness.capture(ContentUnavailableView("No Games", systemImage: "gamecontroller").probe("view").frame(width: 400, height: 300).probe("frame"), scheme: scheme)
        let simpleRows = simple.raster.inkLines(in: simple.frame("frame"), contrast: 0.08, reference: simple.raster.background)
        store.put("unavailable.simple.ink_rows", scheme, Double(simpleRows.count), "")
        for (index, row) in simpleRows.enumerated() {
            store.put("unavailable.simple.row\(index).x", scheme, row.minX - simple.frame("frame").minX)
            store.put("unavailable.simple.row\(index).y", scheme, row.minY - simple.frame("frame").minY)
            store.put("unavailable.simple.row\(index).width", scheme, row.width)
            store.put("unavailable.simple.row\(index).height", scheme, row.height)
        }

        for (name, view) in [("search", AnyView(ContentUnavailableView.search)), ("search_text", AnyView(ContentUnavailableView.search(text: "zelda")))] {
            let found = Harness.capture(view.frame(width: 400, height: 300).probe("frame"), scheme: scheme)
            let found_rows = found.raster.inkLines(in: found.frame("frame"), contrast: 0.08, reference: found.raster.background)
            store.put("unavailable.\(name).ink_rows", scheme, Double(found_rows.count), "")
            for (index, row) in found_rows.enumerated() {
                store.put("unavailable.\(name).row\(index).x", scheme, row.minX - found.frame("frame").minX)
                store.put("unavailable.\(name).row\(index).y", scheme, row.minY - found.frame("frame").minY)
                store.put("unavailable.\(name).row\(index).width", scheme, row.width)
                store.put("unavailable.\(name).row\(index).height", scheme, row.height)
            }
            if name == "search" {
                store.image("content_unavailable_search", found, found.frame("frame"))
            }
        }
        let title = NSFont.preferredFont(forTextStyle: .title2)
        store.put("unavailable.reference.title2_size", scheme, title.pointSize)
        store.put("unavailable.reference.no_games_title2_bold", scheme, ("No Games" as NSString).size(withAttributes: [.font: NSFont.systemFont(ofSize: title.pointSize, weight: .bold)]).width)
        store.put("unavailable.reference.no_games_title2", scheme, ("No Games" as NSString).size(withAttributes: [.font: title]).width)
        store.put("unavailable.reference.no_games_title2_semibold", scheme, ("No Games" as NSString).size(withAttributes: [.font: NSFont.systemFont(ofSize: title.pointSize, weight: .semibold)]).width)
        store.put("unavailable.reference.description_body", scheme, ("Description line" as NSString).size(withAttributes: [.font: NSFont.systemFont(ofSize: 13)]).width)
    }

    static func scales(_ store: Store, _ scheme: Scheme) {
        for (fontName, font) in [("body", Font.body), ("title", Font.title)] {
            let plain = Harness.capture(Text("Ag").font(font).probe("t"), scheme: scheme)
            store.put("image_scale.\(fontName).text_height", nil, plain.frame("t").height)
            for (scaleName, scale) in [("small", Image.Scale.small), ("medium", Image.Scale.medium), ("large", Image.Scale.large)] {
                let shot = Harness.capture(Image(systemName: "square.fill").font(font).imageScale(scale).probe("t"), scheme: scheme)
                let frame = shot.frame("t")
                store.put("image_scale.\(fontName).\(scaleName).width", nil, frame.width)
                store.put("image_scale.\(fontName).\(scaleName).height", nil, frame.height)
                if let ink = shot.raster.differingBounds(in: frame.insetBy(dx: -2, dy: -2), from: shot.raster.background, tolerance: 0.25) {
                    store.put("image_scale.\(fontName).\(scaleName).ink_width", nil, ink.width)
                    store.put("image_scale.\(fontName).\(scaleName).ink_height", nil, ink.height)
                }
            }
        }
    }

    static func rendering(_ store: Store, _ scheme: Scheme) {
        var bytes: [UInt8] = [0, 0, 0, 255, 255, 255, 255, 255, 255, 255, 255, 255, 0, 0, 0, 255]
        let provider = CGDataProvider(data: Data(bytes: &bytes, count: bytes.count) as CFData)!
        let cg = CGImage(width: 2, height: 2, bitsPerComponent: 8, bitsPerPixel: 32, bytesPerRow: 8, space: CGColorSpace(name: CGColorSpace.sRGB)!, bitmapInfo: CGBitmapInfo(rawValue: CGImageAlphaInfo.premultipliedLast.rawValue), provider: provider, decode: nil, shouldInterpolate: true, intent: .defaultIntent)!
        let checker = NSImage(cgImage: cg, size: NSSize(width: 2, height: 2))
        for (name, interpolation) in [("none", Image.Interpolation.none), ("low", .low), ("medium", .medium), ("high", .high)] {
            let shot = Harness.capture(Image(nsImage: checker).resizable().interpolation(interpolation).frame(width: 40, height: 40).probe("t"), scheme: scheme)
            let frame = shot.frame("t")
            let quarter = shot.raster.sampleFlat(CGPoint(x: frame.minX + 10, y: frame.minY + 10))
            let edge = shot.raster.sampleFlat(CGPoint(x: frame.minX + 16, y: frame.minY + 10))
            store.colour("interpolation.\(name).quarter", scheme, quarter)
            store.colour("interpolation.\(name).edge", scheme, edge)
            store.put("interpolation.\(name).edge_luma", nil, edge.luminance, "")
        }
        let plainShot = Harness.capture(Image(nsImage: checker).resizable().frame(width: 40, height: 40).probe("t"), scheme: scheme)
        store.put("interpolation.default.edge_luma", nil, plainShot.raster.sampleFlat(CGPoint(x: plainShot.frame("t").minX + 16, y: plainShot.frame("t").minY + 10)).luminance, "")

        let badge = NSImage(size: NSSize(width: 20, height: 20), flipped: false) { rect in
            NSColor.systemGreen.setFill()
            NSBezierPath(ovalIn: rect).fill()
            return true
        }
        for (name, mode) in [("template", Image.TemplateRenderingMode.template), ("original", .original)] {
            let shot = Harness.capture(Image(nsImage: badge).renderingMode(mode).foregroundStyle(Color.red).probe("t"), scheme: scheme)
            let frame = shot.frame("t")
            store.colour("rendering_mode.\(name).centre", scheme, shot.raster.sampleFlat(CGPoint(x: frame.midX, y: frame.midY)))
            store.colour("rendering_mode.\(name).corner", scheme, shot.raster.sampleFlat(CGPoint(x: frame.minX + 1, y: frame.minY + 1)))
        }
        let plainBadge = Harness.capture(Image(nsImage: badge).foregroundStyle(Color.red).probe("t"), scheme: scheme)
        store.colour("rendering_mode.default.centre", scheme, plainBadge.raster.sampleFlat(CGPoint(x: plainBadge.frame("t").midX, y: plainBadge.frame("t").midY)))
        let symbolOriginal = Harness.capture(Image(systemName: "square.fill").renderingMode(.original).foregroundStyle(Color.red).probe("t"), scheme: scheme)
        store.colour("rendering_mode.symbol_original.centre", scheme, symbolOriginal.raster.sampleFlat(CGPoint(x: symbolOriginal.frame("t").midX, y: symbolOriginal.frame("t").midY)))
    }

    static func symbols(_ store: Store, _ scheme: Scheme) {
        for (name, mode) in [("monochrome", SymbolRenderingMode.monochrome), ("hierarchical", .hierarchical)] {
            let single = Harness.capture(Image(systemName: "square.fill").font(.system(size: 40)).symbolRenderingMode(mode).foregroundStyle(Color.red).probe("t"), scheme: scheme)
            let frame = single.frame("t")
            store.colour("symbol_rendering.\(name).single_layer", scheme, single.raster.sampleFlat(CGPoint(x: frame.midX, y: frame.midY)))
            let layered = Harness.capture(Image(systemName: "plus.circle.fill").font(.system(size: 40)).symbolRenderingMode(mode).foregroundStyle(Color.red).probe("t"), scheme: scheme)
            let lframe = layered.frame("t")
            store.colour("symbol_rendering.\(name).two_layer_ring", scheme, layered.raster.sampleFlat(CGPoint(x: lframe.midX - lframe.width * 0.3, y: lframe.midY)))
            store.colour("symbol_rendering.\(name).two_layer_plus", scheme, layered.raster.sampleFlat(CGPoint(x: lframe.midX, y: lframe.midY)))
        }
        for (name, variant) in [("none", SymbolVariants.none), ("fill", .fill), ("circle", .circle), ("circle_fill", SymbolVariants.circle.fill), ("slash", .slash), ("square", .square)] {
            let shot = Harness.capture(Image(systemName: "heart").font(.system(size: 20)).symbolVariant(variant).probe("t"), scheme: scheme)
            let frame = shot.frame("t")
            store.put("symbol_variant.heart.\(name).width", nil, frame.width)
            store.put("symbol_variant.heart.\(name).height", nil, frame.height)
        }
        let missing = Harness.capture(Image(systemName: "gamecontroller").font(.system(size: 20)).symbolVariant(.slash).probe("t"), scheme: scheme)
        let base = Harness.capture(Image(systemName: "gamecontroller").font(.system(size: 20)).probe("t"), scheme: scheme)
        store.put("symbol_variant.missing_falls_back.same_width", nil, missing.frame("t").width == base.frame("t").width ? 1 : 0, "")
    }
}
