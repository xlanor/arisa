import AppKit
import SwiftUI

enum Canvases {
    @MainActor static func sample(_ store: Store, _ key: String, _ view: some View, width: CGFloat = 40, height: CGFloat = 40, points: [(String, Int, Int)]) {
        guard let shot = Effects.pixels(view, width: width, height: height) else { return }
        for (name, x, y) in points {
            store.put("canvas.\(key).\(name)", nil, text: shot.straight(x, y).hex, "colour")
        }
    }

    @MainActor static func run(_ store: Store) {
        let modes: [(String, GraphicsContext.BlendMode)] = [
            ("clear", .clear), ("copy", .copy), ("source_in", .sourceIn), ("source_out", .sourceOut),
            ("destination_in", .destinationIn), ("destination_out", .destinationOut),
            ("destination_atop", .destinationAtop), ("xor", .xor), ("source_atop", .sourceAtop),
            ("destination_over", .destinationOver),
        ]
        for (name, mode) in modes {
            let view = Canvas { context, _ in
                context.fill(Path(CGRect(x: 0, y: 0, width: 20, height: 40)), with: .color(Color(red: 0, green: 0, blue: 1)))
                context.blendMode = mode
                context.fill(Path(CGRect(x: 10, y: 10, width: 20, height: 20)), with: .color(Color(red: 1, green: 0, blue: 0, opacity: 0.5)))
            }
            sample(store, "blend.\(name)", view, points: [("destination", 5, 5), ("overlap", 15, 20), ("source", 25, 20), ("outside", 35, 35)])
        }

        sample(store, "circle", ZStack { Color.white; Circle().fill(Color.black) }, width: 100, height: 40,
               points: [("centre", 50, 20), ("inside_left", 31, 20), ("outside_left", 29, 20), ("outside_right", 71, 20), ("far_left", 10, 20)])

        sample(store, "mix.perceptual", Rectangle().fill(Color(red: 1, green: 0, blue: 0).mix(with: Color(red: 0, green: 0, blue: 1), by: 0.5)), points: [("centre", 20, 20)])
        sample(store, "mix.device", Rectangle().fill(Color(red: 1, green: 0, blue: 0).mix(with: Color(red: 0, green: 0, blue: 1), by: 0.5, in: .device)), points: [("centre", 20, 20)])
        sample(store, "space.display_p3", Rectangle().fill(Color(.displayP3, red: 0.5, green: 0.5, blue: 0.2)), points: [("centre", 20, 20)])
        sample(store, "space.srgb_linear", Rectangle().fill(Color(.sRGBLinear, red: 0.5, green: 0.2, blue: 0.0)), points: [("centre", 20, 20)])
        sample(store, "space.white_linear", Rectangle().fill(Color(.sRGBLinear, white: 0.5)), points: [("centre", 20, 20)])
        sample(store, "exposure.down", Rectangle().fill(Color(red: 0.5, green: 0.5, blue: 0.5).exposureAdjust(-1)), points: [("centre", 20, 20)])
        sample(store, "exposure.up", Rectangle().fill(Color(red: 0.25, green: 0.5, blue: 0.1).exposureAdjust(1)), points: [("centre", 20, 20)])
        let hdr = Color(red: 0.5, green: 0.5, blue: 0.5).exposureAdjust(1).headroom(2).resolveHDR(in: EnvironmentValues())
        store.put("canvas.hdr.red", nil, Double(hdr.red), "level")
        store.put("canvas.hdr.linear_red", nil, Double(hdr.linearRed), "level")
        store.put("canvas.hdr.headroom", nil, Double(hdr.headroom ?? 0), "level")

        for (name, colour) in [("black", Color(red: 0, green: 0, blue: 0)), ("red", Color(red: 1, green: 0, blue: 0)), ("g50", Color(red: 0.5, green: 0.5, blue: 0.5))] {
            sample(store, "gradient.\(name)", Rectangle().fill(colour.gradient), width: 4, height: 200,
                   points: [("y_0", 2, 0), ("y_70", 2, 70), ("y_140", 2, 140), ("y_199", 2, 199)])
        }
        for scheme in Scheme.allCases {
            let shot = { (background: Color) in ZStack { background; Rectangle().fill(.bar) }.environment(\.colorScheme, scheme.colorScheme) }
            if let white = Effects.pixels(shot(.white), width: 40, height: 40) {
                store.put("canvas.bar.on_white", scheme, text: white.straight(20, 20).hex, "colour")
            }
            if let black = Effects.pixels(shot(.black), width: 40, height: 40) {
                store.put("canvas.bar.on_black", scheme, text: black.straight(20, 20).hex, "colour")
            }
        }

        let filters: [(String, GraphicsContext.Filter)] = [
            ("alpha_multiply", .alphaMultiply(Color(red: 0, green: 1, blue: 0, opacity: 0.5))),
            ("alpha_threshold", .alphaThreshold(min: 0.4, color: Color(red: 0, green: 1, blue: 0))),
            ("shadow", .shadow(color: Color(red: 0, green: 0, blue: 0), radius: 0, x: 10, y: 0)),
            ("shadow_only", .shadow(color: Color(red: 0, green: 0, blue: 0), radius: 0, x: 10, y: 0, options: .shadowOnly)),
            ("inner_shadow", .shadow(color: Color(red: 0, green: 0, blue: 0), radius: 0, x: 10, y: 0, options: .invertsAlpha)),
        ]
        for (name, filter) in filters {
            let view = Canvas { context, _ in
                context.addFilter(filter)
                context.fill(Path(CGRect(x: 0, y: 10, width: 20, height: 20)), with: .color(Color(red: 1, green: 0, blue: 0, opacity: 0.6)))
            }
            sample(store, "filter.\(name)", view, points: [("inside", 5, 20), ("edge", 15, 20), ("beyond", 25, 20)])
        }

        let inverse = Canvas { context, _ in
            context.clip(to: Path(CGRect(x: 10, y: 10, width: 20, height: 20)), options: .inverse)
            context.fill(Path(CGRect(x: 0, y: 0, width: 40, height: 40)), with: .color(Color(red: 1, green: 0, blue: 0)))
        }
        sample(store, "clip.inverse", inverse, points: [("inside", 20, 20), ("outside", 5, 5)])
        let even = Canvas { context, _ in
            var ring = Path(CGRect(x: 0, y: 0, width: 40, height: 40))
            ring.addRect(CGRect(x: 10, y: 10, width: 20, height: 20))
            context.clip(to: ring, style: FillStyle(eoFill: true))
            context.fill(Path(CGRect(x: 0, y: 0, width: 40, height: 40)), with: .color(Color(red: 1, green: 0, blue: 0)))
        }
        sample(store, "clip.even_odd", even, points: [("hole", 20, 20), ("ring", 5, 5)])
        let layer = Canvas { context, _ in
            context.clipToLayer(opacity: 0.5) { inner in
                inner.fill(Path(CGRect(x: 0, y: 0, width: 20, height: 40)), with: .color(.black))
            }
            context.fill(Path(CGRect(x: 0, y: 0, width: 40, height: 40)), with: .color(Color(red: 1, green: 0, blue: 0)))
        }
        sample(store, "clip.layer", layer, points: [("inside", 5, 20), ("outside", 30, 20)])

        sample(store, "fill_then_stroke", Circle().fill(Color(red: 1, green: 0, blue: 0)).stroke(Color(red: 0, green: 0, blue: 1), lineWidth: 8), points: [("centre", 20, 20), ("rim", 20, 2)])
        let union = Rectangle().size(width: 20, height: 40).union(Circle().size(width: 40, height: 40))
        sample(store, "boolean.union", union.fill(Color.black), points: [("corner", 2, 2), ("circle", 30, 20), ("outside", 38, 2)])
        let subtracted = Rectangle().subtracting(Circle())
        sample(store, "boolean.subtracting", subtracted.fill(Color.black), points: [("corner", 2, 2), ("centre", 20, 20)])
    }
}
