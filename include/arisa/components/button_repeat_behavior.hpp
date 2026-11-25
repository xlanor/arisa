#pragma once

#include <cstdint>

namespace arisa::components {

enum class ButtonRepeatBehavior : std::uint8_t {
  automatic,
  enabled,
  disabled,
};

}
