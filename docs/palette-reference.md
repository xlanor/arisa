# Apple semantic palette reference

Starting values for arisa's semantic palette. arisa makes every role opaque and tunes it for the Switch handheld LCD; the final values live in `src/theme/theme.cpp`, and the report for each palette change lists reference against final.

Authoritative values are the live runtime dumps below, resolved on this Mac with `tools/apple_palette/` (UIKit through the iOS 27 simulator with explicit light, dark, elevated and increased-contrast traits; AppKit on macOS 27 per named appearance). Rerun those scripts to refresh. `@NN%` is alpha.

Earlier sources, kept for cross-checking:
- System colours and grays: Apple Human Interface Guidelines, "Color", Specifications (updated June 2025, Liquid Glass era), read from the swatch alt text.
- UI element colours: Apple publishes the role names only (UIKit "UI element colors"); the values below are runtime dumps from Noah Gilmore, "Backwards compatibility for iOS 13 system colors" (2019). Element values have been stable since; system hues are newer in the HIG table, so the HIG wins where they overlap.
- macOS NSColor roles: names and purposes from the HIG macOS table; Apple publishes no values, so arisa derives them.

## iOS UIKit, live (iOS 27 simulator)

Captured: os iOS 27.0

| role | light | dark | dark_elevated | hc_light | hc_dark | hc_dark_elevated |
|---|---|---|---|---|---|---|
| label | #000000 | #FFFFFF | #FFFFFF | #000000 | #FFFFFF | #FFFFFF |
| secondary_label | #3C3C43@60% | #EBEBF5@60% | #EBEBF5@60% | #3C3C43@80% | #EBEBF5@70% | #EBEBF5@70% |
| tertiary_label | #3C3C43@30% | #EBEBF5@30% | #EBEBF5@30% | #3C3C43@70% | #EBEBF5@55% | #EBEBF5@55% |
| quaternary_label | #3C3C43@18% | #EBEBF5@16% | #EBEBF5@16% | #3C3C43@55% | #EBEBF5@40% | #EBEBF5@40% |
| system_fill | #787880@20% | #787880@36% | #787880@36% | #787880@28% | #787880@44% | #787880@44% |
| secondary_system_fill | #787880@16% | #787880@32% | #787880@32% | #787880@24% | #787880@40% | #787880@40% |
| tertiary_system_fill | #767680@12% | #767680@24% | #767680@24% | #767680@20% | #767680@32% | #767680@32% |
| quaternary_system_fill | #747480@8% | #767680@18% | #767680@18% | #747480@16% | #767680@26% | #767680@26% |
| placeholder_text | #3C3C43@30% | #EBEBF5@30% | #EBEBF5@30% | #3C3C43@70% | #EBEBF5@55% | #EBEBF5@55% |
| tint | #0088FF | #0091FF | #0091FF | #1E6EF4 | #5CB8FF | #5CB8FF |
| system_background | #FFFFFF | #000000 | #1C1C1E | #FFFFFF | #000000 | #242426 |
| secondary_system_background | #F2F2F7 | #1C1C1E | #2C2C2E | #EBEBF0 | #242426 | #363638 |
| tertiary_system_background | #FFFFFF | #2C2C2E | #3A3A3C | #FFFFFF | #363638 | #444446 |
| system_grouped_background | #F2F2F7 | #000000 | #1C1C1E | #EBEBF0 | #000000 | #242426 |
| secondary_system_grouped_background | #FFFFFF | #1C1C1E | #2C2C2E | #FFFFFF | #242426 | #363638 |
| tertiary_system_grouped_background | #F2F2F7 | #2C2C2E | #3A3A3C | #EBEBF0 | #363638 | #444446 |
| separator | #3C3C43@12% | #545458@50% | #545458@50% | #3C3C43@12% | #545458@50% | #545458@50% |
| opaque_separator | #E7E7E8 | #2A2A2C | #2A2A2C | #E7E7E8 | #2A2A2C | #2A2A2C |
| link | #007AFF | #0984FF | #0984FF | #007AFF | #0984FF | #0984FF |
| dark_text | #000000 | #000000 | #000000 | #000000 | #000000 | #000000 |
| light_text | #FFFFFF@60% | #FFFFFF@60% | #FFFFFF@60% | #FFFFFF@60% | #FFFFFF@60% | #FFFFFF@60% |
| red | #FF383C | #FF4245 | #FF4245 | #E9152D | #FF6165 | #FF6165 |
| orange | #FF8D28 | #FF9230 | #FF9230 | #C55300 | #FFA056 | #FFA056 |
| yellow | #FFCC00 | #FFD600 | #FFD600 | #A16A00 | #FEDF43 | #FEDF43 |
| green | #34C759 | #30D158 | #30D158 | #008932 | #4AD968 | #4AD968 |
| mint | #00C8B3 | #00DAC3 | #00DAC3 | #008575 | #54DFCB | #54DFCB |
| teal | #00C3D0 | #00D2E0 | #00D2E0 | #008198 | #3BDDEC | #3BDDEC |
| cyan | #00C0E8 | #3CD3FE | #3CD3FE | #007EAE | #6DD9FF | #6DD9FF |
| blue | #0088FF | #0091FF | #0091FF | #1E6EF4 | #5CB8FF | #5CB8FF |
| indigo | #6155F5 | #6D7CFF | #6D7CFF | #564ADE | #A7AAFF | #A7AAFF |
| purple | #CB30E0 | #DB34F2 | #DB34F2 | #B02FC2 | #EA8DFF | #EA8DFF |
| pink | #FF2D55 | #FF375F | #FF375F | #E7124D | #FF8AC4 | #FF8AC4 |
| brown | #AC7F5E | #B78A66 | #B78A66 | #956D51 | #DBA679 | #DBA679 |
| gray | #8E8E93 | #8E8E93 | #8E8E93 | #6C6C70 | #AEAEB2 | #AEAEB2 |
| gray2 | #AEAEB2 | #636366 | #636366 | #8E8E93 | #7C7C80 | #7C7C80 |
| gray3 | #C7C7CC | #48484A | #48484A | #AEAEB2 | #545456 | #545456 |
| gray4 | #D1D1D6 | #3A3A3C | #3A3A3C | #BCBCC0 | #444446 | #444446 |
| gray5 | #E5E5EA | #2C2C2E | #2C2C2E | #D8D8DC | #363638 | #363638 |
| gray6 | #F2F2F7 | #1C1C1E | #1C1C1E | #EBEBF0 | #242426 | #242426 |

