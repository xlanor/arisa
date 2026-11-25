import AppKit
import SwiftUI

enum Effects {
    struct Pixels {
        let width: Int
        let height: Int
        let bytes: [UInt8]

        func straight(_ x: Int, _ y: Int) -> RGBA {
            let i = (min(max(y, 0), height - 1) * width + min(max(x, 0), width - 1)) * 4
            let a = Double(bytes[i + 3]) / 255
            if a <= 0 { return RGBA(r: 0, g: 0, b: 0, a: 0) }
            return RGBA(r: min(1, Double(bytes[i]) / 255 / a), g: min(1, Double(bytes[i + 1]) / 255 / a), b: min(1, Double(bytes[i + 2]) / 255 / a), a: a)
        }
    }

    @MainActor static func pixels(_ view: some View, width: CGFloat, height: CGFloat) -> Pixels? {
        let renderer = ImageRenderer(content: view.frame(width: width, height: height))
        renderer.scale = 1
        guard let image = renderer.cgImage else { return nil }
        let w = image.width, h = image.height
        var bytes = [UInt8](repeating: 0, count: w * h * 4)
        let space = CGColorSpace(name: CGColorSpace.sRGB)!
        let context = CGContext(data: &bytes, width: w, height: h, bitsPerComponent: 8, bytesPerRow: w * 4, space: space, bitmapInfo: CGImageAlphaInfo.premultipliedLast.rawValue)!
        context.draw(image, in: CGRect(x: 0, y: 0, width: w, height: h))
        return Pixels(width: w, height: h, bytes: bytes)
    }

    static let source = Color(red: 0.8, green: 0.4, blue: 0.2)

    @MainActor static func colour(_ store: Store, _ key: String, _ view: some View) {
        guard let shot = pixels(view, width: 40, height: 40) else { return }
        store.put("effects.\(key)", nil, text: shot.straight(20, 20).hex, "colour")
    }

    @MainActor static func run(_ store: Store) {
        let swatch = Rectangle().fill(source)
        colour(store, "source", swatch)
        colour(store, "brightness_0.2", swatch.brightness(0.2))
        colour(store, "contrast_1.5", swatch.contrast(1.5))
        colour(store, "saturation_0.5", swatch.saturation(0.5))
        colour(store, "grayscale_1", swatch.grayscale(1))
        colour(store, "grayscale_red", Rectangle().fill(Color(red: 1, green: 0, blue: 0)).grayscale(1))
        colour(store, "hue_rotation_90", swatch.hueRotation(.degrees(90)))
        colour(store, "color_invert", swatch.colorInvert())
        colour(store, "color_multiply", swatch.colorMultiply(Color(red: 0.5, green: 1, blue: 0.5)))
        colour(store, "luminance_to_alpha", swatch.luminanceToAlpha())
        let backdrop = Color(red: 0.2, green: 0.6, blue: 0.9)
        for (name, mode) in [("multiply", BlendMode.multiply), ("screen", .screen), ("overlay", .overlay),
                             ("darken", .darken), ("lighten", .lighten), ("difference", .difference),
                             ("exclusion", .exclusion), ("color_dodge", .colorDodge), ("color_burn", .colorBurn),
                             ("soft_light", .softLight), ("hard_light", .hardLight), ("hue", .hue),
                             ("saturation", .saturation), ("color", .color), ("luminosity", .luminosity),
                             ("plus_lighter", .plusLighter), ("plus_darker", .plusDarker)] {
            colour(store, "blend_\(name)", ZStack { backdrop; swatch.blendMode(mode) })
        }
        if let blurred = pixels(ZStack { Color.white; Rectangle().fill(Color.black).frame(width: 60, height: 60).blur(radius: 10) }, width: 120, height: 120) {
            for distance in [-20, -10, -5, 0, 5, 10, 20] {
                store.put("effects.blur_10.edge_\(distance)", nil, blurred.straight(30 + distance, 60).r, "level")
            }
        }
        if let opaque = pixels(ZStack { Color.white; Rectangle().fill(Color.black).frame(width: 60, height: 60).blur(radius: 10, opaque: true) }, width: 120, height: 120) {
            store.put("effects.blur_10_opaque.edge_-5", nil, opaque.straight(25, 60).r, "level")
            store.put("effects.blur_10_opaque.edge_5", nil, opaque.straight(35, 60).r, "level")
        }
        if let flipped = pixels(ZStack { Color.white; Rectangle().fill(Color.black).frame(width: 80, height: 60).rotation3DEffect(.degrees(45), axis: (x: 0, y: 1, z: 0)) }, width: 160, height: 120) {
            var minX = flipped.width, maxX = -1
            for x in 0..<flipped.width where flipped.straight(x, 60).r < 0.5 {
                minX = min(minX, x); maxX = max(maxX, x)
            }
            var topLeft = -1, topRight = -1
            for y in 0..<flipped.height where flipped.straight(minX + 1, y).r < 0.5 { topLeft = y; break }
            for y in 0..<flipped.height where flipped.straight(maxX - 1, y).r < 0.5 { topRight = y; break }
            store.put("effects.rotation3d_45.min_x", nil, Double(minX))
            store.put("effects.rotation3d_45.max_x", nil, Double(maxX + 1))
            store.put("effects.rotation3d_45.top_at_min_x", nil, Double(topLeft))
            store.put("effects.rotation3d_45.top_at_max_x", nil, Double(topRight))
        }
        if let masked = pixels(ZStack { Color.white; Rectangle().fill(Color.black).mask(LinearGradient(colors: [.black, .clear], startPoint: .leading, endPoint: .trailing)) }, width: 100, height: 20) {
            store.put("effects.mask_gradient.quarter", nil, masked.straight(25, 10).r, "level")
            store.put("effects.mask_gradient.half", nil, masked.straight(50, 10).r, "level")
        }
        if let circle = pixels(source.clipShape(Circle()), width: 40, height: 40) {
            store.put("effects.clip_circle_40.alpha_1_1", nil, circle.straight(1, 1).a, "level")
            store.put("effects.clip_circle_40.alpha_5_5", nil, circle.straight(5, 5).a, "level")
            store.put("effects.clip_circle_40.alpha_7_7", nil, circle.straight(7, 7).a, "level")
            store.put("effects.clip_circle_40.alpha_0_20", nil, circle.straight(0, 20).a, "level")
        }
        if let rounded = pixels(source.clipShape(RoundedRectangle(cornerRadius: 14, style: .continuous)), width: 60, height: 40) {
            for step in 0...5 {
                store.put("effects.clip_rounded_14_continuous.alpha_\(step)_\(step)", nil, rounded.straight(step, step).a, "level")
            }
            store.put("effects.clip_rounded_14_continuous.alpha_0_20", nil, rounded.straight(0, 20).a, "level")
        }
        for opaque in [true, false] {
            if let group = pixels(ZStack { Color.white; Circle().fill(source).frame(width: 20, height: 20).drawingGroup(opaque: opaque) }, width: 40, height: 40) {
                store.put("effects.drawing_group_opaque_\(opaque).box_corner", nil, text: group.straight(11, 11).hex, "colour")
            }
        }
        if let clipped = pixels(ZStack { Color.white; source.frame(width: 60, height: 60).frame(width: 20, height: 20).clipped() }, width: 40, height: 40) {
            store.put("effects.clipped.inside", nil, text: clipped.straight(11, 11).hex, "colour")
            store.put("effects.clipped.outside", nil, text: clipped.straight(9, 9).hex, "colour")
        }
    }
}
