#pragma once

#include <cstdint>

namespace arisa::text {

enum class FontRole : std::uint8_t {
  latin,
  script,
  japanese,
  chinese_simplified,
  chinese_traditional,
  korean,
  symbols,
  emoji,
};

}
