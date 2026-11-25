#pragma once

#include <arisa/text/locale.hpp>

#include <chrono>
#include <cstdint>
#include <string_view>

namespace arisa::components {

class Calendar {
 public:
  enum class Identifier : std::uint8_t {
    gregorian,
    iso8601,
    buddhist,
    japanese,
    republic_of_china,
  };

  explicit Calendar(Identifier identifier = Identifier::gregorian) noexcept
      : identifier_(identifier),
        first_weekday_(identifier == Identifier::iso8601
                           ? std::chrono::Monday
                           : std::chrono::Sunday) {}

  Calendar(Identifier identifier, std::chrono::weekday first_weekday) noexcept
      : identifier_(identifier), first_weekday_(first_weekday) {}

  auto identifier() const noexcept -> Identifier { return identifier_; }

  auto first_weekday() const noexcept -> std::chrono::weekday {
    return first_weekday_;
  }

  auto name() const noexcept -> std::string_view;

  static auto for_locale(const text::Locale& locale) noexcept -> Calendar;

  friend auto operator==(const Calendar&, const Calendar&) -> bool = default;

 private:
  Identifier identifier_;
  std::chrono::weekday first_weekday_;
};

}
