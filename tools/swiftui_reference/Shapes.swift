import AppKit
import SwiftUI

enum Shapes {
    @MainActor static func colour(_ store: Store, _ key: String, _ scheme: Scheme?, _ view: some View, x: Int = 20, y: Int = 20, width: CGFloat = 40, height: CGFloat = 40) {
        let schemed = view.environment(\.colorScheme, (scheme ?? .light).colorScheme)
        guard let shot = Effects.pixels(schemed, width: width, height: height) else { return }
        store.put("shapes.\(key)", scheme, text: shot.straight(x, y).hex, "colour")
    }

    @MainActor static func level(_ store: Store, _ key: String, _ shot: Effects.Pixels, _ x: Int, _ y: Int) {
        store.put("shapes.\(key)", nil, shot.straight(x, y).r, "level")
    }

    @MainActor static func diagonal(_ shot: Effects.Pixels, fromX: Int, fromY: Int, stepX: Int, stepY: Int) -> Double {
        var steps = 0
        while steps < 100 {
            let x = fromX + steps * stepX
            let y = fromY + steps * stepY
            if shot.straight(x, y).r < 0.5 { break }
            steps += 1
        }
        return Double(steps)
    }

    @MainActor static func corners(_ store: Store, _ key: String, _ view: some View) {
        guard let shot = Effects.pixels(ZStack { Color.white; view.foregroundStyle(Color.black).frame(width: 100, height: 80) }, width: 120, height: 100) else { return }
        store.put("shapes.\(key).top_leading", nil, diagonal(shot, fromX: 10, fromY: 10, stepX: 1, stepY: 1))
        store.put("shapes.\(key).top_trailing", nil, diagonal(shot, fromX: 109, fromY: 10, stepX: -1, stepY: 1))
        store.put("shapes.\(key).bottom_leading", nil, diagonal(shot, fromX: 10, fromY: 89, stepX: 1, stepY: -1))
        store.put("shapes.\(key).bottom_trailing", nil, diagonal(shot, fromX: 109, fromY: 89, stepX: -1, stepY: -1))
    }

