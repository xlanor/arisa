#pragma once

#include <arisa/provider/error.hpp>
#include <arisa/provider/fonts/font_data.hpp>

#include <array>
#include <cstdint>
#include <expected>

namespace arisa::provider {

enum class SystemFont : std::uint8_t {
  standard,
  chinese_simplified,
  chinese_simplified_extended,
  chinese_traditional,
  korean,
  nintendo_symbols,
};

inline constexpr std::array all_system_fonts{
    SystemFont::standard,
    SystemFont::chinese_simplified,
    SystemFont::chinese_simplified_extended,
    SystemFont::chinese_traditional,
    SystemFont::korean,
    SystemFont::nintendo_symbols,
};

class FontProvider {
 public:
  virtual ~FontProvider() = default;

  [[nodiscard]] virtual auto system_font(SystemFont font) const
      -> std::expected<FontData, ProviderError> = 0;

  virtual auto available(SystemFont font) const -> bool = 0;
};

}
