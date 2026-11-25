#pragma once

#include <cstdint>

namespace arisa::text {

enum class TruncationMode : std::uint8_t {
  head,
  middle,
  tail,
};

}
