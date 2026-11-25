#pragma once

#include <arisa/text/locale_data.hpp>

#include <array>
#include <cstdint>
#include <string_view>

namespace arisa::text {

struct LocaleData::Record {
  using Months = std::array<std::string_view, 12>;
  using Weekdays = std::array<std::string_view, 7>;
  using Lengths = std::array<std::string_view, 4>;
  using Counts = std::array<std::string_view, 6>;

  std::string_view identifier;
  std::string_view region;
  std::string_view decimal;
  std::string_view group;
  std::string_view minus;
  std::string_view plus;
  std::string_view percent_sign;
  std::string_view decimal_pattern;
  std::string_view percent_pattern;
  std::uint8_t minimum_grouping;
  Months months_wide;
  Months months_abbreviated;
  Months months_standalone;
  Weekdays weekdays_wide;
  Weekdays weekdays_abbreviated;
  Weekdays weekdays_short;
  Weekdays weekdays_narrow;
  std::string_view am;
  std::string_view pm;
  Lengths date_patterns;
  Lengths time_patterns;
  Lengths date_time_patterns;
  std::string_view month_year;
  std::string_view numeric_date;
  std::string_view abbreviated_date;
  std::array<std::string_view, 5> plural_rules;
  std::array<std::array<Counts, 2>, 7> relative;
};

}
