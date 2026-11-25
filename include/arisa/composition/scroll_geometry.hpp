#pragma once

#include <arisa/composition/point.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/composition/size.hpp>
#include <arisa/theme/space.hpp>

namespace arisa::composition {

struct ScrollGeometry {
  Point content_offset{};
  Size content_size{};
  theme::Edges content_insets{};
  Size container_size{};
  Rect visible_rect{};

  constexpr auto bounds() const noexcept -> Rect {
    return Rect{.x = content_offset.x,
                .y = content_offset.y,
                .width = container_size.width,
                .height = container_size.height};
  }

  friend constexpr auto operator==(const ScrollGeometry&, const ScrollGeometry&)
      -> bool = default;
};

}
