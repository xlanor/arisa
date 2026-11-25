#pragma once

#include <cstdint>

namespace arisa::components {

enum class EditMode : std::uint8_t {
  inactive,
  transient,
  active,
};

}
