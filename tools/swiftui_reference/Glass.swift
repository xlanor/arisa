import AppKit
import SwiftUI

enum Glass {
    static func run(_ store: Store) {
        for scheme in Scheme.allCases {
            for (size, control) in Controls.sizes {
                let plain = Button {} label: { Text("Button").probe("label") }.buttonStyle(.glass).controlSize(control).probe("control")
                Measure.record(store, "button.glass.\(size)", Harness.capture(plain, scheme: scheme))
                let prominent = Button {} label: { Text("Button").probe("label") }.buttonStyle(.glassProminent).controlSize(control).probe("control")
                Measure.record(store, "button.glass_prominent.\(size)", Harness.capture(prominent, scheme: scheme))
            }
            let capsule = Text("Glass").probe("label").padding().glassEffect(.regular, in: .capsule).probe("control")
            Measure.record(store, "glass.regular.capsule", Harness.capture(capsule, scheme: scheme), text: "Glass")
            let clear = Text("Glass").probe("label").padding().glassEffect(.clear, in: .capsule).probe("control")
            Measure.record(store, "glass.clear.capsule", Harness.capture(clear, scheme: scheme), text: "Glass")
            let tinted = Text("Glass").probe("label").padding().glassEffect(.regular.tint(.blue), in: .capsule).probe("control")
            Measure.record(store, "glass.tinted.capsule", Harness.capture(tinted, scheme: scheme), text: "Glass")
            let container = GlassEffectContainer(spacing: 20) {
                HStack(spacing: 8) {
                    Text("A").padding().glassEffect().probe("first")
                    Text("B").padding().glassEffect().probe("second")
                }
            }.probe("control")
            let shot = Harness.capture(container, scheme: scheme)
            store.put("glass.container.default_text_padding", scheme, shot.frame("first").width - 0)
            store.put("glass.container.first_height", scheme, shot.frame("first").height)
            store.put("glass.container.gap", scheme, shot.frame("second").minX - shot.frame("first").maxX)
        }
    }
}
