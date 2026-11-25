#pragma once

#include <cstdint>

namespace arisa::text {

enum class TextAlignment : std::uint8_t {
  natural,
  left,
  center,
  right,
  opposite,
};

}
