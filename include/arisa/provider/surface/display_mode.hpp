#pragma once

#include <cstdint>

namespace arisa::provider {

enum class DisplayMode : std::uint8_t {
  handheld,
  docked,
  desktop,
};

}
