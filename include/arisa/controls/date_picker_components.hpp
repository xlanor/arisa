#pragma once

#include <cstdint>

namespace arisa::controls {

class DatePickerComponents {
 public:
  static const DatePickerComponents date;
  static const DatePickerComponents hour_and_minute;

  constexpr DatePickerComponents() noexcept = default;

  constexpr auto contains(DatePickerComponents other) const noexcept -> bool {
    return (bits_ & other.bits_) == other.bits_ && other.bits_ != 0U;
  }

  friend constexpr auto operator|(DatePickerComponents left,
                                  DatePickerComponents right) noexcept
      -> DatePickerComponents {
    return DatePickerComponents{
        static_cast<std::uint8_t>(left.bits_ | right.bits_)};
  }

  friend constexpr auto operator==(DatePickerComponents, DatePickerComponents)
      -> bool = default;

 private:
  constexpr explicit DatePickerComponents(std::uint8_t bits) noexcept
      : bits_(bits) {}

  std::uint8_t bits_{0};
};

inline constexpr DatePickerComponents DatePickerComponents::date{1U};
inline constexpr DatePickerComponents DatePickerComponents::hour_and_minute{2U};

}
