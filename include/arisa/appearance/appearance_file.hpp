#pragma once

#include <arisa/appearance/appearance_settings.hpp>
#include <arisa/appearance/theme_error.hpp>

#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

namespace arisa::appearance {

class AppearanceFile {
 public:
  static constexpr std::int64_t schema_version = 1;
  static constexpr std::string_view path = "appearance.toml";

  [[nodiscard]] static auto parse(std::string_view text, std::string_view file)
      -> std::expected<AppearanceOverrides, ThemeError>;

  static auto serialise(const AppearanceOverrides& overrides) -> std::string;

 private:
  class Reader;
};

}
