#pragma once

#include <arisa/components/calendar.hpp>
#include <arisa/components/date_interval.hpp>
#include <arisa/text/locale.hpp>
#include <arisa/text/locale_data.hpp>

#include <chrono>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::components {

class SystemFormatStyle {
 public:
  using Date = std::chrono::system_clock::time_point;
  using Duration = std::chrono::duration<double>;

  enum class Field : std::uint8_t {
    year = 1U << 0U,
    month = 1U << 1U,
    week = 1U << 2U,
    day = 1U << 3U,
    hour = 1U << 4U,
    minute = 1U << 5U,
    second = 1U << 6U,
  };

  class Fields {
   public:
    static const Fields all;
    static const Fields relative;

    constexpr Fields(Field field) noexcept
        : bits_(static_cast<std::uint8_t>(field)) {}

    constexpr auto contains(Field field) const noexcept -> bool {
      return (bits_ & static_cast<std::uint8_t>(field)) != 0;
    }

    friend constexpr auto operator|(Fields left, Fields right) noexcept
        -> Fields {
      return Fields{static_cast<std::uint8_t>(left.bits_ | right.bits_)};
    }

    friend constexpr auto operator==(Fields, Fields) -> bool = default;

   private:
    constexpr explicit Fields(std::uint8_t bits) noexcept : bits_(bits) {}

    std::uint8_t bits_;
  };

  enum class Sign : std::uint8_t {
    automatic,
    never,
    always,
    always_including_zero,
  };

  struct Part {
    text::LocaleData::Unit unit{text::LocaleData::Unit::second};
    std::int64_t value{0};

    friend constexpr auto operator==(const Part&, const Part&)
        -> bool = default;
  };

  class DateOffset;
  class DateReference;
  class Stopwatch;
  class Timer;

  static constexpr std::string_view minus = "−";
  static constexpr std::chrono::nanoseconds step{1};

  static auto phrase(std::span<const Part> parts, const text::Locale& locale,
                     bool abbreviated) -> std::string;

  static auto clock(Duration elapsed, bool hours_shown, bool padded_minutes,
                    bool padded_hours, Duration precision) -> std::string;

  static auto parts(Date earlier, Date later, Fields allowed, std::size_t count)
      -> std::vector<Part>;

  using Unit = text::LocaleData::Unit;

  static auto unit_length(Unit unit) noexcept -> std::chrono::seconds;

 private:
  static auto field_of(Unit unit) noexcept -> Field;
};

inline constexpr SystemFormatStyle::Fields SystemFormatStyle::Fields::all{
    static_cast<std::uint8_t>(0x7FU)};
inline constexpr SystemFormatStyle::Fields SystemFormatStyle::Fields::relative{
    static_cast<std::uint8_t>(0x3FU)};

class SystemFormatStyle::DateOffset {
 public:
  explicit DateOffset(Date anchor, Fields allowed = Fields::all,
                      std::size_t max_field_count = 2,
                      Sign sign = Sign::automatic) noexcept
      : anchor_(anchor),
        allowed_(allowed),
        count_(max_field_count == 0 ? 1 : max_field_count),
        sign_(sign) {}

  auto locale(text::Locale value) const -> DateOffset;

  auto calendar(Calendar value) const -> DateOffset;

  auto format(Date now) const -> std::string;

  auto format(Date now, const text::Locale& locale) const -> std::string;

  auto discrete_input(Date after) const -> std::optional<Date>;

  auto discrete_input_before(Date before) const -> std::optional<Date>;

  auto input(Date after) const -> std::optional<Date>;

  auto input_before(Date before) const -> std::optional<Date>;

  auto anchor() const noexcept -> Date { return anchor_; }

  friend auto operator==(const DateOffset&, const DateOffset&)
      -> bool = default;

 private:
  auto smallest(Date now) const -> std::chrono::seconds;

