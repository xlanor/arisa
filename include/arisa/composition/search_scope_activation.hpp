#pragma once

#include <cstdint>

namespace arisa::composition {

enum class SearchScopeActivation : std::uint8_t {
  automatic,
  on_text_entry,
  on_search_presentation,
};

}
