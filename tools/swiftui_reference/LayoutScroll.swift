import AppKit
import SwiftUI

struct ReferenceWedge: Shape {
    func path(in rect: CGRect) -> Path {
        var path = Path()
        path.move(to: CGPoint(x: rect.minX, y: rect.minY))
        path.addLine(to: CGPoint(x: rect.midX, y: rect.minY))
        path.addLine(to: CGPoint(x: rect.minX, y: rect.maxY))
        path.closeSubpath()
        return path
    }
}

struct ReferenceFixedWedge: Shape {
    var layoutDirectionBehavior: LayoutDirectionBehavior { .fixed }

    func path(in rect: CGRect) -> Path {
        ReferenceWedge().path(in: rect)
    }
}

final class ReferenceDirectionSink {
    static var seen: [String] = []
}

struct ReferenceDirectionLayout: SwiftUI.Layout {
    func sizeThatFits(proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) -> CGSize {
        ReferenceDirectionSink.seen.append(subviews.layoutDirection == .rightToLeft ? "rtl" : "ltr")
        return CGSize(width: 40, height: 10)
    }

    func placeSubviews(in bounds: CGRect, proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) {
        for subview in subviews {
            subview.place(at: bounds.origin, proposal: .unspecified)
        }
    }
}

extension HorizontalAlignment {
    struct ReferenceCombined: AlignmentID {
        static func defaultValue(in context: ViewDimensions) -> CGFloat { context[.leading] }
    }

    static let referenceCombined = HorizontalAlignment(ReferenceCombined.self)
}

struct ReferenceShift: GeometryEffect {
    var x: CGFloat

    func effectValue(size: CGSize) -> ProjectionTransform {
        ProjectionTransform(CGAffineTransform(translationX: x, y: 0))
    }
}

final class ReferenceGeometrySink {
    static var global: [String: CGFloat] = [:]
    static var scroll: ScrollGeometry?
    static var calls: [String] = []
}

struct ReferenceChangeProbe: View {
    @State var width: CGFloat = 40

    var body: some View {
        Color.gray.frame(width: width, height: 10)
            .onGeometryChange(for: CGFloat.self, of: { $0.size.width }) { old, new in
                ReferenceGeometrySink.calls.append("\(Int(old))->\(Int(new))")
            }
            .task {
                try? await Task.sleep(nanoseconds: 30_000_000)
                width = 60
            }
    }
}