    @MainActor static func run(_ store: Store) {
        let tiers: [(String, HierarchicalShapeStyle)] = [("primary", .primary), ("secondary", .secondary), ("tertiary", .tertiary), ("quaternary", .quaternary), ("quinary", .quinary)]
        for scheme in Scheme.allCases {
            for (name, tier) in tiers {
                colour(store, "hierarchical.default.\(name)", scheme, Rectangle().fill(tier))
                colour(store, "hierarchical.red.\(name)", scheme, Rectangle().fill(tier).foregroundStyle(Color(red: 1, green: 0, blue: 0)))
            }
            colour(store, "background_style.default", scheme, Rectangle().fill(.background))
        }
        colour(store, "hierarchical.explicit.secondary", nil, Rectangle().fill(.secondary).foregroundStyle(Color.red, Color(red: 0, green: 0, blue: 1), Color(red: 0, green: 1, blue: 0)))
        colour(store, "hierarchical.explicit.tertiary", nil, Rectangle().fill(.tertiary).foregroundStyle(Color.red, Color(red: 0, green: 0, blue: 1), Color(red: 0, green: 1, blue: 0)))
        colour(store, "hierarchical.explicit.quaternary", nil, Rectangle().fill(.quaternary).foregroundStyle(Color.red, Color(red: 0, green: 0, blue: 1), Color(red: 0, green: 1, blue: 0)))
        colour(store, "hierarchical.two.tertiary", nil, Rectangle().fill(.tertiary).foregroundStyle(Color(red: 1, green: 0, blue: 0), Color(red: 0, green: 0, blue: 1)))
        colour(store, "background_style.red", nil, Rectangle().fill(.background).backgroundStyle(Color(red: 1, green: 0, blue: 0)))
        colour(store, "background_style.modifier", nil, Color.clear.background().backgroundStyle(Color(red: 0, green: 0, blue: 1)))
        colour(store, "overlay_in_shape.centre", nil, Color.white.overlay(Color(red: 1, green: 0, blue: 0), in: Circle()))
        colour(store, "overlay_in_shape.corner", nil, Color.white.overlay(Color(red: 1, green: 0, blue: 0), in: Circle()), x: 2, y: 2)
        if let shot = Effects.pixels(Rectangle().foregroundStyle(LinearGradient(colors: [Color.black, Color.white], startPoint: .leading, endPoint: .trailing)), width: 40, height: 40) {
            level(store, "foreground_gradient_fill.x_5", shot, 5, 20)
            level(store, "foreground_gradient_fill.x_30", shot, 30, 20)
        }
        if let shot = Effects.pixels(Rectangle().fill(LinearGradient(colors: [Color.black, Color.white], startPoint: .leading, endPoint: .trailing)), width: 100, height: 10) {
            for x in stride(from: 5, to: 100, by: 10) {
                level(store, "linear_ramp.x_\(x)", shot, x, 5)
            }
        }

        let elliptical = EllipticalGradient(colors: [Color.black, Color.white])
        if let shot = Effects.pixels(Rectangle().fill(elliptical), width: 160, height: 80) {
            for (name, x, y) in [("centre", 80, 40), ("quarter_x", 120, 40), ("edge_x", 159, 40), ("quarter_y", 80, 60), ("edge_y", 80, 79), ("corner", 159, 79)] {
                level(store, "elliptical.\(name)", shot, x, y)
            }
        }
        let fractional = EllipticalGradient(colors: [Color.black, Color.white], center: .center, startRadiusFraction: 0.1, endRadiusFraction: 0.3)
        if let shot = Effects.pixels(Rectangle().fill(fractional), width: 160, height: 80) {
            for (name, x, y) in [("x_16", 96, 40), ("x_32", 112, 40), ("x_48", 128, 40), ("y_16", 80, 56)] {
                level(store, "elliptical_fraction.\(name)", shot, x, y)
            }
        }

        if let shot = Effects.pixels(ZStack { Color.white; Ellipse().fill(Color.black).frame(width: 120, height: 60) }, width: 140, height: 80) {
            var minX = shot.width, maxX = -1, minY = shot.height, maxY = -1
            for x in 0..<shot.width where shot.straight(x, 40).r < 0.5 { minX = min(minX, x); maxX = max(maxX, x) }
            for y in 0..<shot.height where shot.straight(70, y).r < 0.5 { minY = min(minY, y); maxY = max(maxY, y) }
            store.put("shapes.ellipse.min_x", nil, Double(minX))
            store.put("shapes.ellipse.max_x", nil, Double(maxX + 1))
            store.put("shapes.ellipse.min_y", nil, Double(minY))
            store.put("shapes.ellipse.max_y", nil, Double(maxY + 1))
            level(store, "ellipse.at_40_25", shot, 40, 25)
            level(store, "ellipse.at_30_30", shot, 30, 30)
        }

        corners(store, "uneven_default", UnevenRoundedRectangle(topLeadingRadius: 0, bottomLeadingRadius: 10, bottomTrailingRadius: 20, topTrailingRadius: 30))
        corners(store, "uneven_circular", UnevenRoundedRectangle(topLeadingRadius: 0, bottomLeadingRadius: 10, bottomTrailingRadius: 20, topTrailingRadius: 30, style: .circular))
        corners(store, "rounded_default", RoundedRectangle(cornerRadius: 30))
        corners(store, "rounded_circular", RoundedRectangle(cornerRadius: 30, style: .circular))
        corners(store, "uneven_rtl", UnevenRoundedRectangle(topLeadingRadius: 0, bottomLeadingRadius: 10, bottomTrailingRadius: 20, topTrailingRadius: 30, style: .circular).environment(\.layoutDirection, .rightToLeft))

        let container = RoundedRectangle(cornerRadius: 30, style: .circular)
        corners(store, "container_relative", ContainerRelativeShape().padding(10).containerShape(container))
        corners(store, "container_relative_none", ContainerRelativeShape().padding(10))
        corners(store, "container_relative_offset", ContainerRelativeShape().padding(.leading, 20).padding(.top, 5).padding(.trailing, 10).padding(.bottom, 10).containerShape(container))
        corners(store, "concentric", ConcentricRectangle().padding(10).containerShape(container))
        corners(store, "concentric_offset", ConcentricRectangle().padding(.leading, 20).padding(.top, 5).padding(.trailing, 10).padding(.bottom, 10).containerShape(container))
        corners(store, "concentric_minimum", ConcentricRectangle(corners: .concentric(minimum: 25)).padding(10).containerShape(container))
        corners(store, "concentric_fixed", ConcentricRectangle(corners: .fixed(5)).padding(10).containerShape(container))
        corners(store, "concentric_uniform", ConcentricRectangle(corners: .concentric, isUniform: true).padding(.leading, 20).padding(.top, 5).padding(.trailing, 10).padding(.bottom, 10).containerShape(container))
        corners(store, "concentric_none", ConcentricRectangle().padding(10))
        corners(store, "concentric_inner_padded", ConcentricRectangle().frame(width: 40, height: 30).frame(width: 100, height: 80, alignment: .topLeading).padding(0).containerShape(container))

        if let shot = Effects.pixels(ZStack { Color.white; Circle().strokeBorder(Color.black, lineWidth: 10).frame(width: 80, height: 80) }, width: 120, height: 120) {
            var minX = shot.width, maxX = -1, inner = -1
            for x in 0..<shot.width where shot.straight(x, 60).r < 0.5 { minX = min(minX, x); maxX = max(maxX, x) }
            for x in minX..<shot.width where shot.straight(x, 60).r >= 0.5 { inner = x; break }
            store.put("shapes.stroke_border.min_x", nil, Double(minX))
            store.put("shapes.stroke_border.max_x", nil, Double(maxX + 1))
            store.put("shapes.stroke_border.inner_x", nil, Double(inner))
        }
        if let shot = Effects.pixels(ZStack { Color.white; Circle().stroke(Color.black, lineWidth: 10).frame(width: 80, height: 80) }, width: 120, height: 120) {
            var minX = shot.width, maxX = -1
            for x in 0..<shot.width where shot.straight(x, 60).r < 0.5 { minX = min(minX, x); maxX = max(maxX, x) }
            store.put("shapes.stroke.min_x", nil, Double(minX))
            store.put("shapes.stroke.max_x", nil, Double(maxX + 1))
        }
        if let shot = Effects.pixels(ZStack { Color.white; Rectangle().inset(by: 10).fill(Color.black).frame(width: 80, height: 80) }, width: 120, height: 120) {
            var minX = shot.width
            for x in 0..<shot.width where shot.straight(x, 60).r < 0.5 { minX = min(minX, x) }
            store.put("shapes.inset.min_x", nil, Double(minX))
        }
        if let shot = Effects.pixels(ZStack { Color.white; Rectangle().scale(0.5).fill(Color.black).frame(width: 80, height: 80) }, width: 120, height: 120) {
            var minX = shot.width, maxX = -1
            for x in 0..<shot.width where shot.straight(x, 60).r < 0.5 { minX = min(minX, x); maxX = max(maxX, x) }
            store.put("shapes.scaled.min_x", nil, Double(minX))
            store.put("shapes.scaled.max_x", nil, Double(maxX + 1))
        }
        if let shot = Effects.pixels(ZStack { Color.white; Rectangle().offset(x: 10, y: 5).fill(Color.black).frame(width: 80, height: 80) }, width: 120, height: 120) {
            var minX = shot.width, minY = shot.height
            for x in 0..<shot.width where shot.straight(x, 60).r < 0.5 { minX = min(minX, x) }
            for y in 0..<shot.height where shot.straight(60, y).r < 0.5 { minY = min(minY, y) }
            store.put("shapes.offset.min_x", nil, Double(minX))
            store.put("shapes.offset.min_y", nil, Double(minY))
        }
        if let shot = Effects.pixels(ZStack { Color.white; Rectangle().rotation(.degrees(45)).fill(Color.black).frame(width: 60, height: 60) }, width: 120, height: 120) {
            var minX = shot.width, maxX = -1
            for x in 0..<shot.width where shot.straight(x, 60).r < 0.5 { minX = min(minX, x); maxX = max(maxX, x) }
            store.put("shapes.rotated.min_x", nil, Double(minX))
            store.put("shapes.rotated.max_x", nil, Double(maxX + 1))
        }

        if let shot = Effects.pixels(ZStack { Color(red: 0.5, green: 0.5, blue: 0.5); Rectangle().fill(Color.white.shadow(.inner(color: .black, radius: 10))).frame(width: 100, height: 100) }, width: 120, height: 120) {
            for distance in [0, 2, 5, 10, 15, 20, 30, 50] {
                level(store, "inner_shadow_10.edge_\(distance)", shot, 10 + distance, 60)
            }
            level(store, "inner_shadow_10.outside", shot, 5, 60)
        }
        if let shot = Effects.pixels(ZStack { Color(red: 0.5, green: 0.5, blue: 0.5); Rectangle().fill(Color.white.shadow(.inner(color: .black, radius: 4, x: 6, y: 6))).frame(width: 100, height: 100) }, width: 120, height: 120) {
            for distance in [0, 3, 6, 9, 12, 20] {
                level(store, "inner_shadow_offset.left_\(distance)", shot, 10 + distance, 60)
                level(store, "inner_shadow_offset.right_\(distance)", shot, 109 - distance, 60)
            }
        }
        if let shot = Effects.pixels(ZStack { Color.white; Rectangle().fill(Color.white.shadow(.drop(color: .black, radius: 10))).frame(width: 60, height: 60) }, width: 120, height: 120) {
            for distance in [0, 5, 10, 20] {
                level(store, "drop_shadow_style_10.edge_\(distance)", shot, 29 - distance, 60)
            }
        }
        if let shot = Effects.pixels(ZStack { Color.white; Text("MMMM").font(.system(size: 30, weight: .black)).foregroundStyle(LinearGradient(colors: [Color(red: 1, green: 0, blue: 0), Color(red: 0, green: 0, blue: 1)], startPoint: .leading, endPoint: .trailing)) }, width: 200, height: 60) {
            var minX = shot.width, maxX = -1
            for x in 0..<shot.width where shot.straight(x, 30).r + shot.straight(x, 30).b < 1.5 && (shot.straight(x, 30).r > 0.6 || shot.straight(x, 30).b > 0.6) { minX = min(minX, x); maxX = max(maxX, x) }
            store.put("shapes.gradient_text.min_x", nil, Double(minX))
            store.put("shapes.gradient_text.max_x", nil, Double(maxX + 1))
            store.put("shapes.gradient_text.left", nil, text: shot.straight(minX + 1, 30).hex, "colour")
            store.put("shapes.gradient_text.right", nil, text: shot.straight(maxX - 1, 30).hex, "colour")
        }
    }
}
