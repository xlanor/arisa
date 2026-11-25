#pragma once

#include <cstdint>

namespace arisa::composition {

enum class InputModality : std::uint8_t {
  controller,
  touch,
  keyboard_and_pointer,
};

}
