#include <arisa/theme/oklch.hpp>
#include <arisa/theme/semantic_palette.hpp>

#include <algorithm>
#include <cmath>
#include <utility>

namespace arisa::theme {

auto SemanticPalette::rows() noexcept -> std::span<const Row> {
  static constexpr std::array table{
      Row{ColourRole::window_background, 0xFFFFFF, 0x1E1E1E, 0x1E1E1E, 0xFFFFFF,
          0x1E1E1E, 0x1E1E1E},
      Row{ColourRole::control_background, 0xFFFFFF, 0x2C2C2E, 0x2C2C2E,
          0xFFFFFF, 0x2C2C2E, 0x2C2C2E},
      Row{ColourRole::system_background, 0xFFFFFF, 0x000000, 0x1C1C1E, 0xFFFFFF,
          0x000000, 0x242426},
      Row{ColourRole::secondary_system_background, 0xF2F2F7, 0x1C1C1E, 0x2C2C2E,
          0xEBEBF0, 0x242426, 0x363638},
      Row{ColourRole::tertiary_system_background, 0xFFFFFF, 0x2C2C2E, 0x3A3A3C,
          0xFFFFFF, 0x363638, 0x444446},
      Row{ColourRole::system_grouped_background, 0xF2F2F7, 0x000000, 0x1C1C1E,
          0xEBEBF0, 0x000000, 0x242426},
      Row{ColourRole::secondary_system_grouped_background, 0xFFFFFF, 0x1C1C1E,
          0x2C2C2E, 0xFFFFFF, 0x242426, 0x363638},
      Row{ColourRole::tertiary_system_grouped_background, 0xF2F2F7, 0x2C2C2E,
          0x3A3A3C, 0xEBEBF0, 0x363638, 0x444446},
      Row{ColourRole::under_page_background, 0xF6F6F6, 0x282828, 0x282828,
          0xF6F6F6, 0x282828, 0x282828},
      Row{ColourRole::text_background, 0xFFFFFF, 0x2C2C2E, 0x2C2C2E, 0xFFFFFF,
          0x2C2C2E, 0x2C2C2E},
      Row{ColourRole::alternating_content_background, 0xFFFFFF, 0x2C2C2E,
          0x2C2C2E, 0xFFFFFF, 0x2C2C2E, 0x2C2C2E},
      Row{ColourRole::alternating_content_background_alternate, 0xF4F5F5,
          0x373738, 0x373738, 0xF4F5F5, 0x373738, 0x373738},
      Row{ColourRole::scrubber_textured_background, 0xF6F6F6, 0x282828,
          0x282828, 0xF6F6F6, 0x282828, 0x282828},
      Row{ColourRole::label, 0x262626, 0xDFDFE0, 0xDFDFE0, 0x262626, 0xDFDFE0,
          0xDFDFE0},
      Row{ColourRole::secondary_label, 0x6F6F6F, 0xA0A0A1, 0xA0A0A1, 0x6A6A6A,
          0xA0A0A1, 0xA0A0A1},
      Row{ColourRole::tertiary_label, 0xBDBDBD, 0x616162, 0x616162, 0xBDBDBD,
          0x616162, 0x616162},
      Row{ColourRole::quaternary_label, 0xE6E6E6, 0x414143, 0x414143, 0xE6E6E6,
          0x414143, 0x414143},
      Row{ColourRole::quinary_label, 0xF2F2F2, 0x373738, 0x373738, 0xF2F2F2,
          0x373738, 0x373738},
      Row{ColourRole::placeholder_text, 0x808080, 0xA0A0A1, 0xA0A0A1, 0x808080,
          0xA0A0A1, 0xA0A0A1},
      Row{ColourRole::text, 0x000000, 0xFFFFFF, 0xFFFFFF, 0x000000, 0xFFFFFF,
          0xFFFFFF},
      Row{ColourRole::header_text, 0x262626, 0xFFFFFF, 0xFFFFFF, 0x262626,
          0xFFFFFF, 0xFFFFFF},
      Row{ColourRole::window_frame_text, 0x262626, 0xDDDDDD, 0xDDDDDD, 0x262626,
          0xDDDDDD, 0xDDDDDD},
      Row{ColourRole::link, 0x0068DA, 0x419CFF, 0x419CFF, 0x0068DA, 0x419CFF,
          0x419CFF},
      Row{ColourRole::system_fill, 0xE6E6E6, 0x414143, 0x414143, 0xE6E6E6,
          0x414143, 0x414143},
      Row{ColourRole::secondary_system_fill, 0xEBEBEB, 0x3D3D3F, 0x3D3D3F,
          0xEBEBEB, 0x3D3D3F, 0x3D3D3F},
      Row{ColourRole::tertiary_system_fill, 0xF2F2F2, 0x373738, 0x373738,
          0xF2F2F2, 0x373738, 0x373738},
      Row{ColourRole::quaternary_system_fill, 0xF7F7F7, 0x323234, 0x323234,
          0xF7F7F7, 0x323234, 0x323234},
      Row{ColourRole::quinary_system_fill, 0xFCFCFC, 0x2E2E30, 0x2E2E30,
          0xFCFCFC, 0x2E2E30, 0x2E2E30},
      Row{ColourRole::separator, 0xE6E6E6, 0x414143, 0x414143, 0xE6E6E6,
          0x414143, 0x414143},
      Row{ColourRole::opaque_separator, 0xE7E7E8, 0x2A2A2C, 0x2A2A2C, 0xE7E7E8,
          0x2A2A2C, 0x2A2A2C},
      Row{ColourRole::grid, 0xE6E6E6, 0x1A1A1A, 0x1A1A1A, 0xE6E6E6, 0x1A1A1A,
          0x1A1A1A},
      Row{ColourRole::control, 0xFFFFFF, 0x616162, 0x616162, 0xFFFFFF, 0x616162,
          0x616162},
      Row{ColourRole::control_text, 0x262626, 0xE7E7E7, 0xE7E7E7, 0x262626,
          0xE7E7E7, 0xE7E7E7},
      Row{ColourRole::disabled_control_text, 0xBFBFBF, 0x888889, 0x888889,
          0xBFBFBF, 0x888889, 0x888889},
      Row{ColourRole::selected_control, 0xB3D7FF, 0x3F638B, 0x3F638B, 0xB3D7FF,
          0x3F638B, 0x3F638B},
      Row{ColourRole::selected_control_text, 0x1B2026, 0xE2E8EE, 0xE2E8EE,
          0x1B2026, 0xE2E8EE, 0xE2E8EE},
      Row{ColourRole::alternate_selected_control_text, 0xFFFFFF, 0xFFFFFF,
          0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF},
      Row{ColourRole::selected_text, 0x000000, 0xFFFFFF, 0xFFFFFF, 0x000000,
          0xFFFFFF, 0xFFFFFF},
      Row{ColourRole::selected_text_background, 0xB3D7FF, 0x3F638B, 0x3F638B,
          0xB3D7FF, 0x3F638B, 0x3F638B},
      Row{ColourRole::selected_menu_item_text, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF,
          0xFFFFFF, 0xFFFFFF, 0xFFFFFF},
      Row{ColourRole::selected_content_background, 0x0064E1, 0x0059D1, 0x0059D1,
          0x0064E1, 0x0059D1, 0x0059D1},
      Row{ColourRole::unemphasized_selected_content_background, 0xDCDCDC,
          0x464646, 0x464646, 0xDCDCDC, 0x464646, 0x464646},
      Row{ColourRole::unemphasized_selected_text_background, 0xDCDCDC, 0x464646,
          0x464646, 0xDCDCDC, 0x464646, 0x464646},
      Row{ColourRole::unemphasized_selected_text, 0x000000, 0xFFFFFF, 0xFFFFFF,
          0x000000, 0xFFFFFF, 0xFFFFFF},
      Row{ColourRole::keyboard_focus_indicator, 0x80B3FA, 0x236A96, 0x236A96,
          0x80B3FA, 0x236A96, 0x236A96},
      Row{ColourRole::find_highlight, 0xFFFF00, 0xFFFF00, 0xFFFF00, 0xFFFF00,
          0xFFFF00, 0xFFFF00},
      Row{ColourRole::highlight, 0xFFFFFF, 0xB4B4B4, 0xB4B4B4, 0xFFFFFF,
          0xB4B4B4, 0xB4B4B4},
      Row{ColourRole::shadow, 0x000000, 0x000000, 0x000000, 0x000000, 0x000000,
          0x000000},
      Row{ColourRole::dark_text, 0x000000, 0x000000, 0x000000, 0x000000,
          0x000000, 0x000000},
      Row{ColourRole::light_text, 0x999999, 0x999999, 0x999999, 0x999999,
          0x999999, 0x999999},
      Row{ColourRole::red, 0xFF383C, 0xFF4245, 0xFF4245, 0xE9152D, 0xFF6165,
          0xFF6165},
      Row{ColourRole::orange, 0xFF8D28, 0xFF9230, 0xFF9230, 0xC55300, 0xFFA056,
          0xFFA056},
      Row{ColourRole::yellow, 0xFFCC00, 0xFFD600, 0xFFD600, 0xA16A00, 0xFEDF43,
          0xFEDF43},
      Row{ColourRole::green, 0x34C759, 0x30D158, 0x30D158, 0x008932, 0x4AD968,
          0x4AD968},
      Row{ColourRole::mint, 0x00C8B3, 0x00DAC3, 0x00DAC3, 0x008575, 0x54DFCB,
          0x54DFCB},
      Row{ColourRole::teal, 0x00C3D0, 0x00D2E0, 0x00D2E0, 0x008198, 0x3BDDEC,
          0x3BDDEC},
      Row{ColourRole::cyan, 0x00C0E8, 0x3CD3FE, 0x3CD3FE, 0x007EAE, 0x6DD9FF,
          0x6DD9FF},
      Row{ColourRole::blue, 0x0088FF, 0x0091FF, 0x0091FF, 0x1E6EF4, 0x5CB8FF,
          0x5CB8FF},
      Row{ColourRole::indigo, 0x6155F5, 0x6D7CFF, 0x6D7CFF, 0x564ADE, 0xA7AAFF,
          0xA7AAFF},
      Row{ColourRole::purple, 0xCB30E0, 0xDB34F2, 0xDB34F2, 0xB02FC2, 0xEA8DFF,
          0xEA8DFF},
      Row{ColourRole::pink, 0xFF2D55, 0xFF375F, 0xFF375F, 0xE7124D, 0xFF8AC4,
          0xFF8AC4},
      Row{ColourRole::brown, 0xAC7F5E, 0xB78A66, 0xB78A66, 0x956D51, 0xDBA679,
          0xDBA679},
      Row{ColourRole::gray, 0x8E8E93, 0x98989D, 0x98989D, 0x6C6C70, 0xAEAEB2,
          0xAEAEB2},
      Row{ColourRole::gray2, 0xAEAEB2, 0x636366, 0x636366, 0x8E8E93, 0x7C7C80,
          0x7C7C80},
      Row{ColourRole::gray3, 0xC7C7CC, 0x48484A, 0x48484A, 0xAEAEB2, 0x545456,
          0x545456},
      Row{ColourRole::gray4, 0xD1D1D6, 0x3A3A3C, 0x3A3A3C, 0xBCBCC0, 0x444446,
          0x444446},
      Row{ColourRole::gray5, 0xE5E5EA, 0x2C2C2E, 0x2C2C2E, 0xD8D8DC, 0x363638,
          0x363638},
      Row{ColourRole::gray6, 0xF2F2F7, 0x1C1C1E, 0x1C1C1E, 0xEBEBF0, 0x242426,
          0x242426},
  };
  return table;
}

auto SemanticPalette::variants(const Row& row) noexcept -> AppearanceVariants {
  return AppearanceVariants{
      .light = Colour::hex(row.light),
      .dark = Colour::hex(row.dark),
      .dark_elevated = Colour::hex(row.dark_elevated),
      .high_contrast_light = Colour::hex(row.high_contrast_light),
      .high_contrast_dark = Colour::hex(row.high_contrast_dark),
      .high_contrast_dark_elevated =
          Colour::hex(row.high_contrast_dark_elevated)};
}

template <class Transform>
auto SemanticPalette::each(const AppearanceVariants& base,
                           Transform transform) noexcept -> AppearanceVariants {
  return AppearanceVariants{
      .light = transform(base.light),
      .dark = transform(base.dark),
      .dark_elevated = transform(base.dark_elevated),
      .high_contrast_light = transform(base.high_contrast_light),
      .high_contrast_dark = transform(base.high_contrast_dark),
      .high_contrast_dark_elevated =
          transform(base.high_contrast_dark_elevated)};
}

auto SemanticPalette::follow(Colour accent, Colour reference) noexcept
    -> Colour {
  if (accent == reference_accent) {
    return reference;
  }
  const Oklch seed = Oklch::from(accent);
  const Oklch anchor = Oklch::from(reference_accent);
  const Oklch target = Oklch::from(reference);
  const float lighter = target.lightness() - anchor.lightness();
  const float lightness =
      lighter >= 0.0F
          ? seed.lightness() + ((1.0F - seed.lightness()) * lighter /
                                (1.0F - anchor.lightness()))
          : seed.lightness() * target.lightness() / anchor.lightness();
  float chroma = anchor.chroma() > 0.0F
                     ? seed.chroma() * (target.chroma() / anchor.chroma())
                     : target.chroma();
  Oklch shifted{std::clamp(lightness, 0.0F, 1.0F), chroma, seed.hue()};
  while (!shifted.in_gamut() && chroma > 0.0F) {
    chroma = std::max(chroma - 0.002F, 0.0F);
    shifted = shifted.with_chroma(chroma);
  }
  return shifted.to_colour();
}

auto SemanticPalette::separated(Colour ring, Colour background) noexcept
    -> Colour {
  const Oklch shown = Oklch::from(ring);
  const float behind = Oklch::from(background).lightness();
  if (std::abs(shown.lightness() - behind) >= focus_separation) {
    return ring;
  }
  const float lightness =
      behind >= 0.5F ? behind - focus_separation : behind + focus_separation;
  Oklch moved = shown.with_lightness(std::clamp(lightness, 0.0F, 1.0F));
  float chroma = moved.chroma();
  while (!moved.in_gamut() && chroma > 0.0F) {
    chroma = std::max(chroma - 0.002F, 0.0F);
    moved = moved.with_chroma(chroma);
  }
  return moved.to_colour();
}

auto SemanticPalette::pressed(Colour base) noexcept -> Colour {
  const Oklch value = Oklch::from(base);
  return value.with_lightness(value.lightness() - pressed_lightness_step)
      .to_colour();
}

auto SemanticPalette::on(Colour base) noexcept -> Colour {
  const Oklch value = Oklch::from(base);
  return value.lightness() < on_accent_threshold
             ? Colour{1.0F, 1.0F, 1.0F, 1.0F}
             : Oklch{0.2F, 0.02F, value.hue()}.to_colour();
}

auto SemanticPalette::derive(Colour accent) -> Variants {
  Variants palette{};
  for (const Row& row : rows()) {
    palette[std::to_underlying(row.role)] = variants(row);
  }
  const AppearanceVariants accent_reference{
      .light = reference_accent,
      .dark = reference_accent,
      .dark_elevated = reference_accent,
      .high_contrast_light = Colour::hex(0x1E6EF4),
      .high_contrast_dark = Colour::hex(0x5CB8FF),
      .high_contrast_dark_elevated = Colour::hex(0x5CB8FF)};
  const AppearanceVariants tint =
      each(accent_reference,
           [accent](Colour column) { return follow(accent, column); });
  const auto set = [&palette](ColourRole role, AppearanceVariants value) {
    palette[std::to_underlying(role)] = value;
  };
  set(ColourRole::accent, tint);
  set(ColourRole::accent_pressed,
      each(tint, [](Colour column) { return pressed(column); }));
  set(ColourRole::on_accent,
      each(tint, [](Colour column) { return on(column); }));
  for (const ColourRole role :
       {ColourRole::selected_content_background, ColourRole::selected_control,
        ColourRole::selected_text_background,
        ColourRole::keyboard_focus_indicator}) {
    const AppearanceVariants& reference = palette[std::to_underlying(role)];
    set(role, each(reference,
                   [accent](Colour column) { return follow(accent, column); }));
  }
  const AppearanceVariants& window =
      palette[std::to_underlying(ColourRole::window_background)];
  AppearanceVariants& ring =
      palette[std::to_underlying(ColourRole::keyboard_focus_indicator)];
  ring.light = separated(ring.light, window.light);
  ring.dark = separated(ring.dark, window.dark);
  ring.dark_elevated = separated(ring.dark_elevated, window.dark_elevated);
  ring.high_contrast_light =
      separated(ring.high_contrast_light, window.high_contrast_light);
  ring.high_contrast_dark =
      separated(ring.high_contrast_dark, window.high_contrast_dark);
  ring.high_contrast_dark_elevated = separated(
      ring.high_contrast_dark_elevated, window.high_contrast_dark_elevated);
  const Colour light_scrim{0.0F, 0.0F, 0.0F, 0.35F};
  const Colour dark_scrim{0.0F, 0.0F, 0.0F, 0.55F};
  set(ColourRole::scrim,
      AppearanceVariants{.light = light_scrim,
                         .dark = dark_scrim,
                         .dark_elevated = dark_scrim,
                         .high_contrast_light = light_scrim,
                         .high_contrast_dark = dark_scrim,
                         .high_contrast_dark_elevated = dark_scrim});
  return palette;
}

}
