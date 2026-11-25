import UIKit

let variants: [(String, UITraitCollection)] = [
    ("light", UITraitCollection { $0.userInterfaceStyle = .light; $0.userInterfaceLevel = .base; $0.accessibilityContrast = .normal }),
    ("dark", UITraitCollection { $0.userInterfaceStyle = .dark; $0.userInterfaceLevel = .base; $0.accessibilityContrast = .normal }),
    ("dark_elevated", UITraitCollection { $0.userInterfaceStyle = .dark; $0.userInterfaceLevel = .elevated; $0.accessibilityContrast = .normal }),
    ("hc_light", UITraitCollection { $0.userInterfaceStyle = .light; $0.userInterfaceLevel = .base; $0.accessibilityContrast = .high }),
    ("hc_dark", UITraitCollection { $0.userInterfaceStyle = .dark; $0.userInterfaceLevel = .base; $0.accessibilityContrast = .high }),
    ("hc_dark_elevated", UITraitCollection { $0.userInterfaceStyle = .dark; $0.userInterfaceLevel = .elevated; $0.accessibilityContrast = .high }),
]

let colours: [(String, UIColor)] = [
    ("label", .label), ("secondary_label", .secondaryLabel), ("tertiary_label", .tertiaryLabel),
    ("quaternary_label", .quaternaryLabel),
    ("system_fill", .systemFill), ("secondary_system_fill", .secondarySystemFill),
    ("tertiary_system_fill", .tertiarySystemFill), ("quaternary_system_fill", .quaternarySystemFill),
    ("placeholder_text", .placeholderText), ("tint", .tintColor),
    ("system_background", .systemBackground), ("secondary_system_background", .secondarySystemBackground),
    ("tertiary_system_background", .tertiarySystemBackground),
    ("system_grouped_background", .systemGroupedBackground),
    ("secondary_system_grouped_background", .secondarySystemGroupedBackground),
    ("tertiary_system_grouped_background", .tertiarySystemGroupedBackground),
    ("separator", .separator), ("opaque_separator", .opaqueSeparator), ("link", .link),
    ("dark_text", .darkText), ("light_text", .lightText),
    ("red", .systemRed), ("orange", .systemOrange), ("yellow", .systemYellow),
    ("green", .systemGreen), ("mint", .systemMint), ("teal", .systemTeal),
    ("cyan", .systemCyan), ("blue", .systemBlue), ("indigo", .systemIndigo),
    ("purple", .systemPurple), ("pink", .systemPink), ("brown", .systemBrown),
    ("gray", .systemGray), ("gray2", .systemGray2), ("gray3", .systemGray3),
    ("gray4", .systemGray4), ("gray5", .systemGray5), ("gray6", .systemGray6),
]

func hex(_ c: UIColor) -> String {
    var r: CGFloat = 0, g: CGFloat = 0, b: CGFloat = 0, a: CGFloat = 0
    guard c.getRed(&r, green: &g, blue: &b, alpha: &a) else { return "?" }
    let clamp = { (v: CGFloat) in max(0, min(255, Int((v * 255).rounded()))) }
    let h = String(format: "#%02X%02X%02X", clamp(r), clamp(g), clamp(b))
    let p = Int((a * 100).rounded())
    return p == 100 ? h : "\(h)@\(p)%"
}

print((["role"] + variants.map { $0.0 }).joined(separator: "\t"))
for (name, colour) in colours {
    print(([name] + variants.map { hex(colour.resolvedColor(with: $0.1)) }).joined(separator: "\t"))
}
print("os\t\(UIDevice.current.systemName) \(UIDevice.current.systemVersion)")
