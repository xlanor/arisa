import AppKit
import SwiftUI

struct LastValue: PreferenceKey {
    static let defaultValue = 0
    static func reduce(value: inout Int, nextValue: () -> Int) { value = nextValue() }
}

struct Sequence: PreferenceKey {
    static let defaultValue: [Int] = []
    static func reduce(value: inout [Int], nextValue: () -> [Int]) { value.append(contentsOf: nextValue()) }
}

struct Bounds: PreferenceKey {
    static let defaultValue: Anchor<CGRect>? = nil
    static func reduce(value: inout Anchor<CGRect>?, nextValue: () -> Anchor<CGRect>?) { value = value ?? nextValue() }
}

struct Counter: EnvironmentKey {
    static let defaultValue = 0
}

extension EnvironmentValues {
    var counter: Int {
        get { self[Counter.self] }
        set { self[Counter.self] = newValue }
    }
}

final class Seen {
    var values: [String] = []
    var rect: CGRect = .zero
    var scale: CGFloat = 0
    var pixel: CGFloat = 0
    var control = ""
    var focused = false
    var counter = 0
}

struct EnvironmentProbe: View {
    let seen: Seen
    @Environment(\.displayScale) var displayScale
    @Environment(\.pixelLength) var pixelLength
    @Environment(\.controlSize) var controlSize
    @Environment(\.isFocused) var isFocused
    @Environment(\.counter) var counter

    var body: some View {
        seen.scale = displayScale
        seen.pixel = pixelLength
        seen.control = "\(controlSize)"
        seen.focused = isFocused
        seen.counter = counter
        return Color.clear.frame(width: 10, height: 10)
    }
}

enum DataFlow {
    static func run(_ store: Store) {
        let scheme = Scheme.light
        preferences(store, scheme)
        environment(store, scheme)
        anchors(store, scheme)
        link(store)
    }

    static func observe<V: View, K: PreferenceKey>(_ view: V, _ key: K.Type, scheme: Scheme) -> Seen where K.Value: Equatable {
        let seen = Seen()
        _ = Harness.capture(view.onPreferenceChange(key) { seen.values.append("\($0)") }, scheme: scheme)
        return seen
    }

    static func preferences(_ store: Store, _ scheme: Scheme) {
        let outer = observe(
            VStack { Color.clear.frame(width: 10, height: 10).preference(key: LastValue.self, value: 1) }
                .preference(key: LastValue.self, value: 2),
            LastValue.self, scheme: scheme)
        store.put("preference.outer_over_inner", nil, text: outer.values.last ?? "none", "value")
        let siblings = observe(
            VStack {
                Color.clear.frame(width: 10, height: 10).preference(key: Sequence.self, value: [1])
                Color.clear.frame(width: 10, height: 10).preference(key: Sequence.self, value: [2])
            },
            Sequence.self, scheme: scheme)
        store.put("preference.sibling_order", nil, text: siblings.values.last ?? "none", "value")
        let silent = observe(
            VStack {
                Color.clear.frame(width: 10, height: 10).preference(key: LastValue.self, value: 5)
                Color.clear.frame(width: 10, height: 10)
            },
            LastValue.self, scheme: scheme)
        store.put("preference.silent_sibling", nil, text: silent.values.last ?? "none", "value")
        let initial = observe(
            Color.clear.frame(width: 10, height: 10).preference(key: LastValue.self, value: 3),
            LastValue.self, scheme: scheme)
        store.put("preference.initial_calls", nil, Double(initial.values.count), "count")
        store.put("preference.initial_value", nil, text: initial.values.first ?? "none", "value")
        let unset = observe(Color.clear.frame(width: 10, height: 10), LastValue.self, scheme: scheme)
        store.put("preference.unset_calls", nil, Double(unset.values.count), "count")
        let transformed = observe(
            Color.clear.frame(width: 10, height: 10).preference(key: LastValue.self, value: 1)
                .transformPreference(LastValue.self) { $0 += 10 },
            LastValue.self, scheme: scheme)
        store.put("preference.transform", nil, text: transformed.values.last ?? "none", "value")
        let bare = observe(
            Color.clear.frame(width: 10, height: 10).transformPreference(LastValue.self) { $0 += 10 },
            LastValue.self, scheme: scheme)
        store.put("preference.transform_without_value", nil, text: bare.values.last ?? "none", "value")
    }

