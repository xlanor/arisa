import AppKit
import SwiftUI

enum Layout {
    static func run(_ store: Store) {
        for scheme in Scheme.allCases {
            stacks(store, scheme)
            spacers(store, scheme)
            dividers(store, scheme)
            symbols(store, scheme)
            labels(store, scheme)
            shadows(store, scheme)
            shapes(store, scheme)
        }
    }

    static func stacks(_ store: Store, _ scheme: Scheme) {
        let view = VStack(alignment: .leading, spacing: 20) {
            VStack { Text("A").probe("vt1"); Text("B").probe("vt2") }
            HStack { Text("A").probe("ht1"); Text("B").probe("ht2") }
            VStack { Color.gray.frame(width: 20, height: 20).probe("vr1"); Color.gray.frame(width: 20, height: 20).probe("vr2") }
            HStack { Color.gray.frame(width: 20, height: 20).probe("hr1"); Color.gray.frame(width: 20, height: 20).probe("hr2") }
            VStack { Text("A").probe("vm1"); Button("B") {}.probe("vm2") }
            HStack { Text("A").probe("hm1"); Button("B") {}.probe("hm2") }
            HStack { Image(systemName: "star").probe("hi1"); Text("B").probe("hi2") }
            Text("Padded").probe("pad_inner").padding().probe("pad_outer")
            Text("Padded").probe("pads_inner").padding(.horizontal).probe("pads_outer")
        }
        let shot = Harness.capture(view, scheme: scheme)
        let f = shot.frame
        store.put("stack.vstack.text_spacing", scheme, f("vt2").minY - f("vt1").maxY)
        store.put("stack.hstack.text_spacing", scheme, f("ht2").minX - f("ht1").maxX)
        store.put("stack.vstack.shape_spacing", scheme, f("vr2").minY - f("vr1").maxY)
        store.put("stack.hstack.shape_spacing", scheme, f("hr2").minX - f("hr1").maxX)
        store.put("stack.vstack.text_button_spacing", scheme, f("vm2").minY - f("vm1").maxY)
        store.put("stack.hstack.text_button_spacing", scheme, f("hm2").minX - f("hm1").maxX)
        store.put("stack.hstack.image_text_spacing", scheme, f("hi2").minX - f("hi1").maxX)
        store.put("padding.default.leading", scheme, f("pad_inner").minX - f("pad_outer").minX)
        store.put("padding.default.top", scheme, f("pad_inner").minY - f("pad_outer").minY)
        store.put("padding.horizontal.leading", scheme, f("pads_inner").minX - f("pads_outer").minX)
        store.image("stack_vstack", shot, f("vt1").union(f("vt2")))
        store.image("stack_hstack", shot, f("ht1").union(f("ht2")))
        store.image("padding_default", shot, f("pad_outer"))
    }

    static func spacers(_ store: Store, _ scheme: Scheme) {
        let view = VStack(alignment: .leading, spacing: 10) {
            HStack(spacing: 0) { Text("A").probe("s1"); Spacer().probe("spacer"); Text("B").probe("s2") }.fixedSize().probe("row")
            VStack(spacing: 0) { Text("A").probe("v1"); Spacer().probe("vspacer"); Text("B").probe("v2") }.fixedSize().probe("column")
            HStack { Text("A").probe("d1"); Spacer().probe("dspacer"); Text("B").probe("d2") }.fixedSize()
        }
        let shot = Harness.capture(view, scheme: scheme)
        let f = shot.frame
        store.put("spacer.horizontal.min_length", scheme, f("s2").minX - f("s1").maxX)
        store.put("spacer.vertical.min_length", scheme, f("v2").minY - f("v1").maxY)
        store.put("spacer.in_default_hstack.gap", scheme, f("d2").minX - f("d1").maxX)
    }

    static func dividers(_ store: Store, _ scheme: Scheme) {
        let view = VStack(spacing: 12) {
            VStack(spacing: 0) { Color.clear.frame(width: 120, height: 1); Divider().probe("h"); Color.clear.frame(width: 120, height: 1) }
            HStack(spacing: 0) { Color.clear.frame(width: 1, height: 40); Divider().probe("v"); Color.clear.frame(width: 1, height: 40) }
            VStack { Text("A").probe("a"); Divider().probe("between"); Text("B").probe("b") }.frame(width: 120)
        }
        let shot = Harness.capture(view, scheme: scheme)
        let h = shot.frame("h"), v = shot.frame("v")
        store.put("divider.horizontal.thickness", scheme, h.height)
        store.put("divider.vertical.thickness", scheme, v.width)
        store.put("divider.in_vstack.gap_above", scheme, shot.frame("between").minY - shot.frame("a").maxY)
        store.put("divider.in_vstack.gap_below", scheme, shot.frame("b").minY - shot.frame("between").maxY)
        var c = shot.raster.sample(CGPoint(x: h.midX, y: h.minY + 0.25))
        if c.a == 0 { c = shot.raster.sample(CGPoint(x: h.midX, y: h.midY)) }
        store.colour("divider.colour", scheme, c)
        store.colour("divider.colour_flat", scheme, c.over(shot.raster.background))
        store.put("divider.ink_pixels", scheme, shot.raster.runLength(column: h.midX, from: h.minY - 2, to: h.maxY + 2, threshold: 0.05) * Harness.scale, "px@2x")
        store.image("divider", shot, h.union(v).union(shot.frame("b")))
    }

