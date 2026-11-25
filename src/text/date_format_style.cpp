#include <arisa/text/date_format_style.hpp>
#include <arisa/text/number_format_style.hpp>
#include <arisa/text/plural_rules.hpp>

#include <cstdlib>
#include <format>
#include <utility>

namespace arisa::text {

auto DateFormatStyle::pattern(std::string pattern) -> DateFormatStyle {
  DateFormatStyle style;
  style.pattern_ = std::move(pattern);
  return style;
}

auto DateFormatStyle::resolved_pattern(const Locale& locale) const
    -> std::string {
  if (pattern_.has_value()) {
    return *pattern_;
  }
  const LocaleData data = LocaleData::of(locale);
  std::string date;
  switch (date_) {
    case DateStyle::omitted:
      break;
    case DateStyle::numeric:
      date = std::string{data.numeric_date_pattern()};
      break;
    case DateStyle::abbreviated:
      date = std::string{data.abbreviated_date_pattern()};
      break;
    case DateStyle::long_form:
      date = std::string{data.date_pattern(LocaleData::Length::long_form)};
      break;
    case DateStyle::complete:
      date = std::string{data.date_pattern(LocaleData::Length::full)};
      break;
  }
  std::string time;
  switch (time_) {
    case TimeStyle::omitted:
      break;
    case TimeStyle::shortened:
      time = std::string{data.time_pattern(LocaleData::Length::short_form)};
      break;
    case TimeStyle::standard:
      time = std::string{data.time_pattern(LocaleData::Length::medium)};
      break;
    case TimeStyle::complete:
      time = std::string{data.time_pattern(LocaleData::Length::long_form)};
      break;
  }
  if (date.empty()) {
    return time;
  }
  if (time.empty()) {
    return date;
  }
  const LocaleData::Length length =
      date_ == DateStyle::numeric
          ? LocaleData::Length::short_form
          : static_cast<LocaleData::Length>(4U - std::to_underlying(date_));
  std::string joined{data.date_time_pattern(length)};
  if (const std::size_t at = joined.find("{1}"); at != std::string::npos) {
    joined.replace(at, 3, date);
  }
  if (const std::size_t at = joined.find("{0}"); at != std::string::npos) {
    joined.replace(at, 3, time);
  }
  return joined;
}

auto DateFormatStyle::format(std::chrono::sys_seconds instant,
                             std::chrono::seconds offset,
                             const Locale& locale) const -> std::string {
  return expand(resolved_pattern(locale), instant, offset,
                LocaleData::of(locale));
}

auto DateFormatStyle::padded(long long value, std::size_t width)
    -> std::string {
  return std::format("{:0{}}", value, width);
}

auto DateFormatStyle::zone(std::chrono::seconds offset, bool long_form)
    -> std::string {
  if (offset.count() == 0) {
    return "GMT";
  }
  const long long total = std::llabs(offset.count());
  const long long hours = total / 3600;
  const long long minutes = (total % 3600) / 60;
  const char sign = offset.count() < 0 ? '-' : '+';
  if (long_form || minutes != 0) {
    return std::format("GMT{}{:02}:{:02}", sign, hours, minutes);
  }
  return std::format("GMT{}{}", sign, hours);
}

auto DateFormatStyle::field(char letter, std::size_t count,
                            const Fields& fields, LocaleData data)
    -> std::string {
  using Width = LocaleData::Width;
  const long long hour = fields.clock.hours().count();
  switch (letter) {
    case 'y':
    case 'Y':
    case 'u': {
      const int year = static_cast<int>(fields.day.year());
      if (count == 2) {
        return padded(year % 100, 2);
      }
      return padded(year, count);
    }
    case 'M':
    case 'L': {
      const auto month = fields.day.month();
      if (count >= 4) {
        return std::string{data.month(month, Width::wide,
                                      letter == 'L'
                                          ? LocaleData::Context::standalone
                                          : LocaleData::Context::format)};
      }
      if (count == 3) {
        return std::string{data.month(month, Width::abbreviated)};
      }
      return padded(static_cast<unsigned>(month), count);
    }
    case 'd':
      return padded(static_cast<unsigned>(fields.day.day()), count);
    case 'E':
    case 'c':
    case 'e':
      if (count == 4) {
        return std::string{data.weekday(fields.weekday, Width::wide)};
      }
      if (count == 5) {
        return std::string{data.weekday(fields.weekday, Width::narrow)};
      }
      if (count == 6) {
        return std::string{data.weekday(fields.weekday, Width::short_form)};
      }
      if ((letter == 'c' || letter == 'e') && count <= 2) {
        return std::format("{}", fields.weekday.iso_encoding());
      }
      return std::string{data.weekday(fields.weekday, Width::abbreviated)};
    case 'a':
    case 'b':
    case 'B':
      return std::string{data.day_period(hour >= 12)};
    case 'h': {
      const long long shown = hour % 12 == 0 ? 12 : hour % 12;
      return padded(shown, count);
    }
    case 'K':
      return padded(hour % 12, count);
    case 'H':
      return padded(hour, count);
    case 'k':
      return padded(hour == 0 ? 24 : hour, count);
    case 'm':
      return padded(fields.clock.minutes().count(), count);
    case 's':
      return padded(fields.clock.seconds().count(), count);
    case 'z':
    case 'v':
    case 'V':
    case 'O':
    case 'Z':
    case 'x':
    case 'X':
      return zone(fields.offset, count >= 4);
    case 'G':
      return "AD";
    default:
      break;
  }
  return std::string(count, letter);
}

auto DateFormatStyle::expand(std::string_view pattern,
                             std::chrono::sys_seconds instant,
                             std::chrono::seconds offset, LocaleData data)
    -> std::string {
  const std::chrono::sys_seconds local = instant + offset;
  const auto days = std::chrono::floor<std::chrono::days>(local);
  const Fields fields{
      .day = std::chrono::year_month_day{days},
      .weekday = std::chrono::weekday{days},
      .clock = std::chrono::hh_mm_ss<std::chrono::seconds>{local - days},
      .offset = offset};
  std::string result;
  std::size_t at = 0;
  while (at < pattern.size()) {
    const char character = pattern[at];
    if (character == '\'') {
      if (at + 1 < pattern.size() && pattern[at + 1] == '\'') {
        result += '\'';
        at += 2;
        continue;
      }
      const std::size_t end = pattern.find('\'', at + 1);
      const std::size_t stop =
          end == std::string_view::npos ? pattern.size() : end;
      result += pattern.substr(at + 1, stop - at - 1);
      at = stop + 1;
      continue;
    }
    if ((character >= 'a' && character <= 'z') ||
        (character >= 'A' && character <= 'Z')) {
      std::size_t count = 1;
      while (at + count < pattern.size() && pattern[at + count] == character) {
        ++count;
      }
      result += field(character, count, fields, data);
      at += count;
      continue;
    }
    result += character;
    ++at;
  }
  return result;
}

auto RelativeDateTimeStyle::unit(std::chrono::seconds offset) noexcept
    -> std::pair<LocaleData::Unit, long long> {
  using Unit = LocaleData::Unit;
  const long long seconds = std::llabs(offset.count());
  constexpr long long minute = 60;
  constexpr long long hour = 60 * minute;
  constexpr long long day = 24 * hour;
  constexpr long long week = 7 * day;
  constexpr long long month = 30 * day;
  constexpr long long year = 365 * day;
  if (seconds < minute) {
    return {Unit::second, seconds};
  }
  if (seconds < hour) {
    return {Unit::minute, seconds / minute};
  }
  if (seconds < day) {
    return {Unit::hour, seconds / hour};
  }
  if (seconds < week) {
    return {Unit::day, seconds / day};
  }
  if (seconds < month) {
    return {Unit::week, seconds / week};
  }
  if (seconds < year) {
    return {Unit::month, seconds / month};
  }
  return {Unit::year, seconds / year};
}

auto RelativeDateTimeStyle::format(std::chrono::seconds offset,
                                   const Locale& locale) -> std::string {
  const LocaleData data = LocaleData::of(locale);
  const auto [which, count] = unit(offset);
  const PluralCategory category = PluralRules{data}.category(count);
  std::string pattern{
      data.relative_pattern(which, offset.count() >= 0, category)};
  const std::string number = NumberFormatStyle::number().format(
      static_cast<std::int64_t>(count), locale);
  if (const std::size_t at = pattern.find("{0}"); at != std::string::npos) {
    pattern.replace(at, 3, number);
  }
  return pattern;
}

auto DateFormatStyle::format(std::chrono::sys_seconds instant,
                             const Locale& locale) const -> std::string {
  return format(instant, std::chrono::seconds{0}, locale);
}

auto DateFormatStyle::spacing(std::string_view text, std::size_t at) noexcept
    -> std::size_t {
  if (at < text.size() && (text[at] == ' ' || text[at] == ',')) {
    return 1;
  }
  for (const std::string_view space :
       {std::string_view{" "}, std::string_view{" "}}) {
    if (text.substr(at).starts_with(space)) {
      return space.size();
    }
  }
  return 0;
}

auto DateFormatStyle::digits(std::string_view text, std::size_t& at,
                             std::size_t limit) -> std::optional<int> {
  int value = 0;
  std::size_t read = 0;
  while (at < text.size() && read < limit && text[at] >= '0' &&
         text[at] <= '9') {
    value = (value * 10) + (text[at] - '0');
    ++at;
    ++read;
  }
  if (read == 0) {
    return std::nullopt;
  }
  return value;
}

auto DateFormatStyle::word(std::string_view text, std::size_t& at,
                           std::string_view candidate) -> bool {
  if (candidate.empty() || !text.substr(at).starts_with(candidate)) {
    return false;
  }
  at += candidate.size();
  return true;
}

auto DateFormatStyle::read(char letter, std::size_t count,
                           std::string_view text, std::size_t& at,
                           LocaleData data, Parsed& parsed) -> bool {
  using Width = LocaleData::Width;
  switch (letter) {
    case 'y':
    case 'Y':
    case 'u': {
      const std::size_t start = at;
      const std::optional<int> year = digits(text, at, 4);
      if (!year.has_value()) {
        return false;
      }
      parsed.year = at - start <= 2 ? 2000 + *year : *year;
      return true;
    }
    case 'M':
    case 'L':
      if (count >= 3) {
        for (unsigned month = 1; month <= 12; ++month) {
          for (const Width width : {Width::wide, Width::abbreviated}) {
            for (const LocaleData::Context context :
                 {LocaleData::Context::format,
                  LocaleData::Context::standalone}) {
              if (word(text, at,
                       data.month(std::chrono::month{month}, width, context))) {
                parsed.month = month;
                return true;
              }
            }
          }
        }
        return false;
      }
      if (const std::optional<int> month = digits(text, at, 2)) {
        parsed.month = static_cast<unsigned>(*month);
        return true;
      }
      return false;
    case 'd':
      if (const std::optional<int> day = digits(text, at, 2)) {
        parsed.day = static_cast<unsigned>(*day);
        return true;
      }
      return false;
    case 'H':
    case 'k':
    case 'h':
    case 'K':
      if (const std::optional<int> hour = digits(text, at, 2)) {
        parsed.hour = letter == 'k' && *hour == 24 ? 0 : *hour;
        return true;
      }
      return false;
    case 'm':
      if (const std::optional<int> minute = digits(text, at, 2)) {
        parsed.minute = *minute;
        return true;
      }
      return false;
    case 's':
      if (const std::optional<int> second = digits(text, at, 2)) {
        parsed.second = *second;
        return true;
      }
      return false;
    case 'a':
    case 'b':
    case 'B':
      for (const bool after : {false, true}) {
        if (word(text, at, data.day_period(after))) {
          parsed.after_noon = after;
          return true;
        }
      }
      return false;
    case 'E':
    case 'c':
    case 'e':
      for (unsigned day = 0; day < 7; ++day) {
        for (const Width width : {Width::wide, Width::abbreviated,
                                  Width::short_form, Width::narrow}) {
          if (word(text, at, data.weekday(std::chrono::weekday{day}, width))) {
            return true;
          }
        }
      }
      return digits(text, at, 1).has_value();
    default:
      while (at < text.size() && text[at] != ' ' && spacing(text, at) == 0) {
        ++at;
      }
      return true;
  }
}

auto DateFormatStyle::parse(std::string_view text, const Locale& locale) const
    -> std::optional<std::chrono::sys_seconds> {
  const std::string pattern = resolved_pattern(locale);
  const LocaleData data = LocaleData::of(locale);
  Parsed parsed;
  std::size_t at = 0;
  std::size_t index = 0;
  while (index < pattern.size()) {
    const char character = pattern[index];
    if (const std::size_t gap = spacing(pattern, index); gap > 0) {
      while (const std::size_t skip = spacing(text, at)) {
        at += skip;
      }
      index += gap;
      continue;
    }
    if (character == '\'') {
      const std::size_t end = pattern.find('\'', index + 1);
      const std::size_t stop = end == std::string::npos ? pattern.size() : end;
      if (!word(
              text, at,
              std::string_view{pattern}.substr(index + 1, stop - index - 1)) &&
          stop > index + 1) {
        return std::nullopt;
      }
      index = stop + 1;
      continue;
    }
    if ((character >= 'a' && character <= 'z') ||
        (character >= 'A' && character <= 'Z')) {
      std::size_t count = 1;
      while (index + count < pattern.size() &&
             pattern[index + count] == character) {
        ++count;
      }
      if (!read(character, count, text, at, data, parsed)) {
        return std::nullopt;
      }
      index += count;
      continue;
    }
    if (at >= text.size() || text[at] != character) {
      return std::nullopt;
    }
    ++at;
    ++index;
  }
  while (const std::size_t skip = spacing(text, at)) {
    at += skip;
  }
  if (at != text.size()) {
    return std::nullopt;
  }
  int hour = parsed.hour;
  if (parsed.after_noon.has_value()) {
    hour %= 12;
    if (*parsed.after_noon) {
      hour += 12;
    }
  }
  const std::chrono::year_month_day day{std::chrono::year{parsed.year},
                                        std::chrono::month{parsed.month},
                                        std::chrono::day{parsed.day}};
  if (!day.ok() || hour > 23 || parsed.minute > 59 || parsed.second > 60) {
    return std::nullopt;
  }
  return std::chrono::sys_days{day} + std::chrono::hours{hour} +
         std::chrono::minutes{parsed.minute} +
         std::chrono::seconds{parsed.second};
}

}
