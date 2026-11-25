#include <arisa/rendering/glass_lens.hpp>

#include <algorithm>
#include <cmath>

namespace arisa::rendering {

GlassLens::GlassLens(const Surface& surface, float extent, float scale) noexcept
    : amplitude_(
          std::min(extent * 0.5F * amplitude_share, surface.style.refraction) *
          strength(surface.glass.variant() == theme::Glass::Variant::clear,
                   extent) *
          scale),
      blur_(frost(surface.style,
                  surface.glass.variant() == theme::Glass::Variant::clear,
                  extent) *
            blur_per_sigma * scale),
      rim_(surface.glass.variant() == theme::Glass::Variant::clear
               ? surface.style.rim * clear_rim
               : surface.style.rim),
      shadow_(surface.glass.variant() == theme::Glass::Variant::clear
                  ? 0.0F
                  : shade(extent)),
      veil_(),
      response_(0.0F) {
  const Shading shaded = shading(surface, extent);
  veil_ = shaded.veil;
  response_ = packed(shaded.chroma, shaded.curve);
}

GlassLens::GlassLens(float amplitude, float blur, float rim, float shadow,
                     Shading shading) noexcept
    : amplitude_(amplitude),
      blur_(blur),
      rim_(rim),
      shadow_(shadow),
      veil_(shading.veil),
      response_(packed(shading.chroma, shading.curve)) {}

auto GlassLens::frosted(theme::Colour tint, float saturation) noexcept
    -> GlassLens {
  const float alpha = std::clamp(tint.alpha, 0.0F, 1.0F);
  return GlassLens{
      0.0F, 0.0F, 0.0F, 0.0F,
      Shading{.veil = theme::Colour{tint.red * alpha, tint.green * alpha,
                                    tint.blue * alpha, 1.0F - alpha},
              .chroma = (1.0F - alpha) * std::max(saturation, 1.0F),
              .curve = 0.0F}};
}

auto GlassLens::cast(const theme::Glass& glass) noexcept -> float {
  return glass.variant() == theme::Glass::Variant::clear ? clear_cast : 1.0F;
}

auto GlassLens::packed(float chroma, float curve) noexcept -> float {
  const float steps = std::round(std::clamp(curve, 0.0F, 1.0F) * curve_steps);
  return (steps * chroma_span) +
         std::clamp(chroma + chroma_bias, 0.0F, chroma_span - chroma_bias);
}

auto GlassLens::strength(bool clear, float extent) noexcept -> float {
  if (clear) {
    return 1.0F;
  }
  const float t = std::clamp((extent - regular_strength_from) /
                                 (regular_strength_to - regular_strength_from),
                             0.0F, 1.0F);
  const float eased = t * t * (3.0F - (2.0F * t));
  return regular_strength + ((1.0F - regular_strength) * eased);
}

auto GlassLens::frost(const theme::LiquidGlassStyle& style, bool clear,
                      float extent) noexcept -> float {
  if (clear) {
    return style.clear_blur;
  }
  if (extent <= frost_onset) {
    return 0.0F;
  }
  if (extent < frost_full) {
    return style.blur * (extent - frost_onset) / (frost_full - frost_onset);
  }
  return style.blur * (1.0F + ((extent - frost_full) * frost_growth));
}

auto GlassLens::shade(float extent) noexcept -> float {
  return std::clamp((extent - shadow_onset) / shadow_span, shadow_floor, 1.0F);
}

auto GlassLens::shading(const Surface& surface, float extent) noexcept
    -> Shading {
  const bool dark = surface.scheme == theme::ColorScheme::dark;
  Shading shaded{};
  if (surface.glass.variant() == theme::Glass::Variant::clear) {
    shaded = Shading{
        .veil = theme::Colour{clear_lift, clear_lift, clear_lift, clear_gain},
        .chroma = clear_chroma,
        .curve = 0.0F};
  } else {
    const theme::Colour token = surface.style.tint.resolve(surface.scheme);
    const float alpha = std::clamp(token.alpha, 0.0F, 1.0F);
    const theme::Colour offset{token.red * alpha, token.green * alpha,
                               token.blue * alpha, 1.0F - alpha};
    if (dark) {
      shaded = Shading{
          .veil = offset.with_alpha(dark_gain),
          .chroma = dark_chroma,
          .curve = std::clamp(
              dark_curve + (dark_curve_slope *
                            std::log(std::max(extent, 1.0F) / dark_curve_from)),
              dark_curve, dark_curve_high)};
    } else {
      shaded = Shading{.veil = offset, .chroma = light_chroma, .curve = 0.0F};
    }
  }
  if (!surface.tint.has_value()) {
    return shaded;
  }
  const theme::Colour& tint = *surface.tint;
  const float amount = std::clamp(tint.alpha, 0.0F, 1.0F);
  const float floor = dark ? dark_tint_floor : light_tint_floor;
  const float level = floor + ((1.0F - floor) * 0.5F);
  const float kept = 1.0F - amount;
  return Shading{
      .veil =
          theme::Colour{
              (shaded.veil.red * kept) + (tint.red * level * amount),
              (shaded.veil.green * kept) + (tint.green * level * amount),
              (shaded.veil.blue * kept) + (tint.blue * level * amount),
              shaded.veil.alpha * kept},
      .chroma = shaded.chroma * kept,
      .curve = shaded.curve * kept};
}

}