    static func symbols(_ store: Store, _ scheme: Scheme) {
        let view = VStack(alignment: .leading, spacing: 6) {
            ForEach(Typography.styles, id: \.0) { item in
                HStack(spacing: 4) {
                    Image(systemName: "star.fill").font(item.1).probe("\(item.0).image")
                    Text("Ag").font(item.1).probe("\(item.0).text")
                }
            }
            Image(systemName: "star.fill").imageScale(.small).probe("scale.small")
            Image(systemName: "star.fill").imageScale(.medium).probe("scale.medium")
            Image(systemName: "star.fill").imageScale(.large).probe("scale.large")
        }
        let shot = Harness.capture(view, scheme: scheme)
        for (name, _, style) in Typography.styles {
            let image = shot.frame("\(name).image"), text = shot.frame("\(name).text")
            let size = NSFont.preferredFont(forTextStyle: style).pointSize
            store.put("symbol.\(name).width", scheme, image.width)
            store.put("symbol.\(name).height", scheme, image.height)
            store.put("symbol.\(name).height_over_size", scheme, image.height / size, "ratio")
            store.put("symbol.\(name).text_height", scheme, text.height)
            if let ink = shot.raster.opaqueBounds(in: image, threshold: 0.3) {
                store.put("symbol.\(name).ink_height", scheme, ink.height)
                store.put("symbol.\(name).ink_over_size", scheme, ink.height / size, "ratio")
            }
        }
        for s in ["small", "medium", "large"] {
            store.put("symbol.image_scale.\(s).height", scheme, shot.frame("scale.\(s)").height)
        }
        store.image("symbols", shot, shot.frame("large_title.image").union(shot.frame("caption2.text")))
    }

    static func labels(_ store: Store, _ scheme: Scheme) {
        let view = VStack(alignment: .leading, spacing: 8) {
            Label { Text("Downloads").probe("title") } icon: { Image(systemName: "arrow.down.circle").probe("icon") }.probe("label")
            Label { Text("Title").font(.title).probe("ttitle") } icon: { Image(systemName: "star").font(.title).probe("ticon") }
            Label("Icon only", systemImage: "star").labelStyle(.iconOnly).probe("icononly")
            Label("Title only", systemImage: "star").labelStyle(.titleOnly).probe("titleonly")
        }
        let shot = Harness.capture(view, scheme: scheme)
        let f = shot.frame
        store.put("label.body.spacing", scheme, f("title").minX - f("icon").maxX)
        store.put("label.body.icon_width", scheme, f("icon").width)
        store.put("label.body.icon_height", scheme, f("icon").height)
        store.put("label.body.height", scheme, f("label").height)
        store.put("label.title.spacing", scheme, f("ttitle").minX - f("ticon").maxX)
        store.put("label.icon_only.width", scheme, f("icononly").width)
        store.image("label", shot, f("label"))
    }

    static func shadows(_ store: Store, _ scheme: Scheme) {
        let view = HStack(spacing: 40) {
            Rectangle().fill(.white).frame(width: 40, height: 40).shadow(radius: 8).probe("r8")
            Rectangle().fill(.white).frame(width: 40, height: 40).shadow(color: .black.opacity(0.5), radius: 0, x: 0, y: 6).probe("offset")
        }.padding(20)
        let shot = Harness.capture(view, scheme: scheme)
        let box = shot.frame("r8")
        var extent = 0.0
        var peak = 0.0
        for d in stride(from: 0.5, through: 30, by: 0.5) {
            let a = shot.raster.alpha(shot.raster.px(box.maxX + CGFloat(d)), shot.raster.px(box.midY))
            if d == 0.5 { peak = a }
            if a > 0.004 { extent = d }
        }
        store.put("shadow.default.alpha_at_edge", scheme, peak, "alpha")
        let shadowColour = shot.raster.straight(shot.raster.px(box.maxX + 1), shot.raster.px(box.midY))
        store.colour("shadow.default.colour", scheme, RGBA(r: shadowColour.r, g: shadowColour.g, b: shadowColour.b, a: 1))
        store.put("shadow.default.radius8_extent", scheme, extent)
        store.image("shadow", shot, box.insetBy(dx: -20, dy: -20))
    }

    static func shapes(_ store: Store, _ scheme: Scheme) {
        let view = HStack(spacing: 12) {
            RoundedRectangle(cornerRadius: 12, style: .continuous).fill(.blue).frame(width: 60, height: 60).probe("continuous")
            RoundedRectangle(cornerRadius: 12, style: .circular).fill(.blue).frame(width: 60, height: 60).probe("circular")
            Capsule().fill(.blue).frame(width: 60, height: 30).probe("capsule")
        }
        let shot = Harness.capture(view, scheme: scheme)
        store.put("shape.rounded12_continuous.measured_radius", scheme, shot.raster.alphaRadius(of: shot.frame("continuous")))
        store.put("shape.rounded12_circular.measured_radius", scheme, shot.raster.alphaRadius(of: shot.frame("circular")))
        store.put("shape.capsule30.measured_radius", scheme, shot.raster.alphaRadius(of: shot.frame("capsule")))
        store.colour("shape.blue", scheme, shot.raster.sampleFlat(CGPoint(x: shot.frame("circular").midX, y: shot.frame("circular").midY)))
    }
}