@MainActor enum LayoutScroll {
    static func run(_ store: Store) {
        let scheme = Scheme.light
        shapes(store, scheme)
        combine(store, scheme)
        scrollGeometry(store, scheme)
        geometryChange(store, scheme)
        ignored(store, scheme)
        values(store)
        transitions(store, scheme)
        environmentKeys(store, scheme)
        marginPlacement(store, scheme)
        negativeSpacing(store, scheme)
        scenePadding(store, scheme)
        cornerInsets(store)
        safeAreaRegions(store)
        containerCorners(store)
        layoutProtocol(store, scheme)
        staticLazyStacks(store, scheme)
        gridRowPaint(store, scheme)
        gridInFrame(store, scheme)
        radioGroupLayout(store, scheme)
        listBackground(store, .light)
        listBackground(store, .dark)
        idealQuery(store, scheme)
        if #available(macOS 27.1, *) {
            arrangement(store, scheme)
        }
    }

    static func wedgeSide(_ view: some View) -> String {
        guard let shot = Effects.pixels(ZStack { Color.white; view }, width: 80, height: 80) else { return "none" }
        var left = 0
        var right = 0
        for x in 0..<shot.width {
            if shot.straight(x, 10).r < 0.5 {
                if x < 40 { left += 1 } else { right += 1 }
            }
        }
        return left > right ? "left" : "right"
    }

    static func shapes(_ store: Store, _ scheme: Scheme) {
        let rtl = LayoutDirection.rightToLeft
        let cases: [(String, AnyView)] = [
            ("custom_ltr", AnyView(ReferenceWedge().fill(Color.black))),
            ("custom_rtl", AnyView(ReferenceWedge().fill(Color.black).environment(\.layoutDirection, rtl))),
            ("fixed_rtl", AnyView(ReferenceFixedWedge().fill(Color.black).environment(\.layoutDirection, rtl))),
            ("path_rtl", AnyView(ReferenceWedge().path(in: CGRect(x: 0, y: 0, width: 80, height: 80)).fill(Color.black).environment(\.layoutDirection, rtl))),
            ("offset_rtl", AnyView(ReferenceWedge().offset(x: 0, y: 0).fill(Color.black).environment(\.layoutDirection, rtl))),
            ("fixed_offset_rtl", AnyView(ReferenceFixedWedge().offset(x: 0, y: 0).fill(Color.black).environment(\.layoutDirection, rtl))),
            ("rotated_rtl", AnyView(ReferenceWedge().rotation(.degrees(0)).fill(Color.black).environment(\.layoutDirection, rtl))),
            ("scaled_rtl", AnyView(ReferenceWedge().scale(1).fill(Color.black).environment(\.layoutDirection, rtl))),
            ("transformed_rtl", AnyView(ReferenceWedge().transform(.identity).fill(Color.black).environment(\.layoutDirection, rtl))),
        ]
        for (name, view) in cases {
            store.put("layout_scroll.shape.\(name)", nil, text: wedgeSide(view), "side")
        }
        let behaviors: [(String, LayoutDirectionBehavior)] = [
            ("shape", ReferenceWedge().layoutDirectionBehavior),
            ("rectangle", Rectangle().layoutDirectionBehavior),
            ("rounded_rectangle", RoundedRectangle(cornerRadius: 4).layoutDirectionBehavior),
            ("capsule", Capsule().layoutDirectionBehavior),
            ("circle", Circle().layoutDirectionBehavior),
            ("ellipse", Ellipse().layoutDirectionBehavior),
            ("uneven", UnevenRoundedRectangle(topLeadingRadius: 4).layoutDirectionBehavior),
            ("offset_rectangle", Rectangle().offset(x: 1, y: 0).layoutDirectionBehavior),
            ("offset_custom", ReferenceWedge().offset(x: 1, y: 0).layoutDirectionBehavior),
            ("rotated_rectangle", Rectangle().rotation(.degrees(10)).layoutDirectionBehavior),
            ("scaled_circle", Circle().scale(2).layoutDirectionBehavior),
            ("transformed_capsule", Capsule().transform(.identity).layoutDirectionBehavior),
            ("transformed_custom", ReferenceWedge().transform(.identity).layoutDirectionBehavior),
        ]
        for (name, behavior) in behaviors {
            store.put("layout_scroll.behavior.\(name)", nil, text: behavior == .fixed ? "fixed" : "mirrors", "behavior")
        }
        ReferenceDirectionSink.seen = []
        _ = Harness.capture(ReferenceDirectionLayout { Color.gray }.environment(\.layoutDirection, rtl), scheme: scheme)
        store.put("layout_scroll.subviews.rtl", nil, text: ReferenceDirectionSink.seen.last ?? "none", "direction")
        ReferenceDirectionSink.seen = []
        _ = Harness.capture(ReferenceDirectionLayout { Color.gray }, scheme: scheme)
        store.put("layout_scroll.subviews.ltr", nil, text: ReferenceDirectionSink.seen.last ?? "none", "direction")
    }

    static func box(_ width: CGFloat) -> some View {
        Color.gray.frame(width: width, height: 10)
    }

    static func combine(_ store: Store, _ scheme: Scheme) {
        let custom = Harness.capture(
            VStack(alignment: .referenceCombined, spacing: 0) {
                box(100).probe("anchor")
                VStack(alignment: .leading, spacing: 0) {
                    box(40).alignmentGuide(.referenceCombined) { _ in 10 }.probe("a")
                    box(40).alignmentGuide(.referenceCombined) { _ in 30 }.probe("b")
                }.probe("inner")
            }.probe("outer"),
            scheme: scheme)
        let origin = custom.frame("outer")
        store.put("layout_scroll.combine.custom.inner_x", nil, custom.frame("inner").minX - origin.minX)
        store.put("layout_scroll.combine.custom.anchor_x", nil, custom.frame("anchor").minX - origin.minX)
        let leading = Harness.capture(
            VStack(alignment: .leading, spacing: 0) {
                box(100).probe("anchor")
                VStack(alignment: .leading, spacing: 0) {
                    box(40).alignmentGuide(.leading) { _ in 10 }.probe("a")
                    box(40).alignmentGuide(.leading) { _ in 30 }.probe("b")
                }.probe("inner")
            }.probe("outer"),
            scheme: scheme)
        let base = leading.frame("outer")
        store.put("layout_scroll.combine.leading.inner_x", nil, leading.frame("inner").minX - base.minX)
        store.put("layout_scroll.combine.leading.anchor_x", nil, leading.frame("anchor").minX - base.minX)
        store.put("layout_scroll.combine.mean", nil, HorizontalAlignment.leading.combineExplicit([10, nil, 30]) ?? -1)
        store.put("layout_scroll.combine.custom_mean", nil, HorizontalAlignment.referenceCombined.combineExplicit([10, nil, 30]) ?? -1)
        store.put("layout_scroll.combine.first_baseline", nil, VerticalAlignment.firstTextBaseline.combineExplicit([10, nil, 30]) ?? -1)
        store.put("layout_scroll.combine.last_baseline", nil, VerticalAlignment.lastTextBaseline.combineExplicit([10, nil, 30]) ?? -1)
        store.put("layout_scroll.combine.top", nil, VerticalAlignment.top.combineExplicit([10, nil, 30]) ?? -1)
        store.put("layout_scroll.combine.empty", nil, HorizontalAlignment.center.combineExplicit([nil]) ?? -1)
    }

    static func scrollGeometry(_ store: Store, _ scheme: Scheme) {
        ReferenceGeometrySink.scroll = nil
        _ = Harness.capture(
            ScrollView {
                Color.gray.frame(width: 100, height: 400)
            }
            .contentMargins(.vertical, 20)
            .contentMargins(.leading, 10)
            .onScrollGeometryChange(for: ScrollGeometry.self, of: { $0 }) { _, new in
                ReferenceGeometrySink.scroll = new
            }
            .frame(width: 200, height: 150),
            scheme: scheme, settle: 0.3)
        guard let geometry = ReferenceGeometrySink.scroll else { return }
        let prefix = "layout_scroll.scroll_geometry"
        store.put("\(prefix).inset_top", nil, geometry.contentInsets.top)
        store.put("\(prefix).inset_leading", nil, geometry.contentInsets.leading)
        store.put("\(prefix).inset_bottom", nil, geometry.contentInsets.bottom)
        store.put("\(prefix).inset_trailing", nil, geometry.contentInsets.trailing)
        store.put("\(prefix).offset_x", nil, geometry.contentOffset.x)
        store.put("\(prefix).offset_y", nil, geometry.contentOffset.y)
        store.put("\(prefix).content_width", nil, geometry.contentSize.width)
        store.put("\(prefix).content_height", nil, geometry.contentSize.height)
        store.put("\(prefix).container_width", nil, geometry.containerSize.width)
        store.put("\(prefix).container_height", nil, geometry.containerSize.height)
        store.put("\(prefix).visible_x", nil, geometry.visibleRect.minX)
        store.put("\(prefix).visible_y", nil, geometry.visibleRect.minY)
        store.put("\(prefix).visible_width", nil, geometry.visibleRect.width)
        store.put("\(prefix).visible_height", nil, geometry.visibleRect.height)
        store.put("\(prefix).bounds_x", nil, geometry.bounds.minX)
        store.put("\(prefix).bounds_y", nil, geometry.bounds.minY)
        store.put("\(prefix).bounds_width", nil, geometry.bounds.width)
        store.put("\(prefix).bounds_height", nil, geometry.bounds.height)
        let built = ScrollGeometry(contentOffset: CGPoint(x: 5, y: 7), contentSize: CGSize(width: 300, height: 500), contentInsets: EdgeInsets(top: 4, leading: 3, bottom: 2, trailing: 1), containerSize: CGSize(width: 100, height: 200))
        store.put("\(prefix).built.visible_x", nil, built.visibleRect.minX)
        store.put("\(prefix).built.visible_y", nil, built.visibleRect.minY)
        store.put("\(prefix).built.visible_width", nil, built.visibleRect.width)
        store.put("\(prefix).built.visible_height", nil, built.visibleRect.height)
        store.put("\(prefix).built.bounds_x", nil, built.bounds.minX)
        store.put("\(prefix).built.bounds_y", nil, built.bounds.minY)
        store.put("\(prefix).built.bounds_width", nil, built.bounds.width)
        store.put("\(prefix).built.bounds_height", nil, built.bounds.height)
    }

    static func geometryChange(_ store: Store, _ scheme: Scheme) {
        ReferenceGeometrySink.calls = []
        _ = Harness.capture(ReferenceChangeProbe(), scheme: scheme, settle: 0.3)
        store.put("layout_scroll.geometry_change.calls", nil, text: ReferenceGeometrySink.calls.joined(separator: ","), "calls")
    }

    static func ignored(_ store: Store, _ scheme: Scheme) {
        let shot = Harness.capture(
            HStack(spacing: 0) {
                box(20).probe("plain")
                box(20).probe("shifted").modifier(ReferenceShift(x: 30))
                box(20).probe("ignored").modifier(ReferenceShift(x: 30).ignoredByLayout())
            }.probe("row"),
            scheme: scheme)
        let origin = shot.frame("row")
        store.put("layout_scroll.ignored.plain_x", nil, shot.frame("plain").minX - origin.minX)
        store.put("layout_scroll.ignored.shifted_x", nil, shot.frame("shifted").minX - origin.minX)
        store.put("layout_scroll.ignored.ignored_x", nil, shot.frame("ignored").minX - origin.minX)
    }

    static func values(_ store: Store) {
        let along = ScrollPosition(x: 12)
        store.put("layout_scroll.position.x_only.x", nil, Double(along.x ?? -1))
        store.put("layout_scroll.position.x_only.y", nil, text: along.y == nil ? "nil" : "set", "value")
        store.put("layout_scroll.position.x_only.point", nil, text: along.point == nil ? "nil" : "set", "value")
        let down = ScrollPosition(y: 34)
        store.put("layout_scroll.position.y_only.y", nil, Double(down.y ?? -1))
        store.put("layout_scroll.position.y_only.x", nil, text: down.x == nil ? "nil" : "set", "value")
        let phases: [(String, ScrollPhase)] = [("idle", .idle), ("tracking", .tracking), ("interacting", .interacting), ("decelerating", .decelerating), ("animating", .animating)]
        for (name, phase) in phases {
            store.put("layout_scroll.phase.\(name)", nil, text: phase.isScrolling ? "true" : "false", "bool")
        }
        store.put("layout_scroll.orientation.count", nil, Double(InterfaceOrientation.allCases.count))
        store.put("layout_scroll.orientation.order", nil, text: InterfaceOrientation.allCases.map(\.id).joined(separator: ","), "ids")
        store.put("layout_scroll.corner.order", nil, text: Edge.Corner.allCases.map { "\($0.rawValue)" }.joined(separator: ","), "raw")
        store.put("layout_scroll.edge.order", nil, text: Edge.allCases.map { "\($0.rawValue)" }.joined(separator: ","), "raw")
        store.put("layout_scroll.corner_set.leading_raw", nil, Double(Edge.Corner.Set.leading.rawValue))
        store.put("layout_scroll.corner_set.top_raw", nil, Double(Edge.Corner.Set.top.rawValue))
        store.put("layout_scroll.corner_set.none_raw", nil, Double(Edge.Corner.Set.none.rawValue))
        store.put("layout_scroll.corner_set.all_raw", nil, Double(Edge.Corner.Set.all.rawValue))
    }

    static func transitionShift(_ configuration: ScrollTransitionConfiguration, top: CGFloat) -> Double {
        let item: CGFloat = 40
        let viewport: CGFloat = 100
        let tile = Rectangle().fill(Color.red).frame(width: item, height: item)
            .scrollTransition(configuration, axis: .vertical) { content, phase in
                content.offset(x: 100 + phase.value * 100)
            }
        let above = top >= 0
        let lead: CGFloat = above ? top : 0
        let trail: CGFloat = above ? 400 : viewport - item - top
        let shot = Harness.capture(
            ScrollView {
                VStack(alignment: .leading, spacing: 0) {
                    Color.clear.frame(height: lead)
                    tile
                    Color.clear.frame(height: trail)
                }
                .frame(width: 300, alignment: .leading)
            }
            .defaultScrollAnchor(above ? .top : .bottom)
            .scrollIndicators(.never)
            .frame(width: 300, height: viewport)
            .probe("view"),
            scheme: .light, settle: 0.4)
        let view = shot.frame("view")
        let row = view.minY + (max(top, 0) + min(top + item, viewport)) / 2
        let y = shot.raster.px(row)
        for x in shot.raster.px(view.minX)..<shot.raster.px(view.maxX) {
            let colour = shot.raster.straight(x, y)
            if colour.a > 0.5 && colour.r > 0.7 && colour.g < 0.4 {
                return (Double(x) / Double(shot.raster.scale) - Double(view.minX) - 100) / 100
            }
        }
        return -9
    }

    static func transitions(_ store: Store, _ scheme: Scheme) {
        let cases: [(String, ScrollTransitionConfiguration)] = [
            ("interactive", .interactive),
            ("linear", .interactive(timingCurve: .linear)),
            ("visible_half", .interactive(timingCurve: .linear).threshold(.visible(0.5))),
            ("hidden", .interactive(timingCurve: .linear).threshold(.hidden)),
            ("visible_zero", .interactive(timingCurve: .linear).threshold(.visible(0))),
            ("visible_tenth", .interactive(timingCurve: .linear).threshold(.visible(0.1))),
            ("hidden_inset", .interactive(timingCurve: .linear).threshold(.hidden.inset(by: 10))),
            ("visible_negative_inset", .interactive(timingCurve: .linear).threshold(.visible.inset(by: -10))),
            ("animated", .animated(.linear(duration: 0.01))),
            ("animated_full", .animated(.linear(duration: 0.01)).threshold(.visible)),
            ("animated_centered", .animated(.linear(duration: 0.01)).threshold(.centered)),
            ("animated_half", .animated(.linear(duration: 0.01)).threshold(.visible(0.5))),
            ("centered", .interactive(timingCurve: .linear).threshold(.centered)),
            ("inset", .interactive(timingCurve: .linear).threshold(.visible.inset(by: 10))),
            ("interpolated", .interactive(timingCurve: .linear).threshold(.visible.interpolated(towards: .hidden, amount: 0.5))),
            ("centered_inset", .interactive(timingCurve: .linear).threshold(.centered.inset(by: 10))),
            ("hidden_interpolated", .interactive(timingCurve: .linear).threshold(.hidden.interpolated(towards: .centered, amount: 0.5))),
            ("identity", .identity),
        ]
        for (name, configuration) in cases {
            for top in [-30, -20, -10, 0, 10, 25, 30, 35, 50, 60, 65, 70, 80, 90, 95] {
                store.put("layout_scroll.transition.\(name).top_\(top)", nil, transitionShift(configuration, top: CGFloat(top)))
            }
        }
    }
}

final class ReferenceScrollEnvironmentSink {
    static var values: [String: String] = [:]
}

struct ReferenceScrollEnvironmentReader: View {
    let name: String
    @Environment(\.isScrollEnabled) var enabled
    @Environment(\.horizontalScrollIndicatorVisibility) var horizontalIndicators
    @Environment(\.verticalScrollIndicatorVisibility) var verticalIndicators
    @Environment(\.horizontalScrollBounceBehavior) var horizontalBounce
    @Environment(\.verticalScrollBounceBehavior) var verticalBounce
    @Environment(\.scrollDismissesKeyboardMode) var keyboard

    static func indicator(_ value: ScrollIndicatorVisibility) -> String {
        String(describing: value)
    }

    static func bounce(_ value: ScrollBounceBehavior) -> String {
        String(describing: value)
    }

    static func dismissal(_ value: ScrollDismissesKeyboardMode) -> String {
        if value == .automatic { return "automatic" }
        if value == .immediately { return "immediately" }
        if value == .interactively { return "interactively" }
        if value == .never { return "never" }
        return "other"
    }