  Date anchor_;
  Fields allowed_;
  std::size_t count_;
  Sign sign_;
  std::optional<text::Locale> locale_;
  Calendar calendar_{};
};

class SystemFormatStyle::DateReference {
 public:
  static constexpr std::chrono::seconds instant{30};
  static constexpr std::chrono::milliseconds second_rounding{500};
  static constexpr std::int64_t days_shown = 7;

  explicit DateReference(Date anchor, Fields allowed = Fields::relative,
                         std::size_t max_field_count = 1,
                         Field threshold = Field::week) noexcept
      : anchor_(anchor),
        allowed_(allowed),
        count_(max_field_count == 0 ? 1 : max_field_count),
        threshold_(threshold) {}

  auto locale(text::Locale value) const -> DateReference;

  auto calendar(Calendar value) const -> DateReference;

  auto format(Date now) const -> std::string;

  auto format(Date now, const text::Locale& locale) const -> std::string;

  auto discrete_input(Date after) const -> std::optional<Date>;

  auto discrete_input_before(Date before) const -> std::optional<Date>;

  auto input(Date after) const -> std::optional<Date>;

  auto input_before(Date before) const -> std::optional<Date>;

  friend auto operator==(const DateReference&, const DateReference&)
      -> bool = default;

 private:
  auto boundaries(Date now) const -> std::vector<Date>;

  static auto without_year(std::string_view pattern) -> std::string;

  Date anchor_;
  Fields allowed_;
  std::size_t count_;
  Field threshold_;
  std::optional<text::Locale> locale_;
  Calendar calendar_{};
};

class SystemFormatStyle::Stopwatch {
 public:
  explicit Stopwatch(Date start, bool shows_hours = true,
                     std::size_t max_field_count = 4,
                     Duration max_precision = Duration{0.01}) noexcept
      : start_(start),
        shows_hours_(shows_hours),
        count_(max_field_count == 0 ? 1 : max_field_count),
        precision_(max_precision) {}

  auto locale(text::Locale value) const -> Stopwatch;

  auto format(Date now) const -> std::string;

  auto format(Date now, const text::Locale& locale) const -> std::string;

  auto discrete_input(Date after) const -> std::optional<Date>;

  auto discrete_input_before(Date before) const -> std::optional<Date>;

  friend auto operator==(const Stopwatch&, const Stopwatch&) -> bool = default;

 private:
  Date start_;
  bool shows_hours_;
  std::size_t count_;
  Duration precision_;
  std::optional<text::Locale> locale_;
};

class SystemFormatStyle::Timer {
 public:
  using Interval = DateInterval;

  static auto counting_down(Interval range, bool shows_hours = true,
                            std::size_t max_field_count = 3,
                            Duration max_precision = Duration{1.0}) -> Timer;

  static auto counting_up(Interval range, bool shows_hours = true,
                          std::size_t max_field_count = 3,
                          Duration max_precision = Duration{1.0}) -> Timer;

  auto locale(text::Locale value) const -> Timer;

  auto format(Date now) const -> std::string;

  auto format(Date now, const text::Locale& locale) const -> std::string;

  auto discrete_input(Date after) const -> std::optional<Date>;

  auto discrete_input_before(Date before) const -> std::optional<Date>;

  auto shown(Date now) const -> Duration;

  auto counts_down() const noexcept -> bool { return down_; }

  auto range() const noexcept -> const Interval& { return range_; }

  friend auto operator==(const Timer&, const Timer&) -> bool = default;

 private:
  Timer(Interval range, bool down, bool shows_hours, std::size_t count,
        Duration precision) noexcept
      : range_(range),
        down_(down),
        shows_hours_(shows_hours),
        count_(count == 0 ? 1 : count),
        precision_(precision) {}

  Interval range_;
  bool down_;
  bool shows_hours_;
  std::size_t count_;
  Duration precision_;
  std::optional<text::Locale> locale_;
};

}
