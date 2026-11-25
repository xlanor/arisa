#pragma once

#include <compare>
#include <type_traits>
#include <utility>

namespace arisa::support {

template <class T, class Tag>
class StrongValue {
 public:
  constexpr explicit StrongValue(T value) noexcept(
      std::is_nothrow_move_constructible_v<T>)
      : value_(std::move(value)) {}

  constexpr auto value() const noexcept -> const T& { return value_; }

  friend constexpr auto operator==(const StrongValue&, const StrongValue&)
      -> bool = default;
  friend constexpr auto operator<=>(const StrongValue&,
                                    const StrongValue&) = default;

 private:
  T value_;
};

}
