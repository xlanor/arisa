#pragma once

#include <arisa/theme/appearance_variants.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/colour_value.hpp>

#include <array>
#include <cstdint>
#include <span>

namespace arisa::theme {

class SemanticPalette {
 public:
  using Variants = std::array<AppearanceVariants, colour_role_count>;

  static constexpr Colour reference_accent = Colour::hex(0x007AFF);
  static constexpr float pressed_lightness_step = 0.08F;
  static constexpr float on_accent_threshold = 0.72F;
  static constexpr float focus_separation = 0.22F;

  static auto derive(Colour accent) -> Variants;

 private:
  struct Row {
    ColourRole role;
    std::uint32_t light;
    std::uint32_t dark;
    std::uint32_t dark_elevated;
    std::uint32_t high_contrast_light;
    std::uint32_t high_contrast_dark;
    std::uint32_t high_contrast_dark_elevated;
  };

  static auto rows() noexcept -> std::span<const Row>;

  static auto variants(const Row& row) noexcept -> AppearanceVariants;

  static auto follow(Colour accent, Colour reference) noexcept -> Colour;

  static auto separated(Colour ring, Colour background) noexcept -> Colour;

  static auto pressed(Colour base) noexcept -> Colour;

  static auto on(Colour base) noexcept -> Colour;

  template <class Transform>
  static auto each(const AppearanceVariants& base, Transform transform) noexcept
      -> AppearanceVariants;
};

}
