#pragma once

#include <arisa/text/locale.hpp>

#include <array>
#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>

namespace arisa::controls {

class DateFormat {
 public:
  enum class Field : std::uint8_t {
    year,
    month,
    day,
  };

  static auto order(const text::Locale& locale) noexcept
      -> std::array<Field, 3>;

  static auto uses_twelve_hour(const text::Locale& locale) noexcept -> bool;

  static auto month_name(std::chrono::month month, bool abbreviated) noexcept
      -> std::string_view;

  static auto month_name(std::chrono::month month, bool abbreviated,
                         const text::Locale& locale) noexcept
      -> std::string_view;

  static auto weekday_symbol(std::chrono::weekday weekday) noexcept
      -> std::string_view;

  static auto weekday_symbol(std::chrono::weekday weekday,
                             const text::Locale& locale) noexcept
      -> std::string_view;

  static auto date_text(std::chrono::year_month_day day,
                        const text::Locale& locale) -> std::string;

  static auto time_text(std::chrono::minutes since_midnight,
                        const text::Locale& locale) -> std::string;

  static auto month_title(std::chrono::year_month month) -> std::string;

  static auto month_title(std::chrono::year_month month,
                          const text::Locale& locale) -> std::string;
};

}
