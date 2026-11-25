#pragma once

#include <cstdint>

namespace arisa::text {

enum class WritingDirection : std::uint8_t {
  natural,
  left_to_right,
  right_to_left,
};

}
