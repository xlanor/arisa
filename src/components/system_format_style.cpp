#include <arisa/components/system_format_style.hpp>
#include <arisa/text/date_format_style.hpp>
#include <arisa/text/number_format_style.hpp>
#include <arisa/text/plural_rules.hpp>

#include <algorithm>
#include <cmath>
#include <format>
#include <utility>

namespace arisa::components {

auto SystemFormatStyle::unit_length(Unit unit) noexcept
    -> std::chrono::seconds {
  switch (unit) {
    case Unit::second:
      return std::chrono::seconds{1};
    case Unit::minute:
      return std::chrono::seconds{60};
    case Unit::hour:
      return std::chrono::seconds{3600};
    case Unit::day:
      return std::chrono::seconds{86400};
    case Unit::week:
      return std::chrono::seconds{604800};
    case Unit::month:
      return std::chrono::seconds{2629746};
    case Unit::year:
      return std::chrono::seconds{31556952};
  }
  return std::chrono::seconds{1};
}

auto SystemFormatStyle::field_of(Unit unit) noexcept -> Field {
  switch (unit) {
    case Unit::second:
      return Field::second;
    case Unit::minute:
      return Field::minute;
    case Unit::hour:
      return Field::hour;
    case Unit::day:
      return Field::day;
    case Unit::week:
      return Field::week;
    case Unit::month:
      return Field::month;
    case Unit::year:
      return Field::year;
  }
  return Field::second;
}

auto SystemFormatStyle::phrase(std::span<const Part> parts,
                               const text::Locale& locale, bool abbreviated)
    -> std::string {
  const text::LocaleData data = text::LocaleData::of(locale);
  const text::PluralRules rules{locale};
  std::string joined;
  for (const Part& part : parts) {
    std::string piece{data.duration_pattern(part.unit, abbreviated,
                                            rules.category(part.value))};
    if (const std::size_t at = piece.find("{0}"); at != std::string::npos) {
      piece.replace(
          at, 3, text::NumberFormatStyle::number().format(part.value, locale));
    }
    if (joined.empty()) {
      joined = std::move(piece);
      continue;
    }
    std::string pair{data.pair_pattern()};
    if (const std::size_t at = pair.find("{0}"); at != std::string::npos) {
      pair.replace(at, 3, joined);
    }
    if (const std::size_t at = pair.find("{1}"); at != std::string::npos) {
      pair.replace(at, 3, piece);
    }
    joined = std::move(pair);
  }
  return joined;
}

auto SystemFormatStyle::clock(Duration elapsed, bool hours_shown,
                              bool padded_minutes, bool padded_hours,
                              Duration precision) -> std::string {
  const double total = std::max(elapsed.count(), 0.0);
  const double granule = std::max(precision.count(), 0.001);
  const auto decimals = static_cast<int>(
      std::clamp(std::ceil(-std::log10(granule) - 1e-9), 0.0, 3.0));
  const auto whole = static_cast<std::int64_t>(std::floor(total + 1e-9));
  const std::int64_t hours = hours_shown ? whole / 3600 : 0;
  const std::int64_t minutes = hours_shown ? (whole % 3600) / 60 : whole / 60;
  const double seconds =
      total - static_cast<double>((hours * 3600) + (minutes * 60));
  std::string text;
  if (hours_shown && hours > 0) {
    text = padded_hours ? std::format("{:02}:{:02}:", hours, minutes)
                        : std::format("{}:{:02}:", hours, minutes);
  } else {
    text = padded_minutes ? std::format("{:02}:", minutes)
                          : std::format("{}:", minutes);
  }
  if (decimals == 0) {
    text += std::format("{:02}",
                        static_cast<std::int64_t>(std::floor(seconds + 1e-9)));
  } else {
    const double scale = std::pow(10.0, decimals);
    const double truncated = std::floor((seconds * scale) + 1e-6) / scale;
    text += std::format("{:0{}.{}f}", truncated, decimals + 3, decimals);
  }
  return text;
}

auto SystemFormatStyle::parts(Date earlier, Date later, Fields allowed,
                              std::size_t count) -> std::vector<Part> {
  using namespace std::chrono;
  const auto start = floor<seconds>(earlier);
  const auto end = floor<seconds>(later);
  const year_month_day first{floor<days>(start)};
  const year_month_day last{floor<days>(end)};
  std::int64_t months =
      ((static_cast<int>(last.year()) - static_cast<int>(first.year())) * 12) +
      (static_cast<int>(static_cast<unsigned>(last.month())) -
       static_cast<int>(static_cast<unsigned>(first.month())));
  const auto within = [&](std::int64_t count_months) {
    const year_month_day moved =
        year_month_day{first.year(), first.month(), first.day()} +
        std::chrono::months{count_months};
    const auto day =
        moved.ok() ? sys_days{moved}
                   : sys_days{moved.year() / moved.month() / std::chrono::last};
    return day + (start - floor<days>(start));
  };
  while (months > 0 && within(months) > end) {
    --months;
  }
  std::int64_t years = allowed.contains(Field::year) ? months / 12 : 0;
  std::int64_t remaining_months =
      allowed.contains(Field::month) ? months - (years * 12) : 0;
  if (!allowed.contains(Field::year) && allowed.contains(Field::month)) {
    remaining_months = months;
  }
  const std::int64_t used_months = (years * 12) + remaining_months;
  auto rest = end - within(used_months);
  std::vector<Part> all;
  all.push_back(Part{.unit = Unit::year, .value = years});
  all.push_back(Part{.unit = Unit::month, .value = remaining_months});
  const auto take = [&](Unit unit) {
    if (!allowed.contains(field_of(unit))) {
      all.push_back(Part{.unit = unit, .value = 0});
      return;
    }
    const seconds length = unit_length(unit);
    const std::int64_t value = rest / length;
    rest -= value * length;
    all.push_back(Part{.unit = unit, .value = value});
  };
  take(Unit::week);
  take(Unit::day);
  take(Unit::hour);
  take(Unit::minute);
  take(Unit::second);
  std::vector<Part> chosen;
  const auto shown = [&](const Part& part) {
    return allowed.contains(field_of(part.unit));
  };
  auto first_nonzero = std::ranges::find_if(
      all, [&](const Part& part) { return shown(part) && part.value != 0; });
  if (first_nonzero == all.end()) {
    auto smallest = std::ranges::find_if(all.rbegin(), all.rend(), shown);
    if (smallest != all.rend()) {
      chosen.push_back(Part{.unit = smallest->unit, .value = 0});
    }
    return chosen;
  }
  for (auto at = first_nonzero; at != all.end() && chosen.size() < count;
       ++at) {
    if (shown(*at)) {
      chosen.push_back(*at);
    }
  }
  return chosen;
}

auto SystemFormatStyle::DateOffset::locale(text::Locale value) const
    -> DateOffset {
  DateOffset changed = *this;
  changed.locale_ = std::move(value);
  return changed;
}

auto SystemFormatStyle::DateOffset::calendar(Calendar value) const
    -> DateOffset {
  DateOffset changed = *this;
  changed.calendar_ = value;
  return changed;
}

auto SystemFormatStyle::DateOffset::format(Date now) const -> std::string {
  return format(now, locale_.value_or(text::Locale::undetermined()));
}

auto SystemFormatStyle::DateOffset::format(Date now,
                                           const text::Locale& locale) const
    -> std::string {
  const text::Locale& used = locale_.has_value() ? *locale_ : locale;
  const bool future = anchor_ > now;
  const std::vector<Part> shown = future
                                      ? parts(now, anchor_, allowed_, count_)
                                      : parts(anchor_, now, allowed_, count_);
  std::string text = phrase(shown, used, false);
  const bool zero = std::ranges::all_of(
      shown, [](const Part& part) { return part.value == 0; });
  if (future && !zero && sign_ != Sign::never) {
    text.insert(0, minus);
  } else if (!future && sign_ != Sign::never && sign_ != Sign::automatic &&
             (!zero || sign_ == Sign::always_including_zero)) {
    text.insert(0, "+");
  }
  return text;
}

auto SystemFormatStyle::DateOffset::smallest(Date now) const
    -> std::chrono::seconds {
  const bool future = anchor_ > now;
  const std::vector<Part> shown = future
                                      ? parts(now, anchor_, allowed_, count_)
                                      : parts(anchor_, now, allowed_, count_);
  return shown.empty() ? std::chrono::seconds{1}
                       : unit_length(shown.back().unit);
}

auto SystemFormatStyle::DateOffset::discrete_input(Date after) const
    -> std::optional<Date> {
  const std::chrono::seconds unit = smallest(after);
  if (anchor_ <= after) {
    const auto elapsed = after - anchor_;
    const auto steps = elapsed / unit;
    return anchor_ + ((steps + 1) * unit);
  }
  const auto remaining = anchor_ - after;
  const auto steps = remaining / unit;
  const Date boundary = anchor_ - (steps * unit);
  return boundary > after ? boundary : anchor_ - ((steps - 1) * unit);
}

auto SystemFormatStyle::DateOffset::discrete_input_before(Date before) const
    -> std::optional<Date> {
  const std::chrono::seconds unit = smallest(before);
  if (anchor_ <= before) {
    const auto elapsed = before - anchor_;
    const auto steps = elapsed / unit;
    return anchor_ + (steps * unit) - step;
  }
  const auto remaining = anchor_ - before;
  const auto steps = remaining / unit;
  return anchor_ - ((steps + 1) * unit);
}

auto SystemFormatStyle::DateOffset::input(Date after) const
    -> std::optional<Date> {
  return after + step;
}

auto SystemFormatStyle::DateOffset::input_before(Date before) const
    -> std::optional<Date> {
  return before - step;
}

auto SystemFormatStyle::DateReference::locale(text::Locale value) const
    -> DateReference {
  DateReference changed = *this;
  changed.locale_ = std::move(value);
  return changed;
}

auto SystemFormatStyle::DateReference::calendar(Calendar value) const
    -> DateReference {
  DateReference changed = *this;
  changed.calendar_ = value;
  return changed;
}

auto SystemFormatStyle::DateReference::format(Date now) const -> std::string {
  return format(now, locale_.value_or(text::Locale::undetermined()));
}

auto SystemFormatStyle::DateReference::without_year(std::string_view pattern)
    -> std::string {
  std::string result{pattern};
  const std::size_t first = result.find('y');
  if (first == std::string::npos) {
    return result;
  }
  std::size_t last = first;
  while (last < result.size() && result[last] == 'y') {
    ++last;
  }
  std::size_t from = first;
  while (from > 0 && (result[from - 1] == ' ' || result[from - 1] == ',')) {
    --from;
  }
  if (from == 0) {
    while (
        last < result.size() &&
        (result[last] == ' ' || result[last] == ',' || result[last] == '.')) {
      ++last;
    }
  }
  result.erase(from, last - from);
  return result;
}

auto SystemFormatStyle::DateReference::format(Date now,
                                              const text::Locale& locale) const
    -> std::string {
  using namespace std::chrono;
  const text::Locale& used = locale_.has_value() ? *locale_ : locale;
  const text::LocaleData data = text::LocaleData::of(used);
  const auto exact = anchor_ - now;
  const auto magnitude = floor<seconds>(
      (exact < Date::duration::zero() ? -exact : exact) + second_rounding);
  const seconds offset =
      exact < Date::duration::zero() ? -magnitude : magnitude;
  if (magnitude < instant) {
    return std::string{data.named_day(0)};
  }
  const auto rounded = [&](Unit unit) {
    const auto length = unit_length(unit);
    return (magnitude + (length / 2)) / length;
  };
  const seconds sign_of{offset < seconds{0} ? -1 : 1};
  if (allowed_.contains(Field::minute) && rounded(Unit::minute) < 60) {
    return text::RelativeDateTimeStyle::format(
        sign_of * rounded(Unit::minute) * 60, used);
  }
  if (allowed_.contains(Field::hour) && rounded(Unit::hour) < 24) {
    return text::RelativeDateTimeStyle::format(
        sign_of * rounded(Unit::hour) * 3600, used);
  }
  const std::int64_t difference =
      (floor<days>(anchor_) - floor<days>(now)).count();
  const std::int64_t calendar_days = difference < 0 ? -difference : difference;
  if (allowed_.contains(Field::day) && calendar_days <= days_shown &&
      threshold_ != Field::day) {
    if (calendar_days <= 1) {
      return std::string{data.named_day(difference < 0 ? -1 : 1)};
    }
    return text::RelativeDateTimeStyle::format(
        sign_of * std::max<std::int64_t>(calendar_days, 1) * 86400, used);
  }
  const year_month_day day{floor<days>(anchor_)};
  const year_month_day current{floor<days>(now)};
  const std::string pattern =
      day.year() == current.year()
          ? without_year(data.date_pattern(text::LocaleData::Length::full))
          : std::string{data.month_year_pattern()};
  return text::DateFormatStyle::pattern(pattern).format(floor<seconds>(anchor_),
                                                        seconds{0}, used);
}

auto SystemFormatStyle::DateReference::boundaries(Date now) const
    -> std::vector<Date> {
  using namespace std::chrono;
  std::vector<Date> found;
  for (const Unit unit : {Unit::minute, Unit::hour}) {
    const seconds length = unit_length(unit);
    for (int sign : {-1, 1}) {
      const auto distance = duration_cast<seconds>(now - anchor_) * sign;
      const auto base = (distance - (length / 2)) / length;
      for (std::int64_t step_count = base - 1; step_count <= base + 2;
           ++step_count) {
        const auto edge = (step_count * length) + (length / 2);
        found.push_back(anchor_ + (sign * edge));
        found.push_back(anchor_ + (sign * (edge - second_rounding)));
      }
    }
  }
  found.push_back(anchor_ - instant);
  found.push_back(anchor_ + instant);
  found.push_back(anchor_ - instant + second_rounding);
  found.push_back(anchor_ + instant - second_rounding);
  const auto today = floor<days>(now);
  for (int day = -1; day <= 2; ++day) {
    found.push_back(today + days{day});
  }
  return found;
}

auto SystemFormatStyle::DateReference::discrete_input(Date after) const
    -> std::optional<Date> {
  const std::string current = format(after);
  std::vector<Date> candidates = boundaries(after);
  std::ranges::sort(candidates);
  for (const Date candidate : candidates) {
    if (candidate > after &&
        format(candidate + std::chrono::milliseconds{1}) != current) {
      return candidate;
    }
  }
  return std::nullopt;
}

auto SystemFormatStyle::DateReference::discrete_input_before(Date before) const
    -> std::optional<Date> {
  const std::string current = format(before);
  std::vector<Date> candidates = boundaries(before);
  std::ranges::sort(candidates, std::ranges::greater{});
  for (const Date candidate : candidates) {
    if (candidate < before &&
        format(candidate - std::chrono::milliseconds{1}) != current) {
      return candidate - std::chrono::milliseconds{1};
    }
  }
  return std::nullopt;
}

auto SystemFormatStyle::DateReference::input(Date after) const
    -> std::optional<Date> {
  return after + step;
}

auto SystemFormatStyle::DateReference::input_before(Date before) const
    -> std::optional<Date> {
  return before - step;
}

auto SystemFormatStyle::Stopwatch::locale(text::Locale value) const
    -> Stopwatch {
  Stopwatch changed = *this;
  changed.locale_ = std::move(value);
  return changed;
}

auto SystemFormatStyle::Stopwatch::format(Date now) const -> std::string {
  return format(now, locale_.value_or(text::Locale::undetermined()));
}

auto SystemFormatStyle::Stopwatch::format(Date now,
                                          const text::Locale& locale) const
    -> std::string {
  const Duration elapsed =
      now > start_ ? std::chrono::duration_cast<Duration>(now - start_)
                   : Duration{0.0};
  if (count_ < 3) {
    const std::vector<Part> shown = parts(
        start_, std::max(now, start_),
        Fields{Field::hour} | Fields{Field::minute} | Fields{Field::second},
        count_);
    return phrase(shown, locale_.has_value() ? *locale_ : locale, false);
  }
  return clock(elapsed, shows_hours_, true, true, precision_);
}

auto SystemFormatStyle::Stopwatch::discrete_input(Date after) const
    -> std::optional<Date> {
  const auto unit = std::chrono::duration_cast<std::chrono::nanoseconds>(
      Duration{std::max(precision_.count(), 0.001)});
  if (after < start_) {
    return start_ + unit;
  }
  const auto steps = (after - start_) / unit;
  return start_ + ((steps + 1) * unit);
}

auto SystemFormatStyle::Stopwatch::discrete_input_before(Date before) const
    -> std::optional<Date> {
  const auto unit = std::chrono::duration_cast<std::chrono::nanoseconds>(
      Duration{std::max(precision_.count(), 0.001)});
  if (before <= start_) {
    return std::nullopt;
  }
  const auto steps = (before - start_) / unit;
  return start_ + (steps * unit) - step;
}

auto SystemFormatStyle::Timer::counting_down(Interval range, bool shows_hours,
                                             std::size_t max_field_count,
                                             Duration max_precision) -> Timer {
  return Timer{range, true, shows_hours, max_field_count, max_precision};
}

auto SystemFormatStyle::Timer::counting_up(Interval range, bool shows_hours,
                                           std::size_t max_field_count,
                                           Duration max_precision) -> Timer {
  return Timer{range, false, shows_hours, max_field_count, max_precision};
}

auto SystemFormatStyle::Timer::locale(text::Locale value) const -> Timer {
  Timer changed = *this;
  changed.locale_ = std::move(value);
  return changed;
}

auto SystemFormatStyle::Timer::shown(Date now) const -> Duration {
  const Date clamped = std::clamp(now, range_.start, range_.end);
  const double step_size = std::max(precision_.count(), 0.001);
  if (down_) {
    const double remaining =
        std::chrono::duration_cast<Duration>(range_.end - clamped).count();
    return Duration{std::ceil((remaining / step_size) - 1e-9) * step_size};
  }
  const double elapsed =
      std::chrono::duration_cast<Duration>(clamped - range_.start).count();
  return Duration{std::floor((elapsed / step_size) + 1e-9) * step_size};
}

auto SystemFormatStyle::Timer::format(Date now) const -> std::string {
  return format(now, locale_.value_or(text::Locale::undetermined()));
}

auto SystemFormatStyle::Timer::format(Date now,
                                      const text::Locale& locale) const
    -> std::string {
  const Duration value = shown(now);
  if (count_ < 3) {
    const Date origin{};
    const auto length = std::chrono::duration_cast<std::chrono::seconds>(value);
    const std::vector<Part> pieces = parts(
        origin, origin + length,
        Fields{Field::hour} | Fields{Field::minute} | Fields{Field::second},
        count_);
    std::vector<Part> padded = pieces;
    if (padded.size() == 1 && count_ >= 2 &&
        padded.front().unit != Unit::second) {
      padded.push_back(Part{.unit = padded.front().unit == Unit::hour
                                        ? Unit::minute
                                        : Unit::second,
                            .value = 0});
    }
    return phrase(padded, locale_.has_value() ? *locale_ : locale, false);
  }
  return clock(value, shows_hours_, false, false, precision_);
}

auto SystemFormatStyle::Timer::discrete_input(Date after) const
    -> std::optional<Date> {
  const auto unit = std::chrono::duration_cast<std::chrono::nanoseconds>(
      Duration{std::max(precision_.count(), 0.001)});
  if (after < range_.start) {
    return range_.start;
  }
  if (after >= range_.end) {
    return std::nullopt;
  }
  if (down_) {
    const auto remaining = range_.end - after;
    const auto steps = remaining / unit;
    const Date boundary = range_.end - (steps * unit);
    return boundary > after ? boundary : range_.end - ((steps - 1) * unit);
  }
  const auto steps = (after - range_.start) / unit;
  return std::min(range_.start + ((steps + 1) * unit), range_.end);
}

auto SystemFormatStyle::Timer::discrete_input_before(Date before) const
    -> std::optional<Date> {
  const auto unit = std::chrono::duration_cast<std::chrono::nanoseconds>(
      Duration{std::max(precision_.count(), 0.001)});
  if (before <= range_.start) {
    return std::nullopt;
  }
  const Date clamped = std::min(before, range_.end);
  if (down_) {
    const auto remaining = range_.end - clamped;
    const auto steps = remaining / unit;
    return range_.end - ((steps + 1) * unit);
  }
  const auto steps = (clamped - range_.start) / unit;
  return range_.start + (steps * unit) - step;
}

}
