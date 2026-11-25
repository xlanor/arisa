#include <arisa/appearance/appearance_file.hpp>
#include <arisa/appearance/theme_error.hpp>
#include <arisa/appearance/theme_file.hpp>
#include <arisa/theme/theme.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string_view>

extern "C" auto LLVMFuzzerTestOneInput(const std::uint8_t* data,
                                       std::size_t size) -> int {
  const std::string_view text{reinterpret_cast<const char*>(data), size};
  const arisa::appearance::ThemeFile::Resolver bases = [](std::string_view name)
      -> std::expected<arisa::theme::Theme, arisa::appearance::ThemeError> {
    if (name == "defaults") {
      return arisa::theme::Theme::defaults();
    }
    return std::unexpected(arisa::appearance::ThemeError{
        arisa::appearance::ThemeErrorCode::not_found,
        std::string{name},
        1,
        {},
        "unknown base"});
  };
  static_cast<void>(arisa::appearance::ThemeFile::base_of(text));
  static_cast<void>(
      arisa::appearance::ThemeFile::parse(text, "fuzz.toml", bases));
  static_cast<void>(
      arisa::appearance::AppearanceFile::parse(text, "fuzz.toml"));
  return 0;
}
