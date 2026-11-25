#pragma once

#include <array>
#include <cstdint>

namespace arisa::composition {

enum class FrameResizeDirection : std::uint8_t {
  inward,
  outward,
};

inline constexpr std::array<FrameResizeDirection, 2>
    all_frame_resize_directions{FrameResizeDirection::inward,
                                FrameResizeDirection::outward};

}
