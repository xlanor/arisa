#include <arisa/components/timeline_schedule.hpp>

#include <algorithm>
#include <iterator>
#include <utility>

namespace arisa::components {

auto TimelineSchedule::periodic(Date start, std::chrono::nanoseconds interval)
    -> TimelineSchedule {
  TimelineSchedule schedule{Kind::periodic};
  schedule.start_ = start;
  schedule.interval_ = std::max(interval, std::chrono::nanoseconds{1});
  return schedule;
}

auto TimelineSchedule::every_minute() -> TimelineSchedule {
  return periodic(Date{}, std::chrono::minutes{1});
}

auto TimelineSchedule::animation(std::chrono::nanoseconds minimum_interval,
                                 bool paused) -> TimelineSchedule {
  TimelineSchedule schedule{Kind::animation};
  schedule.interval_ = std::max(minimum_interval, std::chrono::nanoseconds{0});
  schedule.paused_ = paused;
  return schedule;
}

auto TimelineSchedule::explicit_dates(std::vector<Date> dates)
    -> TimelineSchedule {
  std::ranges::sort(dates);
  TimelineSchedule schedule{Kind::dates};
  schedule.dates_ = std::make_shared<const std::vector<Date>>(std::move(dates));
  return schedule;
}

auto TimelineSchedule::custom(Next next) -> TimelineSchedule {
  TimelineSchedule schedule{Kind::custom};
  schedule.custom_ = std::make_shared<const Next>(std::move(next));
  return schedule;
}

auto TimelineSchedule::entry(Date now) const -> Date {
  switch (kind_) {
    case Kind::periodic: {
      if (now <= start_) {
        return start_;
      }
      const auto laps = (now - start_) / interval_;
      return start_ +
             std::chrono::duration_cast<Date::duration>(interval_ * laps);
    }
    case Kind::animation:
    case Kind::custom:
      return now;
    case Kind::dates: {
      const auto after = std::ranges::upper_bound(*dates_, now);
      if (after == dates_->begin()) {
        return dates_->empty() ? now : dates_->front();
      }
      return *std::prev(after);
    }
  }
  return now;
}

auto TimelineSchedule::next(Date after) const -> std::optional<Date> {
  switch (kind_) {
    case Kind::periodic:
      if (after < start_) {
        return start_;
      }
      return entry(after) +
             std::chrono::duration_cast<Date::duration>(interval_);
    case Kind::animation:
      if (paused_) {
        return std::nullopt;
      }
      return after + std::chrono::duration_cast<Date::duration>(interval_);
    case Kind::dates: {
      const auto found = std::ranges::upper_bound(*dates_, after);
      if (found == dates_->end()) {
        return std::nullopt;
      }
      return *found;
    }
    case Kind::custom:
      return (*custom_)(after);
  }
  return std::nullopt;
}

auto TimelineSchedule::next(Date after, TimelineScheduleMode mode) const
    -> std::optional<Date> {
  const std::optional<Date> wanted = next(after);
  if (mode == TimelineScheduleMode::normal || !wanted.has_value() ||
      kind_ == Kind::dates || kind_ == Kind::custom) {
    return wanted;
  }
  return std::max(*wanted, after + std::chrono::duration_cast<Date::duration>(
                                       low_frequency_interval));
}

auto operator==(const TimelineSchedule& left, const TimelineSchedule& right)
    -> bool {
  return left.kind_ == right.kind_ && left.start_ == right.start_ &&
         left.interval_ == right.interval_ && left.paused_ == right.paused_ &&
         left.dates_ == right.dates_ && left.custom_ == right.custom_;
}

}
