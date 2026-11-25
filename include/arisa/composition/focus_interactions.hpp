#pragma once

#include <cstdint>
#include <utility>

namespace arisa::composition {

enum class FocusInteractions : std::uint8_t {
  activate = 1,
  edit = 2,
  automatic = 3,
};

constexpr auto operator|(FocusInteractions left,
                         FocusInteractions right) noexcept
    -> FocusInteractions {
  return static_cast<FocusInteractions>(std::to_underlying(left) |
                                        std::to_underlying(right));
}

constexpr auto operator&(FocusInteractions left,
                         FocusInteractions right) noexcept
    -> FocusInteractions {
  return static_cast<FocusInteractions>(std::to_underlying(left) &
                                        std::to_underlying(right));
}

}
