#pragma once

#include <cstdint>

namespace arisa::motion {

enum class ScrollContentOffsetAdjustmentBehavior : std::uint8_t {
  automatic,
  disabled,
};

}
