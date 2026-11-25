#pragma once

#include <arisa/text/locale.hpp>

#include <chrono>
#include <optional>
#include <string>

namespace arisa::components {

struct DateInterval {
  std::chrono::system_clock::time_point start{};
  std::chrono::system_clock::time_point end{};

  auto duration() const noexcept -> std::chrono::system_clock::duration {
    return end - start;
  }

  friend constexpr auto operator==(const DateInterval&, const DateInterval&)
      -> bool = default;
};

class DateIntervalFormatStyle {
 public:
  using Date = std::chrono::system_clock::time_point;

  static constexpr std::string_view separator = " – ";

  explicit DateIntervalFormatStyle(bool shows_time = true) noexcept
      : shows_time_(shows_time) {}

  auto format(const DateInterval& interval, const text::Locale& locale,
              std::chrono::seconds zone = std::chrono::seconds{0}) const
      -> std::string;

  auto discrete_input(const DateInterval& after) const
      -> std::optional<DateInterval>;

  friend auto operator==(const DateIntervalFormatStyle&,
                         const DateIntervalFormatStyle&) -> bool = default;

 private:
  bool shows_time_;
};

class DurationFormatStyle {
 public:
  using Duration = std::chrono::duration<double>;

  enum class Pattern : std::uint8_t {
    hour_minute_second,
    minute_second,
    units_wide,
    units_abbreviated,
  };

  explicit DurationFormatStyle(Pattern pattern = Pattern::hour_minute_second,
                               std::size_t max_field_count = 2) noexcept
      : pattern_(pattern), count_(max_field_count == 0 ? 1 : max_field_count) {}

  auto format(Duration duration, const text::Locale& locale) const
      -> std::string;

  auto discrete_input(Duration after) const -> std::optional<Duration>;

  friend auto operator==(const DurationFormatStyle&, const DurationFormatStyle&)
      -> bool = default;

 private:
  auto unit(Duration duration) const -> Duration;

  Pattern pattern_;
  std::size_t count_;
};

}
