#include <arisa/controls/calendar_month.hpp>

namespace arisa::controls {

auto CalendarMonth::leading() const noexcept -> unsigned {
  const std::chrono::weekday opening{
      std::chrono::sys_days{month_ / std::chrono::day{1}}};
  return (opening - first_).count();
}

auto CalendarMonth::days() const noexcept -> unsigned {
  return static_cast<unsigned>(
      std::chrono::year_month_day_last{month_ / std::chrono::last}.day());
}

auto CalendarMonth::rows() const noexcept -> unsigned {
  return (leading() + days() + columns - 1U) / columns;
}

auto CalendarMonth::weekdays() const noexcept
    -> std::array<std::chrono::weekday, columns> {
  std::array<std::chrono::weekday, columns> ordered{};
  for (unsigned index = 0; index < columns; ++index) {
    ordered[index] = first_ + std::chrono::days{index};
  }
  return ordered;
}

auto CalendarMonth::day(unsigned cell) const noexcept
    -> std::optional<std::chrono::year_month_day> {
  const unsigned before = leading();
  if (cell < before || cell >= before + days()) {
    return std::nullopt;
  }
  return std::chrono::year_month_day{month_ /
                                     std::chrono::day{cell - before + 1U}};
}

auto CalendarMonth::previous() const noexcept -> CalendarMonth {
  return CalendarMonth{month_ - std::chrono::months{1}, first_};
}

auto CalendarMonth::next() const noexcept -> CalendarMonth {
  return CalendarMonth{month_ + std::chrono::months{1}, first_};
}

}
