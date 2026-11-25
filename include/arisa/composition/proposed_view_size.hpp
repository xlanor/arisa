#pragma once

#include <arisa/composition/axis.hpp>
#include <arisa/composition/size.hpp>

#include <limits>
#include <optional>

namespace arisa::composition {

struct ProposedViewSize {
  std::optional<float> width{};
  std::optional<float> height{};

  static const ProposedViewSize zero;
  static const ProposedViewSize unspecified;
  static const ProposedViewSize infinity;

  constexpr ProposedViewSize() noexcept = default;

  constexpr ProposedViewSize(std::optional<float> proposed_width,
                             std::optional<float> proposed_height) noexcept
      : width(proposed_width), height(proposed_height) {}

  constexpr explicit ProposedViewSize(Size size) noexcept
      : width(size.width), height(size.height) {}

  constexpr auto replacing_unspecified_dimensions(
      Size by = Size{.width = 10.0F, .height = 10.0F}) const noexcept -> Size {
    return Size{.width = width.value_or(by.width),
                .height = height.value_or(by.height)};
  }

  constexpr auto along(Axis axis) const noexcept -> std::optional<float> {
    return axis == Axis::horizontal ? width : height;
  }

  friend constexpr auto operator==(const ProposedViewSize&,
                                   const ProposedViewSize&) -> bool = default;
};

inline constexpr ProposedViewSize ProposedViewSize::zero{0.0F, 0.0F};
inline constexpr ProposedViewSize ProposedViewSize::unspecified{};
inline constexpr ProposedViewSize ProposedViewSize::infinity{
    std::numeric_limits<float>::infinity(),
    std::numeric_limits<float>::infinity()};

}
