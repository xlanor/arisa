#include <arisa/controls/date_format.hpp>
#include <arisa/text/date_format_style.hpp>
#include <arisa/text/locale_data.hpp>

#include <cstddef>

namespace arisa::controls {

auto DateFormat::order(const text::Locale& locale) noexcept
    -> std::array<Field, 3> {
  const std::string_view pattern = text::LocaleData::of(locale).date_pattern(
      text::LocaleData::Length::short_form);
  std::array<Field, 3> found{Field::month, Field::day, Field::year};
  std::size_t count = 0;
  bool quoted = false;
  bool year = false;
  bool month = false;
  bool day = false;
  for (const char character : pattern) {
    if (character == '\'') {
      quoted = !quoted;
      continue;
    }
    if (quoted || count == found.size()) {
      continue;
    }
    if (character == 'y' && !year) {
      year = true;
      found[count++] = Field::year;
    } else if ((character == 'M' || character == 'L') && !month) {
      month = true;
      found[count++] = Field::month;
    } else if (character == 'd' && !day) {
      day = true;
      found[count++] = Field::day;
    }
  }
  return count == found.size()
             ? found
             : std::array{Field::month, Field::day, Field::year};
}

auto DateFormat::uses_twelve_hour(const text::Locale& locale) noexcept -> bool {
  return text::LocaleData::of(locale).uses_twelve_hour();
}

auto DateFormat::month_name(std::chrono::month month, bool abbreviated) noexcept
    -> std::string_view {
  return month_name(month, abbreviated, text::Locale::undetermined());
}

auto DateFormat::month_name(std::chrono::month month, bool abbreviated,
                            const text::Locale& locale) noexcept
    -> std::string_view {
  return text::LocaleData::of(locale).month(
      month,
      abbreviated ? text::LocaleData::Width::abbreviated
                  : text::LocaleData::Width::wide,
      text::LocaleData::Context::standalone);
}

auto DateFormat::weekday_symbol(std::chrono::weekday weekday) noexcept
    -> std::string_view {
  return weekday_symbol(weekday, text::Locale::undetermined());
}

auto DateFormat::weekday_symbol(std::chrono::weekday weekday,
                                const text::Locale& locale) noexcept
    -> std::string_view {
  return text::LocaleData::of(locale).weekday(weekday,
                                              text::LocaleData::Width::narrow);
}

auto DateFormat::date_text(std::chrono::year_month_day day,
                           const text::Locale& locale) -> std::string {
  const text::DateFormatStyle style = text::DateFormatStyle::pattern(
      std::string{text::LocaleData::of(locale).date_pattern(
          text::LocaleData::Length::medium)});
  return style.format(std::chrono::sys_days{day}, std::chrono::seconds{0},
                      locale);
}

auto DateFormat::time_text(std::chrono::minutes since_midnight,
                           const text::Locale& locale) -> std::string {
  const text::DateFormatStyle style{
      text::DateFormatStyle::DateStyle::omitted,
      text::DateFormatStyle::TimeStyle::shortened};
  return style.format(std::chrono::sys_seconds{since_midnight},
                      std::chrono::seconds{0}, locale);
}

auto DateFormat::month_title(std::chrono::year_month month) -> std::string {
  return month_title(month, text::Locale::undetermined());
}

auto DateFormat::month_title(std::chrono::year_month month,
                             const text::Locale& locale) -> std::string {
  const text::LocaleData data = text::LocaleData::of(locale);
  return text::DateFormatStyle::expand(
      data.month_year_pattern(),
      std::chrono::sys_days{month / std::chrono::day{1}},
      std::chrono::seconds{0}, data);
}

}