    var body: some View {
        ReferenceScrollEnvironmentSink.values["\(name).enabled"] = enabled ? "true" : "false"
        ReferenceScrollEnvironmentSink.values["\(name).horizontal_indicators"] = Self.indicator(horizontalIndicators)
        ReferenceScrollEnvironmentSink.values["\(name).vertical_indicators"] = Self.indicator(verticalIndicators)
        ReferenceScrollEnvironmentSink.values["\(name).horizontal_bounce"] = Self.bounce(horizontalBounce)
        ReferenceScrollEnvironmentSink.values["\(name).vertical_bounce"] = Self.bounce(verticalBounce)
        ReferenceScrollEnvironmentSink.values["\(name).keyboard"] = Self.dismissal(keyboard)
        return Color.gray.frame(width: 10, height: 10)
    }
}

extension LayoutScroll {
    static func environmentKeys(_ store: Store, _ scheme: Scheme) {
        ReferenceScrollEnvironmentSink.values = [:]
        _ = Harness.capture(
            VStack {
                ReferenceScrollEnvironmentReader(name: "plain")
                ReferenceScrollEnvironmentReader(name: "disabled").scrollDisabled(true)
                ReferenceScrollEnvironmentReader(name: "reenabled").scrollDisabled(false).scrollDisabled(true)
                ReferenceScrollEnvironmentReader(name: "indicators").scrollIndicators(.hidden, axes: .horizontal)
                ReferenceScrollEnvironmentReader(name: "indicators_all").scrollIndicators(.never)
                ReferenceScrollEnvironmentReader(name: "bounce").scrollBounceBehavior(.basedOnSize)
                ReferenceScrollEnvironmentReader(name: "bounce_both").scrollBounceBehavior(.always, axes: [.horizontal, .vertical])
                ReferenceScrollEnvironmentReader(name: "keyboard").scrollDismissesKeyboard(.interactively)
                ScrollView { ReferenceScrollEnvironmentReader(name: "in_scroll") }.frame(height: 20)
                ScrollView { ReferenceScrollEnvironmentReader(name: "in_disabled_scroll") }.scrollDisabled(true).frame(height: 20)
                ScrollView { ReferenceScrollEnvironmentReader(name: "in_hidden_scroll") }.scrollIndicators(.hidden).frame(height: 20)
                ScrollView(showsIndicators: false) { ReferenceScrollEnvironmentReader(name: "in_shows_false") }.frame(height: 20)
                List { ReferenceScrollEnvironmentReader(name: "in_list") }.frame(height: 40)
                TextEditor(text: .constant("")).frame(height: 20)
            },
            scheme: scheme, settle: 0.2)
        for (key, value) in ReferenceScrollEnvironmentSink.values.sorted(by: { $0.key < $1.key }) {
            store.put("layout_scroll.environment.\(key)", nil, text: value, "value")
        }
    }

    static func scrollViews(_ view: NSView) -> [NSScrollView] {
        var found: [NSScrollView] = []
        if let scroll = view as? NSScrollView { found.append(scroll) }
        for child in view.subviews { found.append(contentsOf: scrollViews(child)) }
        return found
    }

    static func marginPlacement(_ store: Store, _ scheme: Scheme) {
        let cases: [(String, ContentMarginPlacement?)] = [("none", nil), ("automatic", .automatic), ("scroll_content", .scrollContent), ("scroll_indicators", .scrollIndicators)]
        for (name, placement) in cases {
            ReferenceGeometrySink.scroll = nil
            let content = ScrollView {
                Color.gray.frame(width: 100, height: 400).probe("content")
            }
            .scrollIndicators(.visible)
            .onScrollGeometryChange(for: ScrollGeometry.self, of: { $0 }) { _, new in
                ReferenceGeometrySink.scroll = new
            }
            let view: AnyView
            if let placement {
                view = AnyView(content.contentMargins(.vertical, 20, for: placement).contentMargins(.trailing, 12, for: placement))
            } else {
                view = AnyView(content)
            }
            let window = NSWindow(contentRect: NSRect(x: -10000, y: -10000, width: 200, height: 150), styleMask: [.borderless], backing: .buffered, defer: true)
            let host = NSHostingView(rootView: view.frame(width: 200, height: 150).probe("frame").coordinateSpace(.named("reference.root")))
            window.contentView = host
            host.frame = NSRect(x: 0, y: 0, width: 200, height: 150)
            host.layoutSubtreeIfNeeded()
            RunLoop.main.run(until: Date().addingTimeInterval(0.3))
            host.layoutSubtreeIfNeeded()
            let prefix = "layout_scroll.margin_placement.\(name)"
            if let geometry = ReferenceGeometrySink.scroll {
                store.put("\(prefix).inset_top", nil, geometry.contentInsets.top)
                store.put("\(prefix).inset_bottom", nil, geometry.contentInsets.bottom)
                store.put("\(prefix).inset_trailing", nil, geometry.contentInsets.trailing)
                store.put("\(prefix).offset_y", nil, geometry.contentOffset.y)
                store.put("\(prefix).container_height", nil, geometry.containerSize.height)
                store.put("\(prefix).content_height", nil, geometry.contentSize.height)
            }
            if let scroll = scrollViews(host).first {
                store.put("\(prefix).scroller_inset_top", nil, scroll.scrollerInsets.top)
                store.put("\(prefix).scroller_inset_bottom", nil, scroll.scrollerInsets.bottom)
                store.put("\(prefix).scroller_inset_right", nil, scroll.scrollerInsets.right)
                store.put("\(prefix).content_inset_top", nil, scroll.contentInsets.top)
                store.put("\(prefix).content_inset_bottom", nil, scroll.contentInsets.bottom)
                store.put("\(prefix).content_inset_right", nil, scroll.contentInsets.right)
                if let scroller = scroll.verticalScroller {
                    let frame = scroller.convert(scroller.bounds, to: host)
                    store.put("\(prefix).scroller_y", nil, host.isFlipped ? frame.minY : 150 - frame.maxY)
                    store.put("\(prefix).scroller_height", nil, frame.height)
                    store.put("\(prefix).scroller_x", nil, frame.minX)
                }
            } else {
                store.put("\(prefix).scroll_view", nil, text: "missing", "value")
            }
            window.contentView = nil
        }
    }

    static func hosted<V: View>(_ view: V, width: CGFloat, height: CGFloat) -> [String: CGRect] {
        let sink = FrameSink()
        let root = view
            .coordinateSpace(.named("reference.root"))
            .onPreferenceChange(ProbeFrames.self) { sink.frames = $0 }
        let window = NSWindow(contentRect: NSRect(x: -10000, y: -10000, width: width, height: height), styleMask: [.borderless], backing: .buffered, defer: true)
        let host = NSHostingView(rootView: root)
        window.contentView = host
        host.frame = NSRect(x: 0, y: 0, width: width, height: height)
        host.layoutSubtreeIfNeeded()
        RunLoop.main.run(until: Date().addingTimeInterval(0.2))
        host.layoutSubtreeIfNeeded()
        RunLoop.main.run(until: Date().addingTimeInterval(0.05))
        window.contentView = nil
        return sink.frames
    }

    static func negativeSpacing(_ store: Store, _ scheme: Scheme) {
        let row = Harness.capture(
            HStack(spacing: -10) {
                Color.gray.frame(width: 20, height: 10).probe("a")
                Color.gray.frame(width: 20, height: 10).probe("b")
            }.probe("row"),
            scheme: scheme)
        store.put("layout_scroll.negative.hstack_width", nil, row.frame("row").width)
        store.put("layout_scroll.negative.hstack_second_x", nil, row.frame("b").minX - row.frame("row").minX)
        let column = Harness.capture(
            VStack(spacing: -10) {
                Color.gray.frame(width: 20, height: 30).probe("a")
                Color.gray.frame(width: 20, height: 30).probe("b")
            }.probe("column"),
            scheme: scheme)
        store.put("layout_scroll.negative.vstack_height", nil, column.frame("column").height)
        store.put("layout_scroll.negative.vstack_second_y", nil, column.frame("b").minY - column.frame("column").minY)
        let padded = Harness.capture(
            HStack(spacing: 0) {
                Color.red.frame(width: 10, height: 10).probe("leading")
                Color.gray.frame(width: 40, height: 20).probe("inner").padding(-4).probe("padded")
                Color.red.frame(width: 10, height: 10).probe("trailing")
            }.probe("row"),
            scheme: scheme)
        let origin = padded.frame("row")
        store.put("layout_scroll.negative.padded_row_width", nil, origin.width)
        store.put("layout_scroll.negative.padded_row_height", nil, origin.height)
        store.put("layout_scroll.negative.padded_width", nil, padded.frame("padded").width)
        store.put("layout_scroll.negative.padded_height", nil, padded.frame("padded").height)
        store.put("layout_scroll.negative.inner_x", nil, padded.frame("inner").minX - origin.minX)
        store.put("layout_scroll.negative.inner_y", nil, padded.frame("inner").minY - origin.minY)
        store.put("layout_scroll.negative.leading_y", nil, padded.frame("leading").minY - origin.minY)
        store.put("layout_scroll.negative.trailing_x", nil, padded.frame("trailing").minX - origin.minX)
    }

    static func scenePadding(_ store: Store, _ scheme: Scheme) {
        let all = Harness.capture(Color.gray.frame(width: 40, height: 20).probe("inner").scenePadding().probe("outer"), scheme: scheme)
        store.put("layout_scroll.scene_padding.all_width", nil, all.frame("outer").width)
        store.put("layout_scroll.scene_padding.all_height", nil, all.frame("outer").height)
        store.put("layout_scroll.scene_padding.all_inset_x", nil, all.frame("inner").minX - all.frame("outer").minX)
        store.put("layout_scroll.scene_padding.all_inset_y", nil, all.frame("inner").minY - all.frame("outer").minY)
        let sides = Harness.capture(Color.gray.frame(width: 40, height: 20).probe("inner").scenePadding(.minimum, edges: .horizontal).probe("outer"), scheme: scheme)
        store.put("layout_scroll.scene_padding.minimum_horizontal_width", nil, sides.frame("outer").width)
        store.put("layout_scroll.scene_padding.minimum_horizontal_height", nil, sides.frame("outer").height)
    }

