#pragma once

#include <cstdint>

namespace arisa::motion {

enum class AnimationCompletionCriteria : std::uint8_t {
  logically_complete,
  removed,
};

}
