#pragma once

#include <arisa/motion/angle.hpp>
#include <arisa/motion/animatable_vector.hpp>

#include <concepts>

namespace arisa::motion {

template <class T>
struct AnimatableTraits;

template <std::floating_point T>
struct AnimatableTraits<T> {
  static constexpr auto vector(T value) noexcept -> AnimatableVector {
    const double widened = value;
    return AnimatableVector{widened};
  }

  static constexpr auto value(const AnimatableVector& vector) noexcept -> T {
    const T narrowed = vector[0];
    return narrowed;
  }

  static constexpr auto magnitude_squared(T value) noexcept -> T {
    return value * value;
  }
};

template <>
struct AnimatableTraits<Angle> {
  static constexpr auto vector(Angle value) noexcept -> AnimatableVector {
    return AnimatableVector{value.radians()};
  }

  static constexpr auto value(const AnimatableVector& vector) noexcept
      -> Angle {
    return Angle::radians(vector[0]);
  }
};

template <>
struct AnimatableTraits<AnimatableVector> {
  static constexpr auto vector(const AnimatableVector& value) noexcept
      -> AnimatableVector {
    return value;
  }

  static constexpr auto value(const AnimatableVector& vector) noexcept
      -> AnimatableVector {
    return vector;
  }
};

template <class T>
concept Animatable = requires(const T& value, const AnimatableVector& vector) {
  { AnimatableTraits<T>::vector(value) } -> std::same_as<AnimatableVector>;
  { AnimatableTraits<T>::value(vector) } -> std::convertible_to<T>;
};

template <class T>
concept VectorArithmetic =
    std::equality_comparable<T> && std::copy_constructible<T> &&
    requires(T value, const T& other, double amount) {
      { T::zero() } -> std::convertible_to<T>;
      { value + other } -> std::convertible_to<T>;
      { value - other } -> std::convertible_to<T>;
      { value.scaled(amount) } -> std::convertible_to<T>;
      { value.magnitude_squared() } -> std::convertible_to<double>;
      value.scale(amount);
      value.interpolate(other, amount);
      { value.interpolated(other, amount) } -> std::convertible_to<T>;
    };

}
