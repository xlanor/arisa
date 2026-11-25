#pragma once

#include <compare>
#include <concepts>
#include <limits>
#include <optional>

namespace arisa::support {

template <std::integral T, T Min, T Max, class Tag>
  requires(Min <= Max)
class BoundedValue {
 public:
  static constexpr auto create(T value) noexcept
      -> std::optional<BoundedValue> {
    if (!contains(value)) {
      return std::nullopt;
    }
    return BoundedValue{value};
  }

  template <T Value>
    requires(Value >= Min && Value <= Max)
  static consteval auto of() noexcept -> BoundedValue {
    return BoundedValue{Value};
  }

  static constexpr auto minimum() noexcept -> T { return Min; }

  static constexpr auto maximum() noexcept -> T { return Max; }

  constexpr auto value() const noexcept -> T { return value_; }

  friend constexpr auto operator==(const BoundedValue&, const BoundedValue&)
      -> bool = default;
  friend constexpr auto operator<=>(const BoundedValue&,
                                    const BoundedValue&) = default;

 private:
  constexpr explicit BoundedValue(T value) noexcept : value_(value) {}

  static constexpr auto contains(T value) noexcept -> bool {
    bool at_least_min = true;
    bool at_most_max = true;
    if constexpr (Min != std::numeric_limits<T>::min()) {
      at_least_min = value >= Min;
    }
    if constexpr (Max != std::numeric_limits<T>::max()) {
      at_most_max = value <= Max;
    }
    return at_least_min && at_most_max;
  }

  T value_;
};

}
