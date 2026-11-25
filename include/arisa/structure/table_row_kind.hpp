#pragma once

#include <cstdint>

namespace arisa::structure {

enum class TableRowKind : std::uint8_t {
  row,
  disclosure,
  header,
};

}
