#pragma once

#include <arisa/motion/angle.hpp>
#include <arisa/theme/colour.hpp>

#include <array>
#include <cstddef>
#include <span>

namespace arisa::graphics {

class ColourMatrix {
 public:
  static constexpr std::size_t rows = 4;
  static constexpr std::size_t columns = 5;
  static constexpr std::size_t count = rows * columns;
  static constexpr std::array<float, 3> luminance{0.2125F, 0.7154F, 0.0721F};
  static constexpr std::array<float, 3> hue_weights{0.213F, 0.715F, 0.072F};

  constexpr ColourMatrix() noexcept = default;

  static constexpr auto identity() noexcept -> ColourMatrix {
    return ColourMatrix{};
  }

  static auto of(std::array<float, count> values) noexcept -> ColourMatrix;

  static auto brightness(double amount) noexcept -> ColourMatrix;

  static auto contrast(double amount) noexcept -> ColourMatrix;

  static auto saturation(double amount) noexcept -> ColourMatrix;

  static auto grayscale(double amount) noexcept -> ColourMatrix;

  static auto hue_rotation(motion::Angle angle) noexcept -> ColourMatrix;

  static auto inverted() noexcept -> ColourMatrix;

  static auto multiplied(theme::Colour colour) noexcept -> ColourMatrix;

  static auto luminance_to_alpha() noexcept -> ColourMatrix;

  auto then(const ColourMatrix& next) const noexcept -> ColourMatrix;

  auto apply(theme::Colour straight) const noexcept -> theme::Colour;

  auto at(std::size_t row, std::size_t column) const noexcept -> float {
    return values_[(row * columns) + column];
  }

  auto values() const noexcept -> std::span<const float, count> {
    return values_;
  }

  auto is_identity() const noexcept -> bool;

  auto touches_alpha() const noexcept -> bool;

  friend constexpr auto operator==(const ColourMatrix&, const ColourMatrix&)
      -> bool = default;

 private:
  std::array<float, count> values_{1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F,
                                   0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F,
                                   0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F};
};

}
