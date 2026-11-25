#pragma once

#include <array>
#include <cstdint>

namespace arisa::composition {

enum class Visibility : std::uint8_t {
  automatic,
  visible,
  hidden,
};

inline constexpr std::array<Visibility, 3> all_visibilities{
    Visibility::automatic, Visibility::visible, Visibility::hidden};

}