    static func cornerInsets(_ store: Store) {
        let insets = EdgeInsets(top: 1, leading: 2, bottom: 3, trailing: 4)
        let corners = RectangleCornerInsets(
            topLeading: CGSize(width: 10, height: 20), topTrailing: CGSize(width: 30, height: 40),
            bottomLeading: CGSize(width: 50, height: 60), bottomTrailing: CGSize(width: 70, height: 80))
        let cases: [(String, Edge.Set)] = [("all", .all), ("top", .top), ("leading", .leading), ("bottom", .bottom), ("trailing", .trailing)]
        for (name, edges) in cases {
            let grown = insets.inset(by: corners, edges: edges)
            store.put("layout_scroll.corner_insets.\(name)", nil, text: "\(Int(grown.top)),\(Int(grown.leading)),\(Int(grown.bottom)),\(Int(grown.trailing))", "insets")
        }
        let empty = RectangleCornerInsets()
        store.put("layout_scroll.corner_insets.default", nil, text: "\(Int(empty.topLeading.width)),\(Int(empty.bottomTrailing.height))", "size")
    }

    static func safeAreaRegions(_ store: Store) {
        let regions: [(String, SafeAreaRegions)] = [("container", .container), ("keyboard", .keyboard), ("all", .all)]
        store.put("layout_scroll.safe_area.raw", nil, text: regions.map { "\($0.1.rawValue)" }.joined(separator: ","), "raw")
        for (name, region) in regions {
            let frames = hosted(ZStack { Color.red.probe("fill").ignoresSafeArea(region, edges: .top) }.safeAreaPadding(.top, 20), width: 200, height: 100)
            let fill = frames["fill"] ?? .zero
            store.put("layout_scroll.safe_area.\(name)_y", nil, fill.minY)
            store.put("layout_scroll.safe_area.\(name)_height", nil, fill.height)
        }
        let alignments: [(String, Alignment?)] = [("none", nil), ("top", .top), ("center", .center), ("bottom", .bottom)]
        for (name, alignment) in alignments {
            let frames = hosted(VStack(spacing: 0) {
                Color.red.frame(width: 20, height: 20).probe("box").ignoresSafeArea(.all, edges: .top, alignment: alignment)
                Spacer()
            }.safeAreaPadding(.top, 40), width: 200, height: 100)
            store.put("layout_scroll.safe_area.aligned_\(name)_y", nil, (frames["box"] ?? .zero).minY)
        }
        var kept = EdgeInsets()
        var ignored = EdgeInsets()
        _ = hosted(GeometryReader { proxy in Color.clear.onAppear { kept = proxy.safeAreaInsets } }
            .safeAreaPadding(EdgeInsets(top: 20, leading: 10, bottom: 0, trailing: 0)), width: 200, height: 100)
        _ = hosted(GeometryReader { proxy in Color.clear.onAppear { ignored = proxy.safeAreaInsets } }
            .ignoresSafeArea()
            .safeAreaPadding(EdgeInsets(top: 20, leading: 10, bottom: 0, trailing: 0)), width: 200, height: 100)
        store.put("layout_scroll.safe_area.proxy_top", nil, kept.top)
        store.put("layout_scroll.safe_area.proxy_leading", nil, kept.leading)
        store.put("layout_scroll.safe_area.proxy_ignored_top", nil, ignored.top)
        store.put("layout_scroll.safe_area.proxy_ignored_leading", nil, ignored.leading)
    }

    static func containerCorners(_ store: Store) {
        let pads: [(String, CGFloat, CGFloat)] = [("4_4", 4, 4), ("4_10", 4, 10), ("10_4", 10, 4), ("30_4", 30, 4)]
        for (name, leading, top) in pads {
            var insets = RectangleCornerInsets()
            var radii: RectangleCornerRadii?
            var within: RectangleCornerRadii?
            _ = hosted(ZStack(alignment: .topLeading) {
                Color.clear
                GeometryReader { proxy in
                    Color.clear.onAppear {
                        insets = proxy.containerCornerInsets
                        radii = proxy.concentricCornerRadii
                        within = proxy.concentricCornerRadii(in: CGRect(x: 0, y: 0, width: 20, height: 20))
                    }
                }.frame(width: 40, height: 40).padding(.leading, leading).padding(.top, top)
            }.containerShape(RoundedRectangle(cornerRadius: 20)), width: 200, height: 100)
            store.put("layout_scroll.container_corners.\(name).top_leading", nil, insets.topLeading.width)
            store.put("layout_scroll.container_corners.\(name).top_trailing", nil, insets.topTrailing.width)
            store.put("layout_scroll.container_corners.\(name).concentric_top_leading", nil, Double(radii?.topLeading ?? -1))
            store.put("layout_scroll.container_corners.\(name).within_top_leading", nil, Double(within?.topLeading ?? -1))
        }
        var capsule = RectangleCornerInsets()
        _ = hosted(ZStack {
            Color.clear
            GeometryReader { proxy in Color.clear.onAppear { capsule = proxy.containerCornerInsets } }.frame(width: 40, height: 20)
        }.frame(width: 60, height: 30).containerShape(Capsule()), width: 200, height: 100)
        store.put("layout_scroll.container_corners.capsule", nil, capsule.topLeading.width)
        var bare: RectangleCornerRadii?
        _ = hosted(GeometryReader { proxy in Color.clear.onAppear { bare = proxy.concentricCornerRadii } }.frame(width: 40, height: 20), width: 200, height: 100)
        store.put("layout_scroll.container_corners.none", nil, text: bare == nil ? "nil" : "\(Int(bare!.topLeading))", "value")
        let fits: [(String, Bool)] = [("offset", false), ("fit", true)]
        for (name, fit) in fits {
            let frames = hosted(ZStack(alignment: .topLeading) {
                Color.clear
                Color.gray.frame(width: 30, height: 30).probe("box").containerCornerOffset(.leading, sizeToFit: fit)
            }.frame(width: 100, height: 100).containerShape(RoundedRectangle(cornerRadius: 20)), width: 200, height: 200)
            store.put("layout_scroll.container_corners.\(name)_x", nil, (frames["box"] ?? .zero).minX)
        }
    }

}

struct ReferenceDelegate<L: SwiftUI.Layout>: SwiftUI.Layout {
    var inner: L

    func makeCache(subviews: Subviews) -> L.Cache { inner.makeCache(subviews: subviews) }

    func sizeThatFits(proposal: ProposedViewSize, subviews: Subviews, cache: inout L.Cache) -> CGSize {
        inner.sizeThatFits(proposal: proposal, subviews: subviews, cache: &cache)
    }

    func placeSubviews(in bounds: CGRect, proposal: ProposedViewSize, subviews: Subviews, cache: inout L.Cache) {
        inner.placeSubviews(in: bounds, proposal: proposal, subviews: subviews, cache: &cache)
    }
}

final class ReferenceLayoutSink {
    static var values: [String: CGFloat] = [:]
}

struct ReferenceBaselineReader: SwiftUI.Layout {
    func sizeThatFits(proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) -> CGSize {
        var height: CGFloat = 0
        var width: CGFloat = 0
        for (index, subview) in subviews.enumerated() {
            let d = subview.dimensions(in: .unspecified)
            ReferenceLayoutSink.values["baseline\(index).first"] = d[.firstTextBaseline]
            ReferenceLayoutSink.values["baseline\(index).last"] = d[.lastTextBaseline]
            ReferenceLayoutSink.values["baseline\(index).height"] = d.height
            height += d.height
            width = max(width, d.width)
        }
        return CGSize(width: width, height: height)
    }

    func placeSubviews(in bounds: CGRect, proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) {
        var y = bounds.minY
        for subview in subviews {
            subview.place(at: CGPoint(x: bounds.minX, y: y), proposal: .unspecified)
            y += subview.sizeThatFits(.unspecified).height
        }
    }
}

struct ReferenceColumn: SwiftUI.Layout {
    var zeroSpacing = false
    var leading: CGFloat? = nil
    var top: CGFloat? = nil

    func spacing(subviews: Subviews, cache: inout ()) -> ViewSpacing {
        if zeroSpacing { return .zero }
        var spacing = ViewSpacing()
        for (index, subview) in subviews.enumerated() {
            if index == 0 { spacing = subview.spacing } else { spacing.formUnion(subview.spacing) }
        }
        return spacing
    }

    func sizeThatFits(proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) -> CGSize {
        let sizes = subviews.map { $0.sizeThatFits(.unspecified) }
        return CGSize(width: sizes.map(\.width).max() ?? 0, height: sizes.reduce(0) { $0 + $1.height })
    }

    func placeSubviews(in bounds: CGRect, proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) {
        var y = bounds.minY
        for subview in subviews {
            subview.place(at: CGPoint(x: bounds.minX, y: y), proposal: .unspecified)
            y += subview.sizeThatFits(.unspecified).height
        }
    }

    func explicitAlignment(of guide: HorizontalAlignment, in bounds: CGRect, proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) -> CGFloat? {
        if guide == .leading, let leading { return bounds.minX + leading }
        return nil
    }

    func explicitAlignment(of guide: VerticalAlignment, in bounds: CGRect, proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) -> CGFloat? {
        if guide == .top, let top { return bounds.minY + top }
        return nil
    }
}

struct ReferenceOrientedHorizontal: SwiftUI.Layout {
    static var layoutProperties: LayoutProperties {
        var properties = LayoutProperties()
        properties.stackOrientation = .horizontal
        return properties
    }

    func sizeThatFits(proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) -> CGSize {
        ReferenceOrientation.record("horizontal", subviews)
        return CGSize(width: 10, height: 10)
    }

    func placeSubviews(in bounds: CGRect, proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) {}
}

struct ReferenceOrientedVertical: SwiftUI.Layout {
    static var layoutProperties: LayoutProperties {
        var properties = LayoutProperties()
        properties.stackOrientation = .vertical
        return properties
    }

    func sizeThatFits(proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) -> CGSize {
        ReferenceOrientation.record("vertical", subviews)
        return CGSize(width: 10, height: 10)
    }

    func placeSubviews(in bounds: CGRect, proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) {}
}

