#pragma once

#include <cstdint>

namespace arisa::structure {

enum class TabSectionExpansion : std::uint8_t {
  automatic,
  expanded,
  collapsed,
};

}
