#pragma once

#include <cstdint>

namespace arisa::composition {

enum class DefaultFocusEvaluationPriority : std::uint8_t {
  automatic,
  user_initiated,
};

}