struct ReferenceOrientedNone: SwiftUI.Layout {
    func sizeThatFits(proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) -> CGSize {
        ReferenceOrientation.record("none", subviews)
        return CGSize(width: 10, height: 10)
    }

    func placeSubviews(in bounds: CGRect, proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) {}
}

enum ReferenceOrientation {
    static func record(_ name: String, _ subviews: LayoutSubviews) {
        for (index, subview) in subviews.enumerated() {
            let kind = index == 0 ? "spacer" : "divider"
            for (label, proposal) in [("unspecified", ProposedViewSize.unspecified), ("zero", ProposedViewSize.zero), ("infinity", ProposedViewSize.infinity), ("hundred", ProposedViewSize(width: 100, height: 100))] {
                let size = subview.sizeThatFits(proposal)
                ReferenceLayoutSink.values["\(name).\(kind).\(label)_width"] = size.width
                ReferenceLayoutSink.values["\(name).\(kind).\(label)_height"] = size.height
            }
        }
    }
}

struct ReferencePair: View {
    var body: some View {
        Color.gray.frame(width: 10, height: 10)
        Color.gray.frame(width: 20, height: 10)
    }
}

final class ReferenceSubviewSink {
    static var count = 0
    static var slice = 0
    static var tag: Int? = nil
    static var hasTag = false
    static var untagged = true
}

extension LayoutScroll {
    static func layoutBox(_ width: CGFloat, _ height: CGFloat) -> some View {
        Color.gray.frame(width: width, height: height)
    }

    static func layoutProtocol(_ store: Store, _ scheme: Scheme) {
        builtInLayouts(store, scheme)
        subviewBaselines(store, scheme)
        stackGuides(store, scheme)
        componentSubviews(store, scheme)
        layoutExtras(store, scheme)
        equalPriority(store, scheme)
    }

    static func builtInLayouts(_ store: Store, _ scheme: Scheme) {
        let row = Harness.capture(
            ReferenceDelegate(inner: HStackLayout(alignment: .top, spacing: 4)).callAsFunction {
                layoutBox(30, 20).probe("a")
                layoutBox(40, 10).probe("b")
            }.probe("stack"),
            scheme: scheme)
        let origin = row.frame("stack")
        store.put("layout_scroll.builtin.hstack.b_x", nil, row.frame("b").minX - origin.minX)
        store.put("layout_scroll.builtin.hstack.b_y", nil, row.frame("b").minY - origin.minY)
        store.put("layout_scroll.builtin.hstack.width", nil, origin.width)
        store.put("layout_scroll.builtin.hstack.height", nil, origin.height)
        let spaced = Harness.capture(
            ReferenceDelegate(inner: HStackLayout()).callAsFunction {
                layoutBox(30, 10).probe("a")
                layoutBox(30, 10).probe("b")
            }.probe("stack"),
            scheme: scheme)
        store.put("layout_scroll.builtin.hstack_default.b_x", nil, spaced.frame("b").minX - spaced.frame("stack").minX)
        let flexible = Harness.capture(
            ReferenceDelegate(inner: HStackLayout(spacing: 0)).callAsFunction {
                Color.gray.frame(minWidth: 10, maxWidth: .infinity).frame(height: 10).probe("a")
                Color.gray.frame(maxWidth: 40).frame(height: 10).probe("b")
                layoutBox(50, 10).probe("c")
            }.frame(width: 200).probe("stack"),
            scheme: scheme)
        store.put("layout_scroll.builtin.flexible.a_width", nil, flexible.frame("a").width)
        store.put("layout_scroll.builtin.flexible.b_width", nil, flexible.frame("b").width)
        store.put("layout_scroll.builtin.flexible.c_x", nil, flexible.frame("c").minX - flexible.frame("stack").minX)
        let column = Harness.capture(
            ReferenceDelegate(inner: VStackLayout(alignment: .trailing, spacing: 6)).callAsFunction {
                layoutBox(30, 20).probe("a")
                layoutBox(50, 10).probe("b")
            }.probe("stack"),
            scheme: scheme)
        store.put("layout_scroll.builtin.vstack.a_x", nil, column.frame("a").minX - column.frame("stack").minX)
        store.put("layout_scroll.builtin.vstack.b_y", nil, column.frame("b").minY - column.frame("stack").minY)
        store.put("layout_scroll.builtin.vstack.width", nil, column.frame("stack").width)
        store.put("layout_scroll.builtin.vstack.height", nil, column.frame("stack").height)
        let layered = Harness.capture(
            ReferenceDelegate(inner: ZStackLayout(alignment: .bottomTrailing)).callAsFunction {
                layoutBox(30, 20).probe("a")
                layoutBox(50, 10).probe("b")
            }.probe("stack"),
            scheme: scheme)
        store.put("layout_scroll.builtin.zstack.a_x", nil, layered.frame("a").minX - layered.frame("stack").minX)
        store.put("layout_scroll.builtin.zstack.a_y", nil, layered.frame("a").minY - layered.frame("stack").minY)
        store.put("layout_scroll.builtin.zstack.b_y", nil, layered.frame("b").minY - layered.frame("stack").minY)
        store.put("layout_scroll.builtin.zstack.width", nil, layered.frame("stack").width)
        store.put("layout_scroll.builtin.zstack.height", nil, layered.frame("stack").height)
        let grid = Harness.capture(
            ReferenceDelegate(inner: GridLayout(alignment: .leading, horizontalSpacing: 4, verticalSpacing: 2)).callAsFunction {
                layoutBox(30, 10).probe("a")
                layoutBox(50, 20).probe("b")
            }.probe("stack"),
            scheme: scheme)
        store.put("layout_scroll.builtin.grid.a_x", nil, grid.frame("a").minX - grid.frame("stack").minX)
        store.put("layout_scroll.builtin.grid.b_x", nil, grid.frame("b").minX - grid.frame("stack").minX)
        store.put("layout_scroll.builtin.grid.b_y", nil, grid.frame("b").minY - grid.frame("stack").minY)
        store.put("layout_scroll.builtin.grid.width", nil, grid.frame("stack").width)
        store.put("layout_scroll.builtin.grid.height", nil, grid.frame("stack").height)
        let rows = Harness.capture(
            ReferenceDelegate(inner: GridLayout(alignment: .leading, horizontalSpacing: 4, verticalSpacing: 2)).callAsFunction {
                GridRow {
                    layoutBox(30, 10).probe("a")
                    layoutBox(20, 10).probe("c")
                }
                GridRow {
                    layoutBox(50, 20).probe("b")
                    layoutBox(10, 10).probe("d")
                }
            }.probe("stack"),
            scheme: scheme)
        let rowsOrigin = rows.frame("stack")
        for name in ["a", "b", "c", "d"] {
            store.put("layout_scroll.builtin.grid_rows.\(name)_x", nil, rows.frame(name).minX - rowsOrigin.minX)
            store.put("layout_scroll.builtin.grid_rows.\(name)_y", nil, rows.frame(name).minY - rowsOrigin.minY)
        }
        store.put("layout_scroll.builtin.grid_rows.width", nil, rowsOrigin.width)
        store.put("layout_scroll.builtin.grid_rows.height", nil, rowsOrigin.height)
    }

    static func subviewBaselines(_ store: Store, _ scheme: Scheme) {
        ReferenceLayoutSink.values = [:]
        _ = Harness.capture(
            ReferenceBaselineReader().callAsFunction {
                Text("One\nTwo").font(.system(size: 13))
                layoutBox(30, 20)
                VStack(spacing: 0) {
                    Text("A").font(.system(size: 13))
                    Text("B").font(.system(size: 13))
                }
                Text("One\nTwo").font(.system(size: 13)).padding(5)
                Text("One").font(.system(size: 13))
            },
            scheme: scheme)
        for index in 0..<5 {
            for part in ["first", "last", "height"] {
                store.put("layout_scroll.dimensions.s\(index).\(part)", nil, ReferenceLayoutSink.values["baseline\(index).\(part)"] ?? -1)
            }
        }
    }

