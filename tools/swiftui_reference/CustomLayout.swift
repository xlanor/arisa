import AppKit
import SwiftUI

struct ReferenceFlow: SwiftUI.Layout {
    var spacing: CGFloat = 8

    func sizeThatFits(proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) -> CGSize {
        let width = proposal.width ?? .infinity
        var x: CGFloat = 0
        var y: CGFloat = 0
        var line: CGFloat = 0
        var widest: CGFloat = 0
        for subview in subviews {
            let size = subview.sizeThatFits(.unspecified)
            if x > 0 && x + size.width > width {
                y += line + spacing
                x = 0
                line = 0
            }
            x += size.width + spacing
            widest = max(widest, x - spacing)
            line = max(line, size.height)
        }
        return CGSize(width: proposal.width ?? widest, height: y + line)
    }

    func placeSubviews(in bounds: CGRect, proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) {
        var x = bounds.minX
        var y = bounds.minY
        var line: CGFloat = 0
        for subview in subviews {
            let size = subview.sizeThatFits(.unspecified)
            if x > bounds.minX && x + size.width > bounds.maxX {
                y += line + spacing
                x = bounds.minX
                line = 0
            }
            subview.place(at: CGPoint(x: x, y: y), proposal: .unspecified)
            x += size.width + spacing
            line = max(line, size.height)
        }
    }
}

struct ReferenceRank: LayoutValueKey {
    static let defaultValue: Int = 0
}

struct ReferenceRanked: SwiftUI.Layout {
    func sizeThatFits(proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) -> CGSize {
        let sizes = subviews.map { $0.sizeThatFits(.unspecified) }
        return CGSize(width: sizes.reduce(0) { $0 + $1.width }, height: sizes.map(\.height).max() ?? 0)
    }

    func placeSubviews(in bounds: CGRect, proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) {
        var x = bounds.minX
        for subview in subviews.sorted(by: { $0[ReferenceRank.self] < $1[ReferenceRank.self] }) {
            let size = subview.sizeThatFits(.unspecified)
            subview.place(at: CGPoint(x: x, y: bounds.minY), proposal: .unspecified)
            x += size.width
        }
    }
}

extension HorizontalAlignment {
    struct ReferenceColon: AlignmentID {
        static func defaultValue(in context: ViewDimensions) -> CGFloat { context[.leading] }
    }

    static let referenceColon = HorizontalAlignment(ReferenceColon.self)
}

enum CustomLayout {
    static func run(_ store: Store) {
        let scheme = Scheme.light
        flow(store, scheme)
        values(store, scheme)
        fits(store, scheme)
        guides(store, scheme)
        baselines(store, scheme)
        switching(store, scheme)
        nested(store, scheme)
        zOrder(store, scheme)
    }

    static func box(_ width: CGFloat, _ height: CGFloat) -> some View {
        Color.gray.frame(width: width, height: height)
    }

    static func flow(_ store: Store, _ scheme: Scheme) {
        let shot = Harness.capture(
            ReferenceFlow(spacing: 8).callAsFunction {
                box(60, 20).probe("t0")
                box(80, 20).probe("t1")
                box(50, 20).probe("t2")
                box(70, 20).probe("t3")
            }.frame(width: 200).probe("flow"),
            scheme: scheme)
        let origin = shot.frame("flow")
        for index in 0..<4 {
            store.put("flow.t\(index)_x", scheme, shot.frame("t\(index)").minX - origin.minX)
            store.put("flow.t\(index)_y", scheme, shot.frame("t\(index)").minY - origin.minY)
        }
        store.put("flow.height", scheme, origin.height)
        store.put("flow.width", scheme, origin.width)
    }

    static func values(_ store: Store, _ scheme: Scheme) {
        let shot = Harness.capture(
            ReferenceRanked().callAsFunction {
                box(30, 10).layoutValue(key: ReferenceRank.self, value: 2).probe("a")
                box(40, 10).layoutValue(key: ReferenceRank.self, value: 1).probe("b")
            }.probe("row"),
            scheme: scheme)
        store.put("values.a_x", scheme, shot.frame("a").minX - shot.frame("row").minX)
        store.put("values.b_x", scheme, shot.frame("b").minX - shot.frame("row").minX)
    }

    static func fits(_ store: Store, _ scheme: Scheme) {
        for width in [CGFloat(300), 120, 40] {
            let shot = Harness.capture(
                ViewThatFits {
                    box(200, 20).probe("wide")
                    box(100, 20).probe("medium")
                    box(60, 20).probe("narrow")
                }.frame(width: width).probe("fits"),
                scheme: scheme)
            let picked = shot.frames["wide"] != nil ? 200 : shot.frames["medium"] != nil ? 100 : 60
            store.put("fits.width\(Int(width)).picked", scheme, Double(picked))
            store.put("fits.width\(Int(width)).height", scheme, shot.frame("fits").height)
        }
        let vertical = Harness.capture(
            ViewThatFits(in: .vertical) {
                box(400, 80).probe("tall")
                box(400, 20).probe("short")
            }.frame(width: 100, height: 40).probe("fits"),
            scheme: scheme)
        store.put("fits.vertical.picked_height", scheme, vertical.frames["tall"] != nil ? 80 : 20)
    }

