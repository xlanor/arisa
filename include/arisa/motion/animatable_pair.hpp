#pragma once

#include <arisa/motion/animatable_traits.hpp>
#include <arisa/motion/animatable_vector.hpp>

#include <array>
#include <cstddef>

namespace arisa::motion {

template <Animatable First, Animatable Second>
struct AnimatablePair {
  First first{};
  Second second{};

  static constexpr auto zero() noexcept -> AnimatablePair {
    return AnimatablePair{};
  }

  constexpr auto magnitude_squared() const noexcept -> double {
    return AnimatableTraits<AnimatablePair>::vector(*this).magnitude_squared();
  }

  constexpr auto scaled(double factor) const noexcept -> AnimatablePair {
    return AnimatableTraits<AnimatablePair>::value(
        AnimatableTraits<AnimatablePair>::vector(*this) * factor);
  }

  constexpr void scale(double factor) noexcept { *this = scaled(factor); }

  constexpr auto interpolated(const AnimatablePair& towards,
                              double amount) const noexcept -> AnimatablePair {
    return AnimatableTraits<AnimatablePair>::value(
        AnimatableTraits<AnimatablePair>::vector(*this).interpolated(
            AnimatableTraits<AnimatablePair>::vector(towards), amount));
  }

  constexpr void interpolate(const AnimatablePair& towards,
                             double amount) noexcept {
    *this = interpolated(towards, amount);
  }

  friend constexpr auto operator+(const AnimatablePair& left,
                                  const AnimatablePair& right) noexcept
      -> AnimatablePair {
    return AnimatableTraits<AnimatablePair>::value(
        AnimatableTraits<AnimatablePair>::vector(left) +
        AnimatableTraits<AnimatablePair>::vector(right));
  }

  friend constexpr auto operator-(const AnimatablePair& left,
                                  const AnimatablePair& right) noexcept
      -> AnimatablePair {
    return AnimatableTraits<AnimatablePair>::value(
        AnimatableTraits<AnimatablePair>::vector(left) -
        AnimatableTraits<AnimatablePair>::vector(right));
  }

  friend auto operator==(const AnimatablePair&, const AnimatablePair&)
      -> bool = default;
};

template <class T>
struct AnimatableWidth {
  static constexpr std::size_t value = AnimatableVector::capacity;
};

template <std::floating_point T>
struct AnimatableWidth<T> {
  static constexpr std::size_t value = 1;
};

template <>
struct AnimatableWidth<Angle> {
  static constexpr std::size_t value = 1;
};

template <Animatable First, Animatable Second>
struct AnimatableWidth<AnimatablePair<First, Second>> {
  static constexpr std::size_t value =
      AnimatableWidth<First>::value + AnimatableWidth<Second>::value;
};

template <Animatable First, Animatable Second>
  requires(AnimatableWidth<First>::value + AnimatableWidth<Second>::value <=
           AnimatableVector::capacity)
struct AnimatableTraits<AnimatablePair<First, Second>> {
  static constexpr std::size_t split = AnimatableWidth<First>::value;

  static constexpr auto vector(
      const AnimatablePair<First, Second>& pair) noexcept -> AnimatableVector {
    const AnimatableVector head = AnimatableTraits<First>::vector(pair.first);
    const AnimatableVector tail = AnimatableTraits<Second>::vector(pair.second);
    std::array<double, AnimatableVector::capacity> joined{};
    for (std::size_t index = 0; index < AnimatableVector::capacity; ++index) {
      joined[index] = index < split ? head[index] : tail[index - split];
    }
    return AnimatableVector{joined[0], joined[1], joined[2], joined[3]};
  }

  static constexpr auto value(const AnimatableVector& vector) noexcept
      -> AnimatablePair<First, Second> {
    std::array<double, AnimatableVector::capacity> tail{};
    for (std::size_t index = split; index < AnimatableVector::capacity;
         ++index) {
      tail[index - split] = vector[index];
    }
    return AnimatablePair<First, Second>{
        .first = AnimatableTraits<First>::value(vector),
        .second = AnimatableTraits<Second>::value(
            AnimatableVector{tail[0], tail[1], tail[2], tail[3]})};
  }
};

}
