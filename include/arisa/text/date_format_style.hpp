#pragma once

#include <arisa/text/locale.hpp>
#include <arisa/text/locale_data.hpp>

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace arisa::text {

class DateFormatStyle {
 public:
  enum class DateStyle : std::uint8_t {
    omitted,
    numeric,
    abbreviated,
    long_form,
    complete,
  };

  enum class TimeStyle : std::uint8_t {
    omitted,
    shortened,
    standard,
    complete,
  };

  constexpr DateFormatStyle() noexcept = default;

  constexpr DateFormatStyle(DateStyle date, TimeStyle time) noexcept
      : date_(date), time_(time) {}

  static auto pattern(std::string pattern) -> DateFormatStyle;

  constexpr auto date() const noexcept -> DateStyle { return date_; }

  constexpr auto time() const noexcept -> TimeStyle { return time_; }

  auto format(std::chrono::sys_seconds instant, std::chrono::seconds offset,
              const Locale& locale) const -> std::string;

  auto format(std::chrono::sys_seconds instant, const Locale& locale) const
      -> std::string;

  auto parse(std::string_view text, const Locale& locale) const
      -> std::optional<std::chrono::sys_seconds>;

  auto resolved_pattern(const Locale& locale) const -> std::string;

  static auto expand(std::string_view pattern, std::chrono::sys_seconds instant,
                     std::chrono::seconds offset, LocaleData data)
      -> std::string;

  friend auto operator==(const DateFormatStyle&, const DateFormatStyle&)
      -> bool = default;

 private:
  struct Fields {
    std::chrono::year_month_day day;
    std::chrono::weekday weekday;
    std::chrono::hh_mm_ss<std::chrono::seconds> clock;
    std::chrono::seconds offset;
  };

  static auto field(char letter, std::size_t count, const Fields& fields,
                    LocaleData data) -> std::string;

  static auto zone(std::chrono::seconds offset, bool long_form) -> std::string;

  static auto padded(long long value, std::size_t width) -> std::string;

  struct Parsed {
    int year{1970};
    unsigned month{1};
    unsigned day{1};
    int hour{0};
    int minute{0};
    int second{0};
    std::optional<bool> after_noon{};
  };

  static auto spacing(std::string_view text, std::size_t at) noexcept
      -> std::size_t;

  static auto digits(std::string_view text, std::size_t& at, std::size_t limit)
      -> std::optional<int>;

  static auto word(std::string_view text, std::size_t& at,
                   std::string_view candidate) -> bool;

  static auto read(char letter, std::size_t count, std::string_view text,
                   std::size_t& at, LocaleData data, Parsed& parsed) -> bool;

  DateStyle date_{DateStyle::numeric};
  TimeStyle time_{TimeStyle::shortened};
  std::optional<std::string> pattern_;
};

class RelativeDateTimeStyle {
 public:
  static auto format(std::chrono::seconds offset, const Locale& locale)
      -> std::string;

  static auto unit(std::chrono::seconds offset) noexcept
      -> std::pair<LocaleData::Unit, long long>;
};

}
