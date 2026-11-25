#include <arisa/conformance/fonts_suite.hpp>

#include <algorithm>
#include <array>

namespace arisa::conformance {

using provider::ProviderErrorCode;
using provider::SystemFont;

void FontsSuite::run(provider::ProviderFactory& factory, Report& report) {
  const provider::FontProvider& fonts = factory.fonts();
  if (!factory.supplies(provider::Capability::fonts)) {
    check_absent(fonts, report);
    return;
  }
  check_standard(fonts, report);
  check_others(fonts, report);
}

auto FontsSuite::has_sfnt_tag(std::span<const std::byte> bytes) noexcept
    -> bool {
  static constexpr std::size_t tag_size = 4;
  static constexpr std::array<std::array<std::byte, tag_size>, 4> tags{{
      {std::byte{0x00}, std::byte{0x01}, std::byte{0x00}, std::byte{0x00}},
      {std::byte{'O'}, std::byte{'T'}, std::byte{'T'}, std::byte{'O'}},
      {std::byte{'t'}, std::byte{'r'}, std::byte{'u'}, std::byte{'e'}},
      {std::byte{'t'}, std::byte{'t'}, std::byte{'c'}, std::byte{'f'}},
  }};
  if (bytes.size() < tag_size) {
    return false;
  }
  const auto head = bytes.first(tag_size);
  return std::ranges::any_of(
      tags, [head](const auto& tag) { return std::ranges::equal(head, tag); });
}

void FontsSuite::check_standard(const provider::FontProvider& fonts,
                                Report& report) {
  const auto standard = fonts.system_font(SystemFont::standard);
  report.record("fonts.standard",
                fonts.available(SystemFont::standard) && standard.has_value() &&
                    !standard->empty() && has_sfnt_tag(standard->bytes()),
                "the standard system font is missing, empty or not an sfnt");
}

void FontsSuite::check_others(const provider::FontProvider& fonts,
                              Report& report) {
  const bool conforms = std::ranges::all_of(
      provider::all_system_fonts, [&fonts](SystemFont font) {
        const auto face = fonts.system_font(font);
        if (face.has_value()) {
          return fonts.available(font) && !face->empty() &&
                 has_sfnt_tag(face->bytes());
        }
        return !fonts.available(font) &&
               face.error().code() == ProviderErrorCode::not_found;
      });
  report.record("fonts.system_fonts", conforms,
                "a system font was neither a valid sfnt nor not_found");
}

void FontsSuite::check_absent(const provider::FontProvider& fonts,
                              Report& report) {
  const bool conforms = std::ranges::all_of(
      provider::all_system_fonts, [&fonts](SystemFont font) {
        const auto face = fonts.system_font(font);
        return !fonts.available(font) && !face.has_value() &&
               face.error().code() == ProviderErrorCode::unsupported_capability;
      });
  report.record("fonts.absent", conforms,
                "an unsupplied font provider did not return "
                "unsupported_capability");
}

}
