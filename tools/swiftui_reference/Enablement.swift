import AppKit
import SwiftUI

struct IgnoringButtonStyle: ButtonStyle {
    func makeBody(configuration: Configuration) -> some View {
        configuration.label.padding(6).background(Color.red)
    }
}

struct ReadingButtonStyle: ButtonStyle {
    @Environment(\.isEnabled) var isEnabled
    func makeBody(configuration: Configuration) -> some View {
        configuration.label.padding(6).background(Color.red).opacity(isEnabled ? 1 : 0.5)
    }
}

struct IgnoringToggleStyle: ToggleStyle {
    func makeBody(configuration: Configuration) -> some View {
        HStack { configuration.label; Rectangle().fill(Color.red).frame(width: 20, height: 12) }
    }
}

enum Enablement {
    static func run(_ store: Store) {
        for scheme in Scheme.allCases {
            let cases: [(String, (Bool) -> AnyView)] = [
                ("text", { AnyView(Text("Label").disabled($0)) }),
                ("rectangle", { AnyView(Rectangle().fill(Color.red).frame(width: 40, height: 20).disabled($0)) }),
                ("image", { AnyView(Image(systemName: "star.fill").foregroundStyle(Color.red).disabled($0)) }),
                ("stack_of_text", { AnyView(VStack { Text("Label") }.disabled($0)) }),
                ("button_automatic", { AnyView(Button("Label") {}.disabled($0)) }),
                ("button_plain", { AnyView(Button("Label") {}.buttonStyle(.plain).disabled($0)) }),
                ("button_borderless", { AnyView(Button("Label") {}.buttonStyle(.borderless).disabled($0)) }),
                ("button_custom_ignoring", { AnyView(Button("Label") {}.buttonStyle(IgnoringButtonStyle()).disabled($0)) }),
                ("button_custom_reading", { AnyView(Button("Label") {}.buttonStyle(ReadingButtonStyle()).disabled($0)) }),
                ("button_custom_label_image", { AnyView(Button {} label: { Image(systemName: "star.fill").foregroundStyle(Color.red) }.buttonStyle(IgnoringButtonStyle()).disabled($0)) }),
                ("toggle_custom_ignoring", { AnyView(Toggle("Label", isOn: .constant(true)).toggleStyle(IgnoringToggleStyle()).disabled($0)) }),
                ("label_in_plain_button", { AnyView(Button {} label: { Label("Label", systemImage: "star") }.buttonStyle(.plain).disabled($0)) }),
            ]
            for (name, make) in cases {
                let on = Harness.capture(make(false).probe("probe"), scheme: scheme)
                let off = Harness.capture(make(true).probe("probe"), scheme: scheme)
                let box = on.frame("probe").insetBy(dx: 0.5, dy: 0.5)
                let background = on.raster.background
                let enabled = on.raster.extreme(in: box, from: background)
                let disabled = off.raster.extreme(in: off.frame("probe").insetBy(dx: 0.5, dy: 0.5), from: background)
                let key = "disabled.\(name)"
                store.colour("\(key).enabled_ink", scheme, enabled)
                store.colour("\(key).disabled_ink", scheme, disabled)
                let reach = enabled.distance(background)
                store.put("\(key).ink_ratio", scheme, reach > 0 ? disabled.distance(background) / reach : 1, "")
            }
        }
    }
}
