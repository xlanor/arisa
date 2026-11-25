#pragma once

#include <array>
#include <cstdint>

namespace arisa::composition {

enum class FrameResizePosition : std::uint8_t {
  top,
  leading,
  bottom,
  trailing,
  top_leading,
  top_trailing,
  bottom_leading,
  bottom_trailing,
};

inline constexpr std::array<FrameResizePosition, 8> all_frame_resize_positions{
    FrameResizePosition::top,
    FrameResizePosition::leading,
    FrameResizePosition::bottom,
    FrameResizePosition::trailing,
    FrameResizePosition::top_leading,
    FrameResizePosition::top_trailing,
    FrameResizePosition::bottom_leading,
    FrameResizePosition::bottom_trailing};

}
