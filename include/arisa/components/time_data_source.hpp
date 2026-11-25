#pragma once

#include <arisa/components/date_interval.hpp>
#include <arisa/components/system_format_style.hpp>

#include <chrono>
#include <concepts>
#include <cstdint>
#include <optional>

namespace arisa::components {

template <class Value>
class TimeDataSource {
 public:
  using Date = std::chrono::system_clock::time_point;
  using Duration = std::chrono::duration<double>;

  static auto current_date() -> TimeDataSource
    requires std::same_as<Value, Date>
  {
    return TimeDataSource{Kind::current, Date{}};
  }

  static auto duration_offset(Date to) -> TimeDataSource
    requires std::same_as<Value, Duration>
  {
    return TimeDataSource{Kind::offset, to};
  }

  static auto date_range_starting_at(Date date) -> TimeDataSource
    requires std::same_as<Value, DateInterval>
  {
    return TimeDataSource{Kind::starting, date};
  }

  static auto date_range_ending_at(Date date) -> TimeDataSource
    requires std::same_as<Value, DateInterval>
  {
    return TimeDataSource{Kind::ending, date};
  }

  auto value(Date now) const -> Value {
    if constexpr (std::same_as<Value, Date>) {
      return now;
    } else if constexpr (std::same_as<Value, Duration>) {
      return std::chrono::duration_cast<Duration>(now - anchor_);
    } else {
      return kind_ == Kind::starting
                 ? DateInterval{.start = anchor_, .end = now}
                 : DateInterval{.start = now, .end = anchor_};
    }
  }

  auto date(const Value& value) const -> Date {
    if constexpr (std::same_as<Value, Date>) {
      return value;
    } else if constexpr (std::same_as<Value, Duration>) {
      return anchor_ + std::chrono::duration_cast<Date::duration>(value);
    } else {
      return kind_ == Kind::starting ? value.end : value.start;
    }
  }

  auto anchor() const noexcept -> Date { return anchor_; }

  friend auto operator==(const TimeDataSource&, const TimeDataSource&)
      -> bool = default;

 private:
  enum class Kind : std::uint8_t { current, offset, starting, ending };

  TimeDataSource(Kind kind, Date anchor) noexcept
      : kind_(kind), anchor_(anchor) {}

  Kind kind_;
  Date anchor_;
};

}
