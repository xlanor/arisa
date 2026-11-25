#include <arisa/graphics/gradient.hpp>
#include <arisa/theme/colour_mix.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <utility>

namespace arisa::graphics {

Gradient::Gradient(std::vector<theme::ColourValue> colors) {
  stops_.reserve(colors.size());
  const std::size_t count = colors.size();
  for (std::size_t index = 0; index < count; ++index) {
    stops_.push_back(Stop{.color = colors[index],
                          .location = count < 2
                                          ? 0.0F
                                          : static_cast<float>(index) /
                                                static_cast<float>(count - 1)});
  }
}

Gradient::Gradient(std::vector<Stop> stops) : stops_(std::move(stops)) {
  std::ranges::stable_sort(stops_, {}, &Stop::location);
}

auto Gradient::standard(theme::ColourValue colour) -> Gradient {
  return Gradient{std::vector<Stop>{
      Stop{.color = colour.mix(theme::Colour{1.0F, 1.0F, 1.0F, 1.0F},
                               standard_lift, ColorSpace::device),
           .location = 0.0F},
      Stop{.color = colour, .location = standard_reach}}};
}

auto Gradient::color_space(ColorSpace space) const -> Gradient {
  Gradient copy = *this;
  copy.space_ = space;
  return copy;
}

auto Gradient::mix(theme::Colour from, theme::Colour to, float amount,
                   ColorSpace space) noexcept -> theme::Colour {
  return theme::ColourMix::mix(from, to, amount, space);
}

auto Gradient::sample(std::span<const theme::Colour> colours,
                      std::span<const float> locations, ColorSpace space,
                      float position) noexcept -> theme::Colour {
  if (colours.empty()) {
    return theme::Colour{0.0F, 0.0F, 0.0F, 0.0F};
  }
  if (position <= locations.front()) {
    return colours.front();
  }
  for (std::size_t index = 1; index < colours.size(); ++index) {
    if (position <= locations[index]) {
      const float span = locations[index] - locations[index - 1];
      const float amount =
          span > 0.0F ? (position - locations[index - 1]) / span : 1.0F;
      return mix(colours[index - 1], colours[index], amount, space);
    }
  }
  return colours.back();
}

}