    static func guides(_ store: Store, _ scheme: Scheme) {
        let leading = Harness.capture(
            VStack(alignment: .leading, spacing: 0) {
                box(40, 10).alignmentGuide(.leading) { d in d[.trailing] }.probe("a")
                box(60, 10).probe("b")
            }.probe("stack"),
            scheme: scheme)
        store.put("guides.leading.a_x", scheme, leading.frame("a").minX - leading.frame("stack").minX)
        store.put("guides.leading.b_x", scheme, leading.frame("b").minX - leading.frame("stack").minX)
        store.put("guides.leading.width", scheme, leading.frame("stack").width)
        let constant = Harness.capture(
            VStack(alignment: .leading, spacing: 0) {
                box(40, 10).alignmentGuide(.leading) { _ in 15 }.probe("a")
                box(60, 10).probe("b")
            }.probe("stack"),
            scheme: scheme)
        store.put("guides.constant.a_x", scheme, constant.frame("a").minX - constant.frame("stack").minX)
        store.put("guides.constant.b_x", scheme, constant.frame("b").minX - constant.frame("stack").minX)
        let form = Harness.capture(
            VStack(alignment: .referenceColon, spacing: 0) {
                HStack(spacing: 4) {
                    box(30, 10).probe("l0")
                    box(50, 10).alignmentGuide(.referenceColon) { d in d[.leading] }.probe("f0")
                }
                HStack(spacing: 4) {
                    box(70, 10).probe("l1")
                    box(50, 10).alignmentGuide(.referenceColon) { d in d[.leading] }.probe("f1")
                }
            }.probe("form"),
            scheme: scheme)
        let origin = form.frame("form")
        store.put("guides.form.l0_x", scheme, form.frame("l0").minX - origin.minX)
        store.put("guides.form.f0_x", scheme, form.frame("f0").minX - origin.minX)
        store.put("guides.form.l1_x", scheme, form.frame("l1").minX - origin.minX)
        store.put("guides.form.f1_x", scheme, form.frame("f1").minX - origin.minX)
        store.put("guides.form.width", scheme, origin.width)
        let row = Harness.capture(
            HStack(alignment: .top, spacing: 0) {
                box(20, 40).probe("a")
                box(20, 20).alignmentGuide(.top) { d in d[.bottom] }.probe("b")
            }.probe("row"),
            scheme: scheme)
        store.put("guides.row.a_y", scheme, row.frame("a").minY - row.frame("row").minY)
        store.put("guides.row.b_y", scheme, row.frame("b").minY - row.frame("row").minY)
        store.put("guides.row.height", scheme, row.frame("row").height)
    }

    static func baselines(_ store: Store, _ scheme: Scheme) {
        let shot = Harness.capture(
            HStack(alignment: .lastTextBaseline, spacing: 0) {
                Text("One\nTwo").probe("two")
                Text("Big").font(.title).probe("big")
            }.probe("row"),
            scheme: scheme)
        store.put("baseline.last.two_y", scheme, shot.frame("two").minY - shot.frame("row").minY)
        store.put("baseline.last.big_y", scheme, shot.frame("big").minY - shot.frame("row").minY)
        store.put("baseline.last.height", scheme, shot.frame("row").height)
        let first = Harness.capture(
            HStack(alignment: .firstTextBaseline, spacing: 0) {
                Text("One\nTwo").probe("two")
                Text("Big").font(.title).probe("big")
            }.probe("row"),
            scheme: scheme)
        store.put("baseline.first.two_y", scheme, first.frame("two").minY - first.frame("row").minY)
        store.put("baseline.first.big_y", scheme, first.frame("big").minY - first.frame("row").minY)
    }

    static func switching(_ store: Store, _ scheme: Scheme) {
        for (name, layout) in [("h", AnyLayout(HStackLayout(spacing: 4))), ("v", AnyLayout(VStackLayout(spacing: 4)))] {
            let shot = Harness.capture(
                layout {
                    box(30, 20).probe("a")
                    box(40, 10).probe("b")
                }.probe("stack"),
                scheme: scheme)
            store.put("any_layout.\(name).b_x", scheme, shot.frame("b").minX - shot.frame("stack").minX)
            store.put("any_layout.\(name).b_y", scheme, shot.frame("b").minY - shot.frame("stack").minY)
            store.put("any_layout.\(name).width", scheme, shot.frame("stack").width)
            store.put("any_layout.\(name).height", scheme, shot.frame("stack").height)
        }
    }

    static func nested(_ store: Store, _ scheme: Scheme) {
        let shot = Harness.capture(
            VStack(spacing: 0) {
                box(30, 10).probe("inner").frame(height: 40).probe("framed").frame(maxWidth: .infinity).probe("outer")
                box(200, 2)
            },
            scheme: scheme)
        store.put("nested_frame.outer_width", scheme, shot.frame("outer").width)
        store.put("nested_frame.outer_height", scheme, shot.frame("outer").height)
        store.put("nested_frame.framed_width", scheme, shot.frame("framed").width)
        store.put("nested_frame.inner_x", scheme, shot.frame("inner").minX - shot.frame("outer").minX)
        store.put("nested_frame.inner_y", scheme, shot.frame("inner").minY - shot.frame("outer").minY)
        let widened = Harness.capture(
            VStack(spacing: 0) {
                box(30, 10).frame(width: 60).probe("framed").frame(width: 120, alignment: .leading).probe("outer")
            },
            scheme: scheme)
        store.put("nested_frame.widened.outer_width", scheme, widened.frame("outer").width)
        store.put("nested_frame.widened.framed_x", scheme, widened.frame("framed").minX - widened.frame("outer").minX)
    }

    static func zOrder(_ store: Store, _ scheme: Scheme) {
        let shot = Harness.capture(
            ZStack {
                ForEach(0..<2, id: \.self) { index in
                    (index == 0 ? Color.red : Color.green).frame(width: 40, height: 40).zIndex(index == 0 ? 2 : 0)
                }
                Color.blue.frame(width: 40, height: 40).zIndex(1)
            }.probe("stack"),
            scheme: scheme)
        let box = shot.frame("stack")
        store.colour("z_index.across_for_each.top", scheme, shot.raster.sampleFlat(CGPoint(x: box.midX, y: box.midY)))
    }
}
