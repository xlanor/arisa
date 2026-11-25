#pragma once

#include <arisa/theme/color_scheme.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/material.hpp>

#include <optional>

namespace arisa::rendering {

class GlassLens {
 public:
  struct Surface {
    const theme::LiquidGlassStyle& style;
    theme::Glass glass;
    theme::ColorScheme scheme{theme::ColorScheme::light};
    std::optional<theme::Colour> tint;
  };

  static constexpr float amplitude_share = 0.93F;
  static constexpr float regular_strength = 0.38F;
  static constexpr float regular_strength_from = 56.0F;
  static constexpr float regular_strength_to = 150.0F;
  static constexpr float frost_onset = 60.0F;
  static constexpr float frost_full = 88.0F;
  static constexpr float frost_growth = 0.0054F;
  static constexpr float blur_per_sigma = 1.4F;
  static constexpr float clear_gain = 0.95F;
  static constexpr float clear_lift = 0.125F;
  static constexpr float clear_chroma = 1.05F;
  static constexpr float clear_rim = 3.3F;
  static constexpr float light_chroma = 0.9F;
  static constexpr float dark_gain = 0.98F;
  static constexpr float dark_chroma = 1.1F;
  static constexpr float dark_curve = 0.41F;
  static constexpr float dark_curve_slope = 0.105F;
  static constexpr float dark_curve_from = 28.0F;
  static constexpr float dark_curve_high = 0.62F;
  static constexpr float chroma_bias = 0.5F;
  static constexpr float chroma_span = 4.0F;
  static constexpr float curve_steps = 1000.0F;
  static constexpr float light_tint_floor = 0.79F;
  static constexpr float dark_tint_floor = 1.32F;
  static constexpr float shadow_onset = 16.0F;
  static constexpr float shadow_span = 130.0F;
  static constexpr float shadow_floor = 0.1F;
  static constexpr float clear_cast = 0.35F;

  GlassLens(const Surface& surface, float extent, float scale) noexcept;

  auto amplitude() const noexcept -> float { return amplitude_; }

  auto blur() const noexcept -> float { return blur_; }

  auto rim() const noexcept -> float { return rim_; }

  auto shadow() const noexcept -> float { return shadow_; }

  auto veil() const noexcept -> theme::Colour { return veil_; }

  auto response() const noexcept -> float { return response_; }

  static auto frosted(theme::Colour tint, float saturation) noexcept
      -> GlassLens;

  static auto packed(float chroma, float curve) noexcept -> float;

  static auto cast(const theme::Glass& glass) noexcept -> float;

 private:
  static auto strength(bool clear, float extent) noexcept -> float;

  static auto frost(const theme::LiquidGlassStyle& style, bool clear,
                    float extent) noexcept -> float;

  struct Shading {
    theme::Colour veil;
    float chroma{1.0F};
    float curve{0.0F};
  };

  GlassLens(float amplitude, float blur, float rim, float shadow,
            Shading shading) noexcept;

  static auto shading(const Surface& surface, float extent) noexcept -> Shading;

  static auto shade(float extent) noexcept -> float;

  float amplitude_;
  float blur_;
  float rim_;
  float shadow_;
  theme::Colour veil_;
  float response_;
};

}
