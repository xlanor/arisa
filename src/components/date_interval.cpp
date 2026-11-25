#include <arisa/components/date_interval.hpp>
#include <arisa/components/system_format_style.hpp>
#include <arisa/text/date_format_style.hpp>

#include <cmath>
#include <vector>

namespace arisa::components {

auto DateIntervalFormatStyle::format(const DateInterval& interval,
                                     const text::Locale& locale,
                                     std::chrono::seconds zone) const
    -> std::string {
  using namespace std::chrono;
  using Style = text::DateFormatStyle;
  const auto first = floor<seconds>(interval.start);
  const auto last = floor<seconds>(interval.end);
  const bool same_day = floor<days>(first + zone) == floor<days>(last + zone);
  const Style both{
      Style::DateStyle::abbreviated,
      shows_time_ ? Style::TimeStyle::shortened : Style::TimeStyle::omitted};
  const Style dated{Style::DateStyle::abbreviated, Style::TimeStyle::omitted};
  const Style timed{Style::DateStyle::omitted, Style::TimeStyle::shortened};
  if (same_day && shows_time_) {
    return both.format(first, zone, locale) + std::string{separator} +
           timed.format(last, zone, locale);
  }
  if (same_day) {
    return dated.format(first, zone, locale);
  }
  return both.format(first, zone, locale) + std::string{separator} +
         both.format(last, zone, locale);
}

auto DateIntervalFormatStyle::discrete_input(const DateInterval& after) const
    -> std::optional<DateInterval> {
  using namespace std::chrono;
  const auto unit = shows_time_
                        ? duration_cast<system_clock::duration>(minutes{1})
                        : duration_cast<system_clock::duration>(days{1});
  DateInterval next = after;
  next.end = (floor<minutes>(after.end) + unit);
  next.start = (floor<minutes>(after.start) + unit);
  return next;
}

auto DurationFormatStyle::unit(Duration duration) const -> Duration {
  const double magnitude = std::fabs(duration.count());
  switch (pattern_) {
    case Pattern::hour_minute_second:
    case Pattern::minute_second:
      return Duration{1.0};
    case Pattern::units_wide:
    case Pattern::units_abbreviated:
      break;
  }
  if (count_ >= 3 || magnitude < 3600.0) {
    return Duration{count_ >= 2 || magnitude < 60.0 ? 1.0 : 60.0};
  }
  return Duration{count_ >= 2 ? 60.0 : 3600.0};
}

auto DurationFormatStyle::format(Duration duration,
                                 const text::Locale& locale) const
    -> std::string {
  using Part = SystemFormatStyle::Part;
  using Unit = text::LocaleData::Unit;
  const double magnitude = std::fabs(duration.count());
  const std::string sign = duration.count() < -0.5 ? "-" : "";
  switch (pattern_) {
    case Pattern::hour_minute_second:
      return sign + SystemFormatStyle::clock(Duration{magnitude}, true, false,
                                             false, Duration{1.0});
    case Pattern::minute_second:
      return sign + SystemFormatStyle::clock(Duration{magnitude}, false, false,
                                             false, Duration{1.0});
    case Pattern::units_wide:
    case Pattern::units_abbreviated:
      break;
  }
  const auto whole = static_cast<std::int64_t>(std::floor(magnitude));
  std::vector<Part> all{
      Part{.unit = Unit::hour, .value = whole / 3600},
      Part{.unit = Unit::minute, .value = (whole % 3600) / 60},
      Part{.unit = Unit::second, .value = whole % 60}};
  std::vector<Part> shown;
  for (const Part& part : all) {
    if ((part.value != 0 || !shown.empty()) && shown.size() < count_) {
      shown.push_back(part);
    }
  }
  if (shown.empty()) {
    shown.push_back(Part{.unit = Unit::second, .value = 0});
  }
  return sign + SystemFormatStyle::phrase(
                    shown, locale, pattern_ == Pattern::units_abbreviated);
}

auto DurationFormatStyle::discrete_input(Duration after) const
    -> std::optional<Duration> {
  const double step = unit(after).count();
  return Duration{(std::floor(after.count() / step) + 1.0) * step};
}

}
