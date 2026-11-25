#pragma once

#include <arisa/composition/gesture.hpp>
#include <arisa/composition/gesture_mask.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace arisa::composition {

enum class GesturePriority : std::uint8_t {
  normal,
  simultaneous,
  high,
};

struct ScrollHandoff {
  bool expands{false};

  friend constexpr auto operator==(const ScrollHandoff&, const ScrollHandoff&)
      -> bool = default;
};

struct GestureAttachment {
  std::shared_ptr<Gesture> gesture;
  GesturePriority priority{GesturePriority::normal};
  GestureMask mask{GestureMask::all};
  std::optional<ScrollHandoff> handoff;
  std::string name{};
};

}
