#pragma once

#include <cstdint>

namespace arisa::composition {

enum class FeedbackOrigin : std::uint8_t {
  application,
  system,
};

}