    static func environment(_ store: Store, _ scheme: Scheme) {
        let transformed = Seen()
        _ = Harness.capture(
            EnvironmentProbe(seen: transformed)
                .transformEnvironment(\.counter) { $0 += 1 }
                .environment(\.counter, 5),
            scheme: scheme)
        store.put("environment.transform_after_outer", nil, Double(transformed.counter), "value")
        let inner = Seen()
        _ = Harness.capture(
            EnvironmentProbe(seen: inner)
                .environment(\.counter, 5)
                .transformEnvironment(\.counter) { $0 += 1 },
            scheme: scheme)
        store.put("environment.transform_before_inner", nil, Double(inner.counter), "value")
        let values = Seen()
        _ = Harness.capture(EnvironmentProbe(seen: values), scheme: scheme)
        store.put("environment.display_scale", nil, Double(values.scale), "x")
        store.put("environment.pixel_length", nil, Double(values.pixel), "pt")
        store.put("environment.control_size", nil, text: values.control, "value")
        store.put("environment.is_focused", nil, text: values.focused ? "true" : "false", "value")
    }

    static func anchors(_ store: Store, _ scheme: Scheme) {
        let seen = Seen()
        let shot = Harness.capture(
            VStack(alignment: .leading, spacing: 10) {
                Color.gray.frame(width: 30, height: 30)
                Color.gray.frame(width: 40, height: 20)
                    .anchorPreference(key: Bounds.self, value: .bounds) { $0 }
                    .probe("target")
            }
            .padding(7)
            .overlayPreferenceValue(Bounds.self) { anchor in
                GeometryReader { proxy in
                    let rect = anchor.map { proxy[$0] } ?? .zero
                    let _ = { seen.rect = rect }()
                    Color.red.frame(width: rect.width, height: rect.height)
                        .offset(x: rect.minX, y: rect.minY)
                }
            }
            .probe("stack"),
            scheme: scheme)
        store.put("anchor.bounds_x", nil, Double(seen.rect.minX))
        store.put("anchor.bounds_y", nil, Double(seen.rect.minY))
        store.put("anchor.bounds_width", nil, Double(seen.rect.width))
        store.put("anchor.bounds_height", nil, Double(seen.rect.height))
        store.put("anchor.target_x", nil, Double(shot.frame("target").minX - shot.frame("stack").minX))
        store.put("anchor.target_y", nil, Double(shot.frame("target").minY - shot.frame("stack").minY))
    }

    static func link(_ store: Store) {
        for scheme in Scheme.allCases {
            let link = Harness.capture(
                Link(destination: URL(string: "https://example.com")!) { Text("Learn more").probe("label") }
                    .probe("control"),
                scheme: scheme)
            let box = link.frame("control")
            store.put("link.width", scheme, Double(box.width))
            store.put("link.height", scheme, Double(box.height))
            store.colour("link.ink", scheme, link.raster.extreme(in: box, from: link.raster.background))
            store.image("link", link, box)
            let borderless = Harness.capture(
                Button {} label: { Text("Learn more").probe("label") }
                    .buttonStyle(.borderless)
                    .probe("control"),
                scheme: scheme)
            let other = borderless.frame("control")
            store.put("link.borderless_width", scheme, Double(other.width))
            store.put("link.borderless_height", scheme, Double(other.height))
            store.colour("link.borderless_ink", scheme, borderless.raster.extreme(in: other, from: borderless.raster.background))
        }
    }
}