## macOS AppKit, live (macOS 27)

Captured: os Version 27.0 (Build 26A428), accent_setting multicolor (default)

| dump_nscolor.swift:13:24: warning: 'quinaryLabel' is deprecated: renamed to 'quinaryLabelColor' [#DeprecatedDeclaration] |
|---|
| 11 |     ("label", .labelColor), ("secondary_label", .secondaryLabelColor), |
| 12 |     ("tertiary_label", .tertiaryLabelColor), ("quaternary_label", .quaternaryLabelColor), |
| 13 |     ("quinary_label", .quinaryLabel), |
|    |                        |- warning: 'quinaryLabel' is deprecated: renamed to 'quinaryLabelColor' [#DeprecatedDeclaration] |
|    |                        `- note: use 'quinaryLabelColor' instead |
| 14 |     ("text", .textColor), ("placeholder_text", .placeholderTextColor), |
| 15 |     ("selected_text", .selectedTextColor), ("text_background", .textBackgroundColor), |
| [#DeprecatedDeclaration]: <https://docs.swift.org/compiler/documentation/diagnostics/deprecated-declaration> |
| role | light | dark | hc_light | hc_dark |
| label | #000000@85% | #FFFFFF@85% | #000000@85% | #FFFFFF@85% |
| secondary_label | #000000@50% | #FFFFFF@55% | #000000@50% | #FFFFFF@55% |
| tertiary_label | #000000@26% | #FFFFFF@25% | #000000@26% | #FFFFFF@25% |
| quaternary_label | #000000@10% | #FFFFFF@10% | #000000@10% | #FFFFFF@10% |
| quinary_label | #000000@5% | #FFFFFF@5% | #000000@5% | #FFFFFF@5% |
| text | #000000 | #FFFFFF | #000000 | #FFFFFF |
| placeholder_text | #000000@50% | #FFFFFF@55% | #000000@50% | #FFFFFF@55% |
| selected_text | #000000 | #FFFFFF | #000000 | #FFFFFF |
| text_background | #FFFFFF | #1E1E1E | #FFFFFF | #1E1E1E |
| selected_text_background | #B3D7FF | #3F638B | #B3D7FF | #3F638B |
| keyboard_focus_indicator | #0067F4@50% | #1AA9FF@50% | #0067F4@50% | #1AA9FF@50% |
| unemphasized_selected_text | #000000 | #FFFFFF | #000000 | #FFFFFF |
| unemphasized_selected_text_background | #DCDCDC | #464646 | #DCDCDC | #464646 |
| link | #0068DA | #419CFF | #0068DA | #419CFF |
| separator | #000000@10% | #FFFFFF@10% | #000000@10% | #FFFFFF@10% |
| selected_content_background | #0064E1 | #0059D1 | #0064E1 | #0059D1 |
| unemphasized_selected_content_background | #DCDCDC | #464646 | #DCDCDC | #464646 |
| selected_menu_item_text | #FFFFFF | #FFFFFF | #FFFFFF | #FFFFFF |
| grid | #E6E6E6 | #1A1A1A | #E6E6E6 | #1A1A1A |
| header_text | #000000@85% | #FFFFFF | #000000@85% | #FFFFFF |
| control_accent | #007AFF | #007AFF | #007AFF | #007AFF |
| control | #FFFFFF | #FFFFFF@25% | #FFFFFF | #FFFFFF@25% |
| control_background | #FFFFFF | #1E1E1E | #FFFFFF | #1E1E1E |
| control_text | #000000@85% | #FFFFFF@85% | #000000@85% | #FFFFFF@85% |
| disabled_control_text | #000000@25% | #FFFFFF@25% | #000000@25% | #FFFFFF@25% |
| selected_control | #B3D7FF | #3F638B | #B3D7FF | #3F638B |
| selected_control_text | #000000@85% | #FFFFFF@85% | #000000@85% | #FFFFFF@85% |
| alternate_selected_control_text | #FFFFFF | #FFFFFF | #FFFFFF | #FFFFFF |
| scrubber_textured_background | pattern | pattern | pattern | pattern |
| window_background | #FFFFFF | #1E1E1E | #FFFFFF | #1E1E1E |
| window_frame_text | #000000@85% | #FFFFFF@85% | #000000@85% | #FFFFFF@85% |
| under_page_background | #F6F6F6 | #282828 | #F6F6F6 | #282828 |
| find_highlight | #FFFF00 | #FFFF00 | #FFFF00 | #FFFF00 |
| highlight | #FFFFFF | #B4B4B4 | #FFFFFF | #B4B4B4 |
| shadow | #000000 | #000000 | #000000 | #000000 |
| system_fill | #000000@10% | #FFFFFF@10% | #000000@10% | #FFFFFF@10% |
| secondary_system_fill | #000000@8% | #FFFFFF@8% | #000000@8% | #FFFFFF@8% |
| tertiary_system_fill | #000000@5% | #FFFFFF@5% | #000000@5% | #FFFFFF@5% |
| quaternary_system_fill | #000000@3% | #FFFFFF@3% | #000000@3% | #FFFFFF@3% |
| quinary_system_fill | #000000@1% | #FFFFFF@1% | #000000@1% | #FFFFFF@1% |
| red | #FF383C | #FF4245 | #FF383C | #FF4245 |
| orange | #FF8D28 | #FF9230 | #FF8D28 | #FF9230 |
| yellow | #FFCC00 | #FFD600 | #FFCC00 | #FFD600 |
| green | #34C759 | #30D158 | #34C759 | #30D158 |
| mint | #00C8B3 | #00DAC3 | #00C8B3 | #00DAC3 |
| teal | #00C3D0 | #00D2E0 | #00C3D0 | #00D2E0 |
| cyan | #00C0E8 | #3CD3FE | #00C0E8 | #3CD3FE |
| blue | #0088FF | #0091FF | #0088FF | #0091FF |
| indigo | #6155F5 | #6D7CFF | #6155F5 | #6D7CFF |
| purple | #CB30E0 | #DB34F2 | #CB30E0 | #DB34F2 |
| pink | #FF2D55 | #FF375F | #FF2D55 | #FF375F |
| brown | #AC7F5E | #B78A66 | #AC7F5E | #B78A66 |
| gray | #8E8E93 | #98989D | #8E8E93 | #98989D |
| alternating_content_backgrounds | #FFFFFF / #F4F5F5 | #1E1E1E / #FFFFFF@5% | #FFFFFF / #F4F5F5 | #1E1E1E / #FFFFFF@5% |

The macOS increased-contrast appearances only resolve differently when the system setting is on, so the hc columns repeat the defaults; use the iOS increased-contrast values for the system hues.

Notable drift from the 2019 dump: iOS separator is now #3C3C43@12% / #545458@50% (was 29% / 60%), opaque_separator #E7E7E8 / #2A2A2C (was #C6C6C8 / #38383A); quaternary_label dark is 16%. macOS label tiers are black/white at 85/50/26/10/5% and its fills are black/white at 10/8/5/3/1%, unlike iOS greys.

## System colours (HIG)

| Role | SwiftUI | Light | Dark | Increased contrast light | Increased contrast dark |
|---|---|---|---|---|---|
| red | `red` | #FF383C | #FF4245 | #E9152D | #FF6165 |
| orange | `orange` | #FF8D28 | #FF9230 | #C55300 | #FFA056 |
| yellow | `yellow` | #FFCC00 | #FFD600 | #A16A00 | #FEDF43 |
| green | `green` | #34C759 | #30D158 | #008932 | #4AD968 |
| mint | `mint` | #00C8B3 | #00DAC3 | #008575 | #54DFCB |
| teal | `teal` | #00C3D0 | #00D2E0 | #008198 | #3BDDEC |
| cyan | `cyan` | #00C0E8 | #3CD3FE | #007EAE | #6DD9FF |
| blue | `blue` | #0088FF | #0091FF | #1E6EF4 | #5CB8FF |
| indigo | `indigo` | #6155F5 | #6D7CFF | #564ADE | #A7AAFF |
| purple | `purple` | #CB30E0 | #DB34F2 | #B02FC2 | #EA8DFF |
| pink | `pink` | #FF2D55 | #FF375F | #E7124D | #FF8AC4 |
| brown | `brown` | #AC7F5E | #B78A66 | #956D51 | #DBA679 |

## System grays (HIG, iOS/iPadOS)

| Role | UIKit | Light | Dark | Increased contrast light | Increased contrast dark |
|---|---|---|---|---|---|
| gray | `systemGray` | #8E8E93 | #8E8E93 | #6C6C70 | #AEAEB2 |
| gray2 | `systemGray2` | #AEAEB2 | #636366 | #8E8E93 | #7C7C80 |
| gray3 | `systemGray3` | #C7C7CC | #48484A | #AEAEB2 | #545456 |
| gray4 | `systemGray4` | #D1D1D6 | #3A3A3C | #BCBCC0 | #444446 |
| gray5 | `systemGray5` | #E5E5EA | #2C2C2E | #D8D8DC | #363638 |
| gray6 | `systemGray6` | #F2F2F7 | #1C1C1E | #EBEBF0 | #242426 |

## Rules for arisa

- Translucent references (labels below primary, fills, separator, placeholder) become opaque by compositing over the background they normally sit on, then are checked against the handheld LCD minimum steps.
- Pure black dark backgrounds are not used: arisa's dark scheme stays grey (decided in M5), so dark backgrounds shift up the gray ladder while keeping their relative steps.
- Increased contrast values seed arisa's high-contrast scheme (backlog).
