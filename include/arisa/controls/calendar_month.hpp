#pragma once

#include <array>
#include <chrono>
#include <optional>

namespace arisa::controls {

class CalendarMonth {
 public:
  static constexpr unsigned columns = 7;

  CalendarMonth(std::chrono::year_month month,
                std::chrono::weekday first_weekday) noexcept
      : month_(month), first_(first_weekday) {}

  auto month() const noexcept -> std::chrono::year_month { return month_; }

  auto first_weekday() const noexcept -> std::chrono::weekday { return first_; }

  auto leading() const noexcept -> unsigned;

  auto days() const noexcept -> unsigned;

  auto rows() const noexcept -> unsigned;

  auto weekdays() const noexcept -> std::array<std::chrono::weekday, columns>;

  auto day(unsigned cell) const noexcept
      -> std::optional<std::chrono::year_month_day>;

  auto previous() const noexcept -> CalendarMonth;

  auto next() const noexcept -> CalendarMonth;

  friend auto operator==(const CalendarMonth&, const CalendarMonth&)
      -> bool = default;

 private:
  std::chrono::year_month month_;
  std::chrono::weekday first_;
};

}
