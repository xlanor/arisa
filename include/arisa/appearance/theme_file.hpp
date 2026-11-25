#pragma once

#include <arisa/appearance/theme_error.hpp>
#include <arisa/theme/theme.hpp>

#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace arisa::appearance {

class ThemeFile {
 public:
  static constexpr std::int64_t schema_version = 1;

  using Resolver = std::function<std::expected<theme::Theme, ThemeError>(
      std::string_view name)>;

  [[nodiscard]] static auto parse(std::string_view text, std::string_view file,
                                  const Resolver& bases)
      -> std::expected<theme::Theme, ThemeError>;

  static auto base_of(std::string_view text) -> std::optional<std::string>;

  static auto serialise(const theme::Theme& edited, const theme::Theme& base,
                        std::string_view name, std::string_view extends)
      -> std::string;

 private:
  class Reader;

  class Writer;
};

}
