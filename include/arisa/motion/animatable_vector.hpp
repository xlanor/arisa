#pragma once

#include <array>
#include <cmath>
#include <cstddef>

namespace arisa::motion {

class AnimatableVector {
 public:
  static constexpr std::size_t capacity = 4;

  constexpr AnimatableVector() noexcept = default;

  constexpr explicit AnimatableVector(double first, double second = 0.0,
                                      double third = 0.0,
                                      double fourth = 0.0) noexcept
      : values_{first, second, third, fourth} {}

  constexpr auto operator[](std::size_t index) const noexcept -> double {
    return values_[index];
  }

  static constexpr auto zero() noexcept -> AnimatableVector {
    return AnimatableVector{};
  }

  constexpr auto magnitude_squared() const noexcept -> double {
    double sum = 0.0;
    for (const double value : values_) {
      sum += value * value;
    }
    return sum;
  }

  auto magnitude() const noexcept -> double {
    return std::sqrt(magnitude_squared());
  }

  constexpr void scale(double factor) noexcept { *this = *this * factor; }

  constexpr auto scaled(double factor) const noexcept -> AnimatableVector {
    return *this * factor;
  }

  constexpr void negate() noexcept { *this = -*this; }

  constexpr void interpolate(const AnimatableVector& towards,
                             double amount) noexcept {
    *this = interpolated(towards, amount);
  }

  constexpr auto interpolated(const AnimatableVector& towards,
                              double amount) const noexcept
      -> AnimatableVector {
    return *this + ((towards - *this) * amount);
  }

  constexpr auto operator+=(const AnimatableVector& other) noexcept
      -> AnimatableVector& {
    *this = *this + other;
    return *this;
  }

  constexpr auto operator-=(const AnimatableVector& other) noexcept
      -> AnimatableVector& {
    *this = *this - other;
    return *this;
  }

  friend constexpr auto operator-(const AnimatableVector& vector) noexcept
      -> AnimatableVector {
    return AnimatableVector{} - vector;
  }

  friend constexpr auto operator+(const AnimatableVector& left,
                                  const AnimatableVector& right) noexcept
      -> AnimatableVector {
    return AnimatableVector{
        left.values_[0] + right.values_[0], left.values_[1] + right.values_[1],
        left.values_[2] + right.values_[2], left.values_[3] + right.values_[3]};
  }

  friend constexpr auto operator-(const AnimatableVector& left,
                                  const AnimatableVector& right) noexcept
      -> AnimatableVector {
    return AnimatableVector{
        left.values_[0] - right.values_[0], left.values_[1] - right.values_[1],
        left.values_[2] - right.values_[2], left.values_[3] - right.values_[3]};
  }

  friend constexpr auto operator*(const AnimatableVector& vector,
                                  double factor) noexcept -> AnimatableVector {
    return AnimatableVector{
        vector.values_[0] * factor, vector.values_[1] * factor,
        vector.values_[2] * factor, vector.values_[3] * factor};
  }

  friend constexpr auto operator==(const AnimatableVector&,
                                   const AnimatableVector&) -> bool = default;

 private:
  std::array<double, capacity> values_{};
};

}