    static func stackGuides(_ store: Store, _ scheme: Scheme) {
        let leading = Harness.capture(
            ZStack(alignment: .leading) {
                layoutBox(40, 10).alignmentGuide(.leading) { d in d[.trailing] }.probe("a")
                layoutBox(60, 10).probe("b")
            }.probe("stack"),
            scheme: scheme)
        store.put("layout_scroll.guides.zstack_leading.a_x", nil, leading.frame("a").minX - leading.frame("stack").minX)
        store.put("layout_scroll.guides.zstack_leading.b_x", nil, leading.frame("b").minX - leading.frame("stack").minX)
        store.put("layout_scroll.guides.zstack_leading.width", nil, leading.frame("stack").width)
        let corner = Harness.capture(
            ZStack(alignment: .topLeading) {
                layoutBox(40, 20).alignmentGuide(.top) { _ in 5 }.alignmentGuide(.leading) { _ in 10 }.probe("a")
                layoutBox(60, 30).probe("b")
            }.probe("stack"),
            scheme: scheme)
        let cornerOrigin = corner.frame("stack")
        store.put("layout_scroll.guides.zstack_corner.a_x", nil, corner.frame("a").minX - cornerOrigin.minX)
        store.put("layout_scroll.guides.zstack_corner.a_y", nil, corner.frame("a").minY - cornerOrigin.minY)
        store.put("layout_scroll.guides.zstack_corner.b_x", nil, corner.frame("b").minX - cornerOrigin.minX)
        store.put("layout_scroll.guides.zstack_corner.b_y", nil, corner.frame("b").minY - cornerOrigin.minY)
        store.put("layout_scroll.guides.zstack_corner.width", nil, cornerOrigin.width)
        store.put("layout_scroll.guides.zstack_corner.height", nil, cornerOrigin.height)
        let custom = Harness.capture(
            ZStack(alignment: Alignment(horizontal: .referenceCombined, vertical: .center)) {
                layoutBox(40, 10).alignmentGuide(.referenceCombined) { d in d[.trailing] }.probe("a")
                layoutBox(60, 10).probe("b")
            }.probe("stack"),
            scheme: scheme)
        store.put("layout_scroll.guides.zstack_custom.a_x", nil, custom.frame("a").minX - custom.frame("stack").minX)
        store.put("layout_scroll.guides.zstack_custom.b_x", nil, custom.frame("b").minX - custom.frame("stack").minX)
        store.put("layout_scroll.guides.zstack_custom.width", nil, custom.frame("stack").width)
        let grid = Harness.capture(
            Grid(alignment: .leading, horizontalSpacing: 0, verticalSpacing: 0) {
                GridRow {
                    layoutBox(40, 10).alignmentGuide(.leading) { _ in 10 }.probe("a")
                    layoutBox(20, 10).probe("c")
                }
                GridRow {
                    layoutBox(60, 10).probe("b")
                    layoutBox(20, 10).probe("d")
                }
            }.probe("grid"),
            scheme: scheme)
        let gridOrigin = grid.frame("grid")
        for name in ["a", "b", "c", "d"] {
            store.put("layout_scroll.guides.grid_leading.\(name)_x", nil, grid.frame(name).minX - gridOrigin.minX)
        }
        store.put("layout_scroll.guides.grid_leading.width", nil, gridOrigin.width)
        let gridTop = Harness.capture(
            Grid(alignment: .top, horizontalSpacing: 0, verticalSpacing: 0) {
                GridRow {
                    layoutBox(20, 40).probe("a")
                    layoutBox(20, 20).alignmentGuide(.top) { d in d[.bottom] }.probe("b")
                }
            }.probe("grid"),
            scheme: scheme)
        store.put("layout_scroll.guides.grid_top.a_y", nil, gridTop.frame("a").minY - gridTop.frame("grid").minY)
        store.put("layout_scroll.guides.grid_top.b_y", nil, gridTop.frame("b").minY - gridTop.frame("grid").minY)
        store.put("layout_scroll.guides.grid_top.height", nil, gridTop.frame("grid").height)
        let rtl = Harness.capture(
            VStack(alignment: .leading, spacing: 0) {
                layoutBox(40, 10).alignmentGuide(.leading) { d in d[.trailing] }.probe("a")
                layoutBox(60, 10).probe("b")
                layoutBox(30, 10).alignmentGuide(.leading) { _ in 15 }.probe("c")
            }.environment(\.layoutDirection, .rightToLeft).probe("stack"),
            scheme: scheme)
        for name in ["a", "b", "c"] {
            store.put("layout_scroll.guides.rtl_vstack.\(name)_x", nil, rtl.frame(name).minX - rtl.frame("stack").minX)
        }
        store.put("layout_scroll.guides.rtl_vstack.width", nil, rtl.frame("stack").width)
        let rtlRow = Harness.capture(
            HStack(alignment: .top, spacing: 0) {
                layoutBox(20, 40).probe("a")
                layoutBox(30, 20).alignmentGuide(.top) { d in d[.bottom] }.probe("b")
            }.environment(\.layoutDirection, .rightToLeft).probe("row"),
            scheme: scheme)
        for name in ["a", "b"] {
            store.put("layout_scroll.guides.rtl_hstack.\(name)_x", nil, rtlRow.frame(name).minX - rtlRow.frame("row").minX)
            store.put("layout_scroll.guides.rtl_hstack.\(name)_y", nil, rtlRow.frame(name).minY - rtlRow.frame("row").minY)
        }
        let rtlForm = Harness.capture(
            VStack(alignment: .referenceCombined, spacing: 0) {
                HStack(spacing: 4) {
                    layoutBox(30, 10).probe("l0")
                    layoutBox(50, 10).alignmentGuide(.referenceCombined) { d in d[.leading] }.probe("f0")
                }
                HStack(spacing: 4) {
                    layoutBox(70, 10).probe("l1")
                    layoutBox(50, 10).alignmentGuide(.referenceCombined) { d in d[.leading] }.probe("f1")
                }
            }.environment(\.layoutDirection, .rightToLeft).probe("form"),
            scheme: scheme)
        for name in ["l0", "f0", "l1", "f1"] {
            store.put("layout_scroll.guides.rtl_form.\(name)_x", nil, rtlForm.frame(name).minX - rtlForm.frame("form").minX)
        }
        store.put("layout_scroll.guides.rtl_form.width", nil, rtlForm.frame("form").width)
        let rtlZ = Harness.capture(
            ZStack(alignment: .leading) {
                layoutBox(40, 10).alignmentGuide(.leading) { d in d[.trailing] }.probe("a")
                layoutBox(60, 10).probe("b")
            }.environment(\.layoutDirection, .rightToLeft).probe("stack"),
            scheme: scheme)
        store.put("layout_scroll.guides.rtl_zstack.a_x", nil, rtlZ.frame("a").minX - rtlZ.frame("stack").minX)
        store.put("layout_scroll.guides.rtl_zstack.b_x", nil, rtlZ.frame("b").minX - rtlZ.frame("stack").minX)
        store.put("layout_scroll.guides.rtl_zstack.width", nil, rtlZ.frame("stack").width)
    }

    static func componentSubviews(_ store: Store, _ scheme: Scheme) {
        let shot = Harness.capture(
            Group(subviews: Group {
                ReferencePair()
                layoutBox(30, 10).tag(3)
            }) { subviews in
                let _ = { ReferenceSubviewSink.count = subviews.count }()
                let _ = { ReferenceSubviewSink.slice = subviews[1...].count }()
                let _ = { ReferenceSubviewSink.tag = subviews[2].containerValues.tag(for: Int.self) }()
                let _ = { ReferenceSubviewSink.hasTag = subviews[2].containerValues.hasTag(3) }()
                let _ = { ReferenceSubviewSink.untagged = subviews[0].containerValues.tag(for: Int.self) == nil }()
                HStack(spacing: 0) {
                    ForEach(subviews[1...]) { subview in subview }
                }.probe("row")
            },
            scheme: scheme)
        store.put("layout_scroll.subviews.count", nil, Double(ReferenceSubviewSink.count))
        store.put("layout_scroll.subviews.slice_count", nil, Double(ReferenceSubviewSink.slice))
        store.put("layout_scroll.subviews.slice_width", nil, shot.frame("row").width)
        store.put("layout_scroll.subviews.tag", nil, Double(ReferenceSubviewSink.tag ?? -1))
        store.put("layout_scroll.subviews.has_tag", nil, ReferenceSubviewSink.hasTag ? 1 : 0)
        store.put("layout_scroll.subviews.untagged", nil, ReferenceSubviewSink.untagged ? 1 : 0)
    }

    static func layoutExtras(_ store: Store, _ scheme: Scheme) {
        let zero = Harness.capture(
            HStack {
                ReferenceColumn(zeroSpacing: true).callAsFunction { layoutBox(20, 10) }.probe("custom")
                layoutBox(20, 10).probe("b")
            },
            scheme: scheme)
        store.put("layout_scroll.extras.zero_gap", nil, zero.frame("b").minX - zero.frame("custom").maxX)
        let union = Harness.capture(
            VStack {
                ReferenceColumn().callAsFunction { Text("A") }.probe("custom")
                Text("B").probe("b")
            },
            scheme: scheme)
        store.put("layout_scroll.extras.union_text_gap", nil, union.frame("b").minY - union.frame("custom").maxY)
        let plain = Harness.capture(
            VStack {
                Text("A").probe("a")
                Text("B").probe("b")
            },
            scheme: scheme)
        store.put("layout_scroll.extras.plain_text_gap", nil, plain.frame("b").minY - plain.frame("a").maxY)
        let boxes = Harness.capture(
            VStack {
                ReferenceColumn().callAsFunction { layoutBox(20, 10) }.probe("custom")
                Text("B").probe("b")
            },
            scheme: scheme)
        store.put("layout_scroll.extras.union_box_gap", nil, boxes.frame("b").minY - boxes.frame("custom").maxY)
        let explicitLeading = Harness.capture(
            VStack(alignment: .leading, spacing: 0) {
                ReferenceColumn(leading: 25).callAsFunction { layoutBox(40, 10) }.probe("custom")
                layoutBox(60, 10).probe("b")
            }.probe("stack"),
            scheme: scheme)
        store.put("layout_scroll.extras.explicit_leading.custom_x", nil, explicitLeading.frame("custom").minX - explicitLeading.frame("stack").minX)
        store.put("layout_scroll.extras.explicit_leading.b_x", nil, explicitLeading.frame("b").minX - explicitLeading.frame("stack").minX)
        store.put("layout_scroll.extras.explicit_leading.width", nil, explicitLeading.frame("stack").width)
        let explicitTop = Harness.capture(
            HStack(alignment: .top, spacing: 0) {
                ReferenceColumn(top: 10).callAsFunction { layoutBox(20, 20) }.probe("custom")
                layoutBox(20, 40).probe("b")
            }.probe("row"),
            scheme: scheme)
        store.put("layout_scroll.extras.explicit_top.custom_y", nil, explicitTop.frame("custom").minY - explicitTop.frame("row").minY)
        store.put("layout_scroll.extras.explicit_top.b_y", nil, explicitTop.frame("b").minY - explicitTop.frame("row").minY)
        store.put("layout_scroll.extras.explicit_top.height", nil, explicitTop.frame("row").height)
        let merged = Harness.capture(
            VStack(alignment: .leading, spacing: 0) {
                ReferenceColumn().callAsFunction { layoutBox(40, 10).alignmentGuide(.leading) { _ in 10 } }.probe("custom")
                layoutBox(60, 10).probe("b")
            }.probe("stack"),
            scheme: scheme)
        store.put("layout_scroll.extras.merged.custom_x", nil, merged.frame("custom").minX - merged.frame("stack").minX)
        store.put("layout_scroll.extras.merged.b_x", nil, merged.frame("b").minX - merged.frame("stack").minX)
        ReferenceLayoutSink.values = [:]
        _ = Harness.capture(ReferenceOrientedHorizontal().callAsFunction { Spacer(); Divider() }, scheme: scheme)
        _ = Harness.capture(ReferenceOrientedVertical().callAsFunction { Spacer(); Divider() }, scheme: scheme)
        _ = Harness.capture(ReferenceOrientedNone().callAsFunction { Spacer(); Divider() }, scheme: scheme)
        for (key, value) in ReferenceLayoutSink.values.sorted(by: { $0.key < $1.key }) {
            store.put("layout_scroll.stack_orientation.\(key)", nil, value.isFinite ? value : -1)
        }
    }

