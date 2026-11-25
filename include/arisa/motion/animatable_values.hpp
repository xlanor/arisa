#pragma once

#include <arisa/motion/animatable_pair.hpp>
#include <arisa/motion/animatable_traits.hpp>
#include <arisa/motion/animatable_vector.hpp>

#include <array>
#include <cstddef>
#include <tuple>
#include <utility>

namespace arisa::motion {

template <Animatable... Values>
  requires((AnimatableWidth<Values>::value + ... + 0) <=
           AnimatableVector::capacity)
struct AnimatableValues {
  std::tuple<Values...> value{};

  constexpr AnimatableValues() noexcept = default;

  constexpr explicit AnimatableValues(Values... values) noexcept
      : value(std::move(values)...) {}

  static constexpr auto zero() noexcept -> AnimatableValues {
    return AnimatableValues{};
  }

  constexpr auto magnitude_squared() const noexcept -> double {
    return AnimatableTraits<AnimatableValues>::vector(*this)
        .magnitude_squared();
  }

  constexpr auto scaled(double factor) const noexcept -> AnimatableValues {
    return AnimatableTraits<AnimatableValues>::value(
        AnimatableTraits<AnimatableValues>::vector(*this) * factor);
  }

  constexpr void scale(double factor) noexcept { *this = scaled(factor); }

  constexpr auto interpolated(const AnimatableValues& towards,
                              double amount) const noexcept
      -> AnimatableValues {
    return AnimatableTraits<AnimatableValues>::value(
        AnimatableTraits<AnimatableValues>::vector(*this).interpolated(
            AnimatableTraits<AnimatableValues>::vector(towards), amount));
  }

  constexpr void interpolate(const AnimatableValues& towards,
                             double amount) noexcept {
    *this = interpolated(towards, amount);
  }

  friend constexpr auto operator+(const AnimatableValues& left,
                                  const AnimatableValues& right) noexcept
      -> AnimatableValues {
    return AnimatableTraits<AnimatableValues>::value(
        AnimatableTraits<AnimatableValues>::vector(left) +
        AnimatableTraits<AnimatableValues>::vector(right));
  }

  friend constexpr auto operator-(const AnimatableValues& left,
                                  const AnimatableValues& right) noexcept
      -> AnimatableValues {
    return AnimatableTraits<AnimatableValues>::value(
        AnimatableTraits<AnimatableValues>::vector(left) -
        AnimatableTraits<AnimatableValues>::vector(right));
  }

  friend auto operator==(const AnimatableValues&, const AnimatableValues&)
      -> bool = default;
};

template <Animatable... Values>
struct AnimatableWidth<AnimatableValues<Values...>> {
  static constexpr std::size_t value =
      (AnimatableWidth<Values>::value + ... + 0);
};

template <Animatable... Values>
struct AnimatableTraits<AnimatableValues<Values...>> {
  static constexpr std::array<std::size_t, sizeof...(Values)> widths{
      AnimatableWidth<Values>::value...};

  static constexpr auto offset(std::size_t index) noexcept -> std::size_t {
    std::size_t total = 0;
    for (std::size_t at = 0; at < index; ++at) {
      total += widths[at];
    }
    return total;
  }

  static constexpr auto vector(
      const AnimatableValues<Values...>& values) noexcept -> AnimatableVector {
    std::array<double, AnimatableVector::capacity> joined{};
    [&]<std::size_t... Index>(std::index_sequence<Index...>) {
      (
          [&] {
            const AnimatableVector part = AnimatableTraits<
                std::tuple_element_t<Index, std::tuple<Values...>>>::
                vector(std::get<Index>(values.value));
            for (std::size_t at = 0; at < widths[Index]; ++at) {
              joined[offset(Index) + at] = part[at];
            }
          }(),
          ...);
    }(std::index_sequence_for<Values...>{});
    return AnimatableVector{joined[0], joined[1], joined[2], joined[3]};
  }

  static constexpr auto value(const AnimatableVector& vector) noexcept
      -> AnimatableValues<Values...> {
    AnimatableValues<Values...> made;
    [&]<std::size_t... Index>(std::index_sequence<Index...>) {
      (
          [&] {
            std::array<double, AnimatableVector::capacity> part{};
            for (std::size_t at = 0; at < widths[Index]; ++at) {
              part[at] = vector[offset(Index) + at];
            }
            std::get<Index>(made.value) = AnimatableTraits<std::tuple_element_t<
                Index, std::tuple<Values...>>>::value(AnimatableVector{
                part[0], part[1], part[2], part[3]});
          }(),
          ...);
    }(std::index_sequence_for<Values...>{});
    return made;
  }
};

}
