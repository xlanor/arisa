#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace arisa::components {

enum class TimelineScheduleMode : std::uint8_t {
  normal,
  low_frequency,
};

class TimelineSchedule {
 public:
  using Date = std::chrono::system_clock::time_point;
  using Next = std::function<std::optional<Date>(Date)>;

  static auto periodic(Date start, std::chrono::nanoseconds interval)
      -> TimelineSchedule;

  static auto every_minute() -> TimelineSchedule;

  static auto animation(
      std::chrono::nanoseconds minimum_interval = std::chrono::nanoseconds{0},
      bool paused = false) -> TimelineSchedule;

  static auto explicit_dates(std::vector<Date> dates) -> TimelineSchedule;

  static auto custom(Next next) -> TimelineSchedule;

  auto entry(Date now) const -> Date;

  auto next(Date after) const -> std::optional<Date>;

  auto next(Date after, TimelineScheduleMode mode) const -> std::optional<Date>;

  static constexpr std::chrono::seconds low_frequency_interval{60};

  auto continuous() const noexcept -> bool { return kind_ == Kind::animation; }

  auto paused() const noexcept -> bool { return paused_; }

  auto minimum_interval() const noexcept -> std::chrono::nanoseconds {
    return interval_;
  }

  friend auto operator==(const TimelineSchedule& left,
                         const TimelineSchedule& right) -> bool;

 private:
  enum class Kind : std::uint8_t { periodic, animation, dates, custom };

  explicit TimelineSchedule(Kind kind) noexcept : kind_(kind) {}

  Kind kind_;
  Date start_{};
  std::chrono::nanoseconds interval_{0};
  bool paused_{false};
  std::shared_ptr<const std::vector<Date>> dates_;
  std::shared_ptr<const Next> custom_;
};

}