    static func equalPriority(_ store: Store, _ scheme: Scheme) {
        let long = "one two three four five six seven eight nine ten eleven twelve"
        let line = Harness.capture(Text("A").font(.system(size: 13)).probe("line"), scheme: scheme)
        store.put("layout_scroll.equal_priority.line_height", nil, line.frame("line").height)
        let shot = Harness.capture(
            VStack(spacing: 0) {
                Text(long).font(.system(size: 13)).probe("a")
                Text(long).font(.system(size: 13)).probe("b")
            }.frame(width: 100, height: 60).probe("frame"),
            scheme: scheme)
        let origin = shot.frame("frame")
        store.put("layout_scroll.equal_priority.a_y", nil, shot.frame("a").minY - origin.minY)
        store.put("layout_scroll.equal_priority.a_height", nil, shot.frame("a").height)
        store.put("layout_scroll.equal_priority.b_y", nil, shot.frame("b").minY - origin.minY)
        store.put("layout_scroll.equal_priority.b_height", nil, shot.frame("b").height)
        let free = Harness.capture(
            VStack(spacing: 0) {
                Text(long).font(.system(size: 13)).probe("a")
            }.frame(width: 100).probe("frame"),
            scheme: scheme)
        store.put("layout_scroll.equal_priority.natural_height", nil, free.frame("a").height)
        let uneven = Harness.capture(
            VStack(spacing: 0) {
                Text("one two").font(.system(size: 13)).probe("a")
                Text(long).font(.system(size: 13)).probe("b")
            }.frame(width: 100, height: 60).probe("frame"),
            scheme: scheme)
        store.put("layout_scroll.equal_priority.short_a_height", nil, uneven.frame("a").height)
        store.put("layout_scroll.equal_priority.short_b_height", nil, uneven.frame("b").height)
        store.put("layout_scroll.equal_priority.short_b_y", nil, uneven.frame("b").minY - uneven.frame("frame").minY)
    }

    static func staticLazyStacks(_ store: Store, _ scheme: Scheme) {
        let column = Harness.capture(
            LazyVStack(alignment: .leading, spacing: 4) {
                Color.gray.frame(width: 40, height: 10).probe("h")
                ForEach(0..<3, id: \.self) { index in
                    Color.gray.frame(width: 60, height: 20).probe("r\(index)")
                }
                Color.gray.frame(width: 30, height: 10).probe("f")
            }.probe("stack").frame(width: 200),
            scheme: scheme)
        let stack = column.frame("stack")
        store.put("layout_scroll.lazy_static.v.width", nil, stack.width)
        store.put("layout_scroll.lazy_static.v.height", nil, stack.height)
        for name in ["h", "r0", "r1", "r2", "f"] {
            store.put("layout_scroll.lazy_static.v.\(name).x", nil, column.frame(name).minX - stack.minX)
            store.put("layout_scroll.lazy_static.v.\(name).y", nil, column.frame(name).minY - stack.minY)
        }
        let row = Harness.capture(
            LazyHStack(alignment: .top, spacing: 4) {
                Color.gray.frame(width: 10, height: 40).probe("h")
                ForEach(0..<2, id: \.self) { index in
                    Color.gray.frame(width: 20, height: 60).probe("r\(index)")
                }
            }.probe("stack").frame(height: 100),
            scheme: scheme)
        let across = row.frame("stack")
        store.put("layout_scroll.lazy_static.h.width", nil, across.width)
        store.put("layout_scroll.lazy_static.h.height", nil, across.height)
        for name in ["h", "r0", "r1"] {
            store.put("layout_scroll.lazy_static.h.\(name).x", nil, row.frame(name).minX - across.minX)
            store.put("layout_scroll.lazy_static.h.\(name).y", nil, row.frame(name).minY - across.minY)
        }
    }

    static func painted(_ shot: Shot, _ x: CGFloat, _ y: CGFloat) -> Double {
        let colour = shot.raster.flat(shot.raster.px(x), shot.raster.px(y))
        if colour.r > 0.8 && colour.g < 0.3 && colour.b < 0.3 { return 1 }
        if colour.b > 0.8 && colour.r < 0.3 && colour.g < 0.6 { return 2 }
        return 0
    }

    static func gridRowPaint(_ store: Store, _ scheme: Scheme) {
        let grid = Grid(alignment: .topLeading, horizontalSpacing: 10, verticalSpacing: 10) {
            GridRow {
                Color.clear.frame(width: 30, height: 20).probe("a")
                Color.clear.frame(width: 60, height: 40).probe("b")
            }.background(Color.red)
            GridRow {
                Color.clear.frame(width: 50, height: 20).probe("c")
                Color.clear.frame(width: 20, height: 20).probe("d")
            }.border(Color.blue, width: 2)
        }.probe("g")
        let shot = Harness.capture(grid, scheme: scheme)
        let origin = shot.frame("g")
        for name in ["a", "b", "c", "d"] {
            let cell = shot.frame(name)
            store.put("layout_scroll.grid_row_paint.\(name).x", nil, cell.minX - origin.minX)
            store.put("layout_scroll.grid_row_paint.\(name).y", nil, cell.minY - origin.minY)
        }
        let at = { (x: CGFloat, y: CGFloat) in painted(shot, origin.minX + x, origin.minY + y) }
        store.put("layout_scroll.grid_row_paint.inside_a", nil, at(5, 5), "")
        store.put("layout_scroll.grid_row_paint.below_a", nil, at(5, 30), "")
        store.put("layout_scroll.grid_row_paint.right_of_a", nil, at(40, 5), "")
        store.put("layout_scroll.grid_row_paint.gap", nil, at(55, 5), "")
        store.put("layout_scroll.grid_row_paint.inside_b", nil, at(80, 30), "")
        store.put("layout_scroll.grid_row_paint.border_c_left", nil, at(1, 55), "")
        store.put("layout_scroll.grid_row_paint.inside_c", nil, at(20, 60), "")
        store.put("layout_scroll.grid_row_paint.gap_cd", nil, at(55, 55), "")
        store.put("layout_scroll.grid_row_paint.border_d_left", nil, at(61, 55), "")
        store.put("layout_scroll.grid_row_paint.right_of_d", nil, at(95, 55), "")
        store.image("layout_scroll_grid_row_paint", shot, origin)
    }

    static func gridInFrame(_ store: Store, _ scheme: Scheme) {
        let cases: [(String, Alignment, LayoutDirection)] = [
            ("leading", .leading, .leftToRight),
            ("center", .center, .leftToRight),
            ("trailing", .trailing, .leftToRight),
            ("leading_rtl", .leading, .rightToLeft),
        ]
        for (name, alignment, direction) in cases {
            let shot = Harness.capture(
                Grid(horizontalSpacing: 10) {
                    GridRow {
                        Color.gray.frame(width: 40, height: 10).probe("a")
                        Color.gray.frame(width: 60, height: 10).probe("b")
                    }
                }.probe("g").frame(width: 300, height: 60, alignment: alignment).probe("frame")
                    .environment(\.layoutDirection, direction),
                scheme: scheme)
            let frame = shot.frame("frame")
            store.put("layout_scroll.grid_frame.\(name).grid_x", nil, shot.frame("g").minX - frame.minX)
            store.put("layout_scroll.grid_frame.\(name).grid_y", nil, shot.frame("g").minY - frame.minY)
            store.put("layout_scroll.grid_frame.\(name).grid_width", nil, shot.frame("g").width)
            store.put("layout_scroll.grid_frame.\(name).a_x", nil, shot.frame("a").minX - frame.minX)
            store.put("layout_scroll.grid_frame.\(name).b_x", nil, shot.frame("b").minX - frame.minX)
        }
    }

    static func radioGroupLayout(_ store: Store, _ scheme: Scheme) {
        let vertical = Harness.capture(
            Picker("Size", selection: .constant(0)) {
                Text("Small").tag(0)
                Text("Medium").tag(1)
                Text("Large").tag(2)
            }.pickerStyle(.radioGroup).labelsHidden().probe("picker"),
            scheme: scheme)
        store.put("layout_scroll.radio_group.vertical.width", nil, vertical.frame("picker").width)
        store.put("layout_scroll.radio_group.vertical.height", nil, vertical.frame("picker").height)
        let horizontal = Harness.capture(
            Picker("Size", selection: .constant(0)) {
                Text("Small").tag(0)
                Text("Medium").tag(1)
                Text("Large").tag(2)
            }.pickerStyle(.radioGroup).horizontalRadioGroupLayout().labelsHidden().probe("picker"),
            scheme: scheme)
        store.put("layout_scroll.radio_group.horizontal.width", nil, horizontal.frame("picker").width)
        store.put("layout_scroll.radio_group.horizontal.height", nil, horizontal.frame("picker").height)
        store.image("layout_scroll_radio_vertical", vertical, vertical.frame("picker"))
        store.image("layout_scroll_radio_horizontal", horizontal, horizontal.frame("picker"))
        let single = Harness.capture(
            Picker("Size", selection: .constant(0)) {
                Text("Small").tag(0)
            }.pickerStyle(.radioGroup).labelsHidden().probe("picker"),
            scheme: scheme)
        store.put("layout_scroll.radio_group.single.width", nil, single.frame("picker").width)
        store.put("layout_scroll.radio_group.single.height", nil, single.frame("picker").height)
        let pair = Harness.capture(
            Picker("Size", selection: .constant(0)) {
                Text("Small").tag(0)
                Text("Small").tag(1)
            }.pickerStyle(.radioGroup).horizontalRadioGroupLayout().labelsHidden().probe("picker"),
            scheme: scheme)
        store.put("layout_scroll.radio_group.pair_horizontal.width", nil, pair.frame("picker").width)
        let text = Harness.capture(Text("Small").probe("t"), scheme: scheme)
        store.put("layout_scroll.radio_group.small_text_width", nil, text.frame("t").width)
    }

