#pragma once

#include <arisa/theme/scaled_metric.hpp>

#include <algorithm>
#include <optional>

namespace arisa::composition {

class Length {
 public:
  constexpr Length(float points) noexcept : value_(points) {}

  constexpr Length(int points) noexcept : value_(static_cast<float>(points)) {}

  constexpr Length(double points) noexcept
      : value_(static_cast<float>(points)) {}

  constexpr Length(theme::ScaledMetric metric) noexcept
      : value_(metric.value() != nullptr ? *metric.value() : 0.0F),
        scaled_(metric) {}

  constexpr Length(theme::MetricToken token) noexcept
      : Length(theme::ScaledMetric{token}) {}

  static constexpr auto percent(float fraction) noexcept -> Length {
    Length length{std::clamp(fraction, 0.0F, 1.0F)};
    length.fraction_ = true;
    return length;
  }

  constexpr auto is_fraction() const noexcept -> bool { return fraction_; }

  constexpr auto value() const noexcept -> float { return value_; }

  constexpr auto scaled() const noexcept
      -> const std::optional<theme::ScaledMetric>& {
    return scaled_;
  }

  friend constexpr auto operator==(const Length&, const Length&)
      -> bool = default;

 private:
  float value_;
  bool fraction_{false};
  std::optional<theme::ScaledMetric> scaled_;
};

}
