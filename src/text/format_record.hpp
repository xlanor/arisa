#pragma once

#include <arisa/text/locale_data.hpp>

#include <array>
#include <string_view>

namespace arisa::text {

struct LocaleData::FormatRecord {
  using Counts = std::array<std::string_view, 6>;

  std::string_view identifier;
  std::string_view currency;
  std::string_view currency_negative;
  std::string_view currency_iso;
  std::array<std::string_view, 18> currency_symbols;
  Counts bytes;
  std::array<std::string_view, 5> byte_units;
  std::string_view zero_bytes;
  std::string_view pair;
  std::string_view now;
  std::string_view yesterday;
  std::string_view tomorrow;
  std::array<std::array<Counts, 7>, 2> durations;
  std::array<std::array<Counts, 3>, 22> units;
};

}
