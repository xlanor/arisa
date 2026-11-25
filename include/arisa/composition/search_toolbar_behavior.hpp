#pragma once

#include <cstdint>

namespace arisa::composition {

enum class SearchToolbarBehavior : std::uint8_t {
  automatic,
  minimize,
};

enum class SearchPresentationToolbarBehavior : std::uint8_t {
  automatic,
  avoid_hiding_content,
};

}
