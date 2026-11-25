#pragma once

#include <arisa/theme/colour.hpp>
#include <arisa/theme/colour_interpolation.hpp>
#include <arisa/theme/colour_value.hpp>

#include <cstdint>
#include <span>
#include <vector>

namespace arisa::graphics {

class Gradient {
 public:
  struct Stop {
    theme::ColourValue color;
    float location{0.0F};

    friend constexpr auto operator==(const Stop&, const Stop&)
        -> bool = default;
  };

  using ColorSpace = theme::ColourInterpolation;

  explicit Gradient(std::vector<theme::ColourValue> colors);

  explicit Gradient(std::vector<Stop> stops);

  static constexpr float standard_lift = 0.2F;
  static constexpr float standard_reach = 0.7F;

  static auto standard(theme::ColourValue colour) -> Gradient;

  auto color_space(ColorSpace space) const -> Gradient;

  auto stops() const noexcept -> std::span<const Stop> { return stops_; }

  auto interpolation() const noexcept -> ColorSpace { return space_; }

  static auto mix(theme::Colour from, theme::Colour to, float amount,
                  ColorSpace space) noexcept -> theme::Colour;

  static auto sample(std::span<const theme::Colour> colours,
                     std::span<const float> locations, ColorSpace space,
                     float position) noexcept -> theme::Colour;

  friend auto operator==(const Gradient&, const Gradient&) -> bool = default;

 private:
  std::vector<Stop> stops_;
  ColorSpace space_{ColorSpace::device};
};

}
