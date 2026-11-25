import UIKit

let styles: [(String, UIFont.TextStyle)] = [
    ("large_title", .largeTitle), ("title", .title1), ("title2", .title2), ("title3", .title3),
    ("headline", .headline), ("subheadline", .subheadline), ("body", .body), ("callout", .callout),
    ("footnote", .footnote), ("caption", .caption1), ("caption2", .caption2),
]

let categories: [(String, UIContentSizeCategory)] = [
    ("x_small", .extraSmall), ("small", .small), ("medium", .medium), ("large", .large),
    ("x_large", .extraLarge), ("xx_large", .extraExtraLarge), ("xxx_large", .extraExtraExtraLarge),
    ("accessibility1", .accessibilityMedium), ("accessibility2", .accessibilityLarge),
    ("accessibility3", .accessibilityExtraLarge), ("accessibility4", .accessibilityExtraExtraLarge),
    ("accessibility5", .accessibilityExtraExtraExtraLarge),
]

func weight(of font: UIFont) -> Int {
    let traits = font.fontDescriptor.object(forKey: .traits) as? [UIFontDescriptor.TraitKey: Any]
    let w = (traits?[.weight] as? Double) ?? 0
    let table: [(Double, Int)] = [(-0.8, 100), (-0.6, 200), (-0.4, 300), (0, 400), (0.23, 500), (0.3, 600), (0.4, 700), (0.56, 800), (0.62, 900)]
    return table.min { abs($0.0 - w) < abs($1.0 - w) }!.1
}

func format(_ value: CGFloat) -> String {
    var text = String(format: "%.3f", Double(value))
    while text.hasSuffix("0") { text.removeLast() }
    if text.hasSuffix(".") { text.removeLast() }
    return text
}

var lines = ["# os iOS \(UIDevice.current.systemVersion) simulator", "style\tcategory\tsize\tweight\tline_height\tleading"]
for (name, style) in styles {
    for (category, value) in categories {
        let traits = UITraitCollection(preferredContentSizeCategory: value)
        let font = UIFont.preferredFont(forTextStyle: style, compatibleWith: traits)
        lines.append("\(name)\t\(category)\t\(format(font.pointSize))\t\(weight(of: font))\t\(format(font.lineHeight))\t\(format(font.leading))")
    }
}
print(lines.joined(separator: "\n"))
