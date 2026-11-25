#pragma once

#include <cstdint>

namespace arisa::components {

enum class ButtonRole : std::uint8_t {
  cancel,
  destructive,
  confirm,
  close,
};

}
