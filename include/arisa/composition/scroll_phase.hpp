#pragma once

#include <cstdint>

namespace arisa::composition {

enum class ScrollPhase : std::uint8_t {
  idle,
  tracking,
  interacting,
  decelerating,
  animating,
};

constexpr auto is_scrolling(ScrollPhase phase) noexcept -> bool {
  return phase != ScrollPhase::idle;
}

}
