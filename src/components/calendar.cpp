#include <arisa/components/calendar.hpp>
#include <arisa/text/locale_data.hpp>

namespace arisa::components {

auto Calendar::name() const noexcept -> std::string_view {
  switch (identifier_) {
    case Identifier::gregorian:
      return "gregorian";
    case Identifier::iso8601:
      return "iso8601";
    case Identifier::buddhist:
      return "buddhist";
    case Identifier::japanese:
      return "japanese";
    case Identifier::republic_of_china:
      return "republic_of_china";
  }
  return "gregorian";
}

auto Calendar::for_locale(const text::Locale& locale) noexcept -> Calendar {
  return Calendar{Identifier::gregorian,
                  text::LocaleData::of(locale).first_weekday(locale.region())};
}

}
