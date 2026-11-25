#pragma once

#include <cstdint>

namespace arisa::composition {

enum class ContentMarginPlacement : std::uint8_t {
  automatic,
  scroll_content,
  scroll_indicators,
};

}
