#pragma once

#include <arisa/composition/alignment.hpp>
#include <arisa/composition/node_id.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>

#include <algorithm>
#include <array>

namespace arisa::composition {

class ContainerCorners {
 public:
  struct Enclosure {
    NodeId container;
    theme::RectangleCornerRadii radii;

    friend constexpr auto operator==(const Enclosure&, const Enclosure&)
        -> bool = default;
  };

  constexpr ContainerCorners() noexcept = default;

  constexpr ContainerCorners(Rect frame, theme::RectangleCornerRadii radii,
                             LayoutDirection direction) noexcept
      : frame_(frame), radii_(radii), direction_(direction) {}

  constexpr auto frame() const noexcept -> Rect { return frame_; }

  constexpr auto insets(Rect view) const noexcept
      -> theme::RectangleCornerInsets {
    const std::array<float, 4> reach = reaches(view);
    return theme::RectangleCornerInsets{
        .top_leading = {.width = reach[0], .height = reach[0]},
        .top_trailing = {.width = reach[1], .height = reach[1]},
        .bottom_leading = {.width = reach[2], .height = reach[2]},
        .bottom_trailing = {.width = reach[3], .height = reach[3]}};
  }

  constexpr auto concentric_radii(Rect view) const noexcept
      -> theme::RectangleCornerRadii {
    const std::array<float, 4> reach = reaches(view);
    const float limit =
        std::max(std::min(view.width, view.height) * 0.5F, 0.0F);
    return theme::RectangleCornerRadii{
        .top_leading = std::min(reach[0], limit),
        .bottom_leading = std::min(reach[2], limit),
        .bottom_trailing = std::min(reach[3], limit),
        .top_trailing = std::min(reach[1], limit)};
  }

  friend constexpr auto operator==(const ContainerCorners&,
                                   const ContainerCorners&) -> bool = default;

 private:
  constexpr auto reaches(Rect view) const noexcept -> std::array<float, 4> {
    const float limit =
        std::max(std::min(frame_.width, frame_.height) * 0.5F, 0.0F);
    const float left = std::max(view.x - frame_.x, 0.0F);
    const float top = std::max(view.y - frame_.y, 0.0F);
    const float right =
        std::max((frame_.x + frame_.width) - (view.x + view.width), 0.0F);
    const float bottom =
        std::max((frame_.y + frame_.height) - (view.y + view.height), 0.0F);
    const bool mirrored = direction_ == LayoutDirection::right_to_left;
    const float leading = mirrored ? right : left;
    const float trailing = mirrored ? left : right;
    const auto reach = [limit](float radius, float across, float down) {
      return std::max(std::min(radius, limit) - std::max(across, down), 0.0F);
    };
    return {reach(radii_.top_leading, leading, top),
            reach(radii_.top_trailing, trailing, top),
            reach(radii_.bottom_leading, leading, bottom),
            reach(radii_.bottom_trailing, trailing, bottom)};
  }

  Rect frame_{};
  theme::RectangleCornerRadii radii_{};
  LayoutDirection direction_{LayoutDirection::left_to_right};
};

}