    @available(macOS 27.1, *)
    static func arrangementCase(_ store: Store, _ scheme: Scheme, _ name: String, width: CGFloat, height: CGFloat, _ view: some View) {
        ReferenceArrangementSink.seen = [:]
        let shot = Harness.capture(view.frame(width: width, height: height).probe("box"), scheme: scheme)
        let box = shot.frame("box")
        for part in ["primary", "secondary"] {
            let frame = shot.frame(part)
            let prefix = "layout_scroll.arrangement.\(name).\(part)"
            store.put("\(prefix).x", nil, frame.minX - box.minX)
            store.put("\(prefix).y", nil, frame.minY - box.minY)
            store.put("\(prefix).width", nil, frame.width)
            store.put("\(prefix).height", nil, frame.height)
            store.put("\(prefix).axis", nil, text: ReferenceArrangementSink.seen["\(part).axis"] ?? "unread", "axis")
            store.put("\(prefix).z", nil, text: ReferenceArrangementSink.seen["\(part).z"] ?? "unread", "z")
        }
    }

    @available(macOS 27.1, *)
    static func arrangement(_ store: Store, _ scheme: Scheme) {
        let plain = { (style: any ArrangementViewStyle) in
            AnyView(ArrangementView {
                ReferenceArrangementPart(name: "primary")
            } secondary: {
                ReferenceArrangementPart(name: "secondary")
            }.arrangementViewStyle(style))
        }
        arrangementCase(store, scheme, "split_wide", width: 400, height: 200, plain(.split))
        arrangementCase(store, scheme, "split_tall", width: 200, height: 400, plain(.split))
        arrangementCase(store, scheme, "split_square", width: 300, height: 300, plain(.split))
        arrangementCase(store, scheme, "split_horizontal_tall", width: 200, height: 400, plain(SplitArrangementViewStyle().axes(.horizontal)))
        arrangementCase(store, scheme, "split_vertical_wide", width: 400, height: 200, plain(SplitArrangementViewStyle().axes(.vertical)))
        arrangementCase(store, scheme, "overlay_wide", width: 400, height: 200, plain(.overlay))
        arrangementCase(store, scheme, "overlay_tall", width: 200, height: 400, plain(.overlay))
        arrangementCase(store, scheme, "overlay_horizontal_tall", width: 200, height: 400, plain(OverlayArrangementViewStyle().axes(.horizontal)))
        arrangementCase(store, scheme, "overlay_vertical_wide", width: 400, height: 200, plain(OverlayArrangementViewStyle().axes(.vertical)))
        arrangementCase(store, scheme, "automatic_wide", width: 400, height: 200, plain(.automatic))
        arrangementCase(store, scheme, "automatic_tall", width: 200, height: 400, plain(.automatic))
        let sized = { (name: String, primary: AnyView, secondary: AnyView, style: any ArrangementViewStyle, width: CGFloat, height: CGFloat) in
            arrangementCase(store, scheme, name, width: width, height: height,
                            AnyView(ArrangementView { primary } secondary: { secondary }.arrangementViewStyle(style)))
        }
        let part = { (name: String) in ReferenceArrangementPart(name: name) }
        sized("ratio_secondary", AnyView(part("primary")), AnyView(part("secondary").splitArrangementLayoutRatio(0.25)), .split, 400, 200)
        sized("ratio_primary", AnyView(part("primary").splitArrangementLayoutRatio(0.25)), AnyView(part("secondary")), .split, 400, 200)
        sized("ratio_both", AnyView(part("primary").splitArrangementLayoutRatio(0.25)), AnyView(part("secondary").splitArrangementLayoutRatio(0.25)), .split, 400, 200)
        sized("ratio_tall", AnyView(part("primary")), AnyView(part("secondary").splitArrangementLayoutRatio(0.25)), .split, 200, 400)
        sized("ratio_min", AnyView(part("primary")), AnyView(part("secondary").splitArrangementLayoutRatio(minHorizontal: 0.7)), .split, 400, 200)
        sized("ratio_max", AnyView(part("primary")), AnyView(part("secondary").splitArrangementLayoutRatio(maxHorizontal: 0.2)), .split, 400, 200)
        sized("ratio_ideal_vertical", AnyView(part("primary")), AnyView(part("secondary").splitArrangementLayoutRatio(idealHorizontal: 0.3, idealVertical: 0.1)), .split, 200, 400)
        sized("size_ideal", AnyView(part("primary")), AnyView(part("secondary").splitArrangementLayoutSize(idealWidth: 120)), .split, 400, 200)
        sized("size_min", AnyView(part("primary")), AnyView(part("secondary").splitArrangementLayoutSize(minWidth: 300)), .split, 400, 200)
        sized("size_max", AnyView(part("primary")), AnyView(part("secondary").splitArrangementLayoutSize(maxWidth: 50)), .split, 400, 200)
        sized("size_primary_min", AnyView(part("primary").splitArrangementLayoutSize(minWidth: 300)), AnyView(part("secondary")), .split, 400, 200)
        sized("size_height", AnyView(part("primary")), AnyView(part("secondary").splitArrangementLayoutSize(idealHeight: 90)), .split, 200, 400)
        sized("fixed_text", AnyView(part("primary")), AnyView(ReferenceArrangementText(name: "secondary").splitArrangementFixedLayoutSize()), .split, 400, 200)
        sized("unfixed_text", AnyView(part("primary")), AnyView(ReferenceArrangementText(name: "secondary")), .split, 400, 200)
        sized("overlay_text", AnyView(part("primary")), AnyView(ReferenceArrangementText(name: "secondary")), .overlay, 400, 200)
        sized("overlay_text_tall", AnyView(part("primary")), AnyView(ReferenceArrangementText(name: "secondary")), .overlay, 200, 400)
        sized("overlay_top", AnyView(part("primary")), AnyView(ReferenceArrangementText(name: "secondary").overlayArrangementEdge(VerticalEdge.top)), .overlay, 200, 400)
        sized("overlay_leading", AnyView(part("primary")), AnyView(ReferenceArrangementText(name: "secondary").overlayArrangementEdge(HorizontalEdge.leading)), .overlay, 400, 200)
        sized("overlay_top_wide", AnyView(part("primary")), AnyView(ReferenceArrangementText(name: "secondary").overlayArrangementEdge(VerticalEdge.top)), .overlay, 400, 200)
        sized("overlay_ratio", AnyView(part("primary")), AnyView(part("secondary").splitArrangementLayoutRatio(0.25)), .overlay, 400, 200)
        ReferenceArrangementSink.seen = [:]
        _ = Harness.capture(ReferenceArrangementPart(name: "outside").frame(width: 40, height: 40), scheme: scheme)
        store.put("layout_scroll.arrangement.outside.axis", nil, text: ReferenceArrangementSink.seen["outside.axis"] ?? "unread", "axis")
        store.put("layout_scroll.arrangement.outside.z", nil, text: ReferenceArrangementSink.seen["outside.z"] ?? "unread", "z")
    }
}

final class ReferenceArrangementSink {
    static var seen: [String: String] = [:]
}

@available(macOS 27.1, *)
struct ReferenceArrangementPart: View {
    let name: String
    @Environment(\.splitArrangementAxis) var axis
    @Environment(\.overlayArrangementZIndex) var z

    var body: some View {
        let _ = ReferenceArrangementSink.seen["\(name).axis"] = axis.map { $0 == .horizontal ? "horizontal" : "vertical" } ?? "nil"
        let _ = ReferenceArrangementSink.seen["\(name).z"] = "\(z)"
        Color.gray.probe(name)
    }
}

@available(macOS 27.1, *)
struct ReferenceArrangementText: View {
    let name: String
    @Environment(\.splitArrangementAxis) var axis
    @Environment(\.overlayArrangementZIndex) var z

    var body: some View {
        let _ = ReferenceArrangementSink.seen["\(name).axis"] = axis.map { $0 == .horizontal ? "horizontal" : "vertical" } ?? "nil"
        let _ = ReferenceArrangementSink.seen["\(name).z"] = "\(z)"
        Text("Secondary").probe(name)
    }
}

struct ReferenceIdealProbe: SwiftUI.Layout {
    func sizeThatFits(proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) -> CGSize {
        for (index, subview) in subviews.enumerated() {
            let ideal = subview.sizeThatFits(.unspecified)
            ReferenceLayoutSink.values["s\(index).ideal_width"] = ideal.width
            ReferenceLayoutSink.values["s\(index).ideal_height"] = ideal.height
        }
        return CGSize(width: 10, height: 10)
    }

    func placeSubviews(in bounds: CGRect, proposal: ProposedViewSize, subviews: Subviews, cache: inout ()) {}
}

extension LayoutScroll {
    static func listBackground(_ store: Store, _ scheme: Scheme) {
        let styles: [(String, AnyView)] = [
            ("automatic", AnyView(List { Text("One"); Text("Two") }.listStyle(.automatic).frame(width: 300, height: 160).probe("list"))),
            ("plain", AnyView(List { Text("One"); Text("Two") }.listStyle(.plain).frame(width: 300, height: 160).probe("list"))),
            ("inset", AnyView(List { Text("One"); Text("Two") }.listStyle(.inset).frame(width: 300, height: 160).probe("list"))),
            ("sidebar", AnyView(List { Text("One"); Text("Two") }.listStyle(.sidebar).frame(width: 300, height: 160).probe("list"))),
        ]
        for (name, view) in styles {
            let shot = Harness.capture(view, scheme: scheme, settle: 0.3)
            let box = shot.frame("list")
            let point = CGPoint(x: box.midX, y: box.maxY - 10)
            store.colour("layout_scroll.list_background.\(name).fill_alpha", scheme, shot.raster.sample(point))
        }
    }

    static func idealQuery(_ store: Store, _ scheme: Scheme) {
        ReferenceLayoutSink.values = [:]
        _ = Harness.capture(
            ReferenceIdealProbe().callAsFunction {
                ZStack {}.frame(idealWidth: 80, maxWidth: .infinity)
                ZStack {}.frame(idealHeight: 30, maxHeight: .infinity)
                Color.gray.frame(idealWidth: 50, idealHeight: 20)
                Color.gray.frame(minWidth: 10, idealWidth: 60, maxWidth: 200, minHeight: 5, idealHeight: 25, maxHeight: 100)
            },
            scheme: scheme)
        for (key, value) in ReferenceLayoutSink.values.sorted(by: { $0.key < $1.key }) {
            store.put("layout_scroll.ideal_query.\(key)", nil, value.isFinite ? value : -1)
        }
    }
}
