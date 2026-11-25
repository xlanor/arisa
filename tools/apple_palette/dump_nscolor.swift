import AppKit

let appearances: [(String, NSAppearance.Name)] = [
    ("light", .aqua),
    ("dark", .darkAqua),
    ("hc_light", .accessibilityHighContrastAqua),
    ("hc_dark", .accessibilityHighContrastDarkAqua),
]

let colours: [(String, NSColor)] = [
    ("label", .labelColor), ("secondary_label", .secondaryLabelColor),
    ("tertiary_label", .tertiaryLabelColor), ("quaternary_label", .quaternaryLabelColor),
    ("quinary_label", .quinaryLabelColor),
    ("text", .textColor), ("placeholder_text", .placeholderTextColor),
    ("selected_text", .selectedTextColor), ("text_background", .textBackgroundColor),
    ("selected_text_background", .selectedTextBackgroundColor),
    ("keyboard_focus_indicator", .keyboardFocusIndicatorColor),
    ("unemphasized_selected_text", .unemphasizedSelectedTextColor),
    ("unemphasized_selected_text_background", .unemphasizedSelectedTextBackgroundColor),
    ("link", .linkColor), ("separator", .separatorColor),
    ("selected_content_background", .selectedContentBackgroundColor),
    ("unemphasized_selected_content_background", .unemphasizedSelectedContentBackgroundColor),
    ("selected_menu_item_text", .selectedMenuItemTextColor), ("grid", .gridColor),
    ("header_text", .headerTextColor), ("control_accent", .controlAccentColor),
    ("control", .controlColor), ("control_background", .controlBackgroundColor),
    ("control_text", .controlTextColor), ("disabled_control_text", .disabledControlTextColor),
    ("selected_control", .selectedControlColor), ("selected_control_text", .selectedControlTextColor),
    ("alternate_selected_control_text", .alternateSelectedControlTextColor),
    ("scrubber_textured_background", .scrubberTexturedBackground),
    ("window_background", .windowBackgroundColor), ("window_frame_text", .windowFrameTextColor),
    ("under_page_background", .underPageBackgroundColor), ("find_highlight", .findHighlightColor),
    ("highlight", .highlightColor), ("shadow", .shadowColor),
    ("system_fill", .systemFill), ("secondary_system_fill", .secondarySystemFill),
    ("tertiary_system_fill", .tertiarySystemFill), ("quaternary_system_fill", .quaternarySystemFill),
    ("quinary_system_fill", .quinarySystemFill),
    ("red", .systemRed), ("orange", .systemOrange), ("yellow", .systemYellow),
    ("green", .systemGreen), ("mint", .systemMint), ("teal", .systemTeal),
    ("cyan", .systemCyan), ("blue", .systemBlue), ("indigo", .systemIndigo),
    ("purple", .systemPurple), ("pink", .systemPink), ("brown", .systemBrown),
    ("gray", .systemGray),
]

func hex(_ c: NSColor) -> String {
    guard let s = c.usingColorSpace(.sRGB) else { return "pattern" }
    let r = Int((s.redComponent * 255).rounded()), g = Int((s.greenComponent * 255).rounded()),
        b = Int((s.blueComponent * 255).rounded()), a = Int((s.alphaComponent * 100).rounded())
    let h = String(format: "#%02X%02X%02X", max(0, min(255, r)), max(0, min(255, g)), max(0, min(255, b)))
    return a == 100 ? h : "\(h)@\(a)%"
}

var header = "role"
for (n, _) in appearances { header += "\t\(n)" }
print(header)
for (name, colour) in colours {
    var line = name
    for (_, a) in appearances {
        var v = ""
        NSAppearance(named: a)!.performAsCurrentDrawingAppearance { v = hex(colour) }
        line += "\t\(v)"
    }
    print(line)
}
var alt = "alternating_content_backgrounds"
for (_, a) in appearances {
    var v = ""
    NSAppearance(named: a)!.performAsCurrentDrawingAppearance {
        v = NSColor.alternatingContentBackgroundColors.map(hex).joined(separator: " / ")
    }
    alt += "\t\(v)"
}
print(alt)
print("os\t\(ProcessInfo.processInfo.operatingSystemVersionString)")
print("accent_setting\t\(UserDefaults.standard.object(forKey: "AppleAccentColor").map { "\($0)" } ?? "multicolor (default)")")
