#pragma once

#include <arisa/graphics/any_shape.hpp>

namespace arisa::graphics {

class RotatedShape {
 public:
  RotatedShape(AnyShape shape, motion::Angle angle,
               motion::UnitPoint anchor = motion::UnitPoint::center);

  auto path(composition::Rect rect,
            const theme::RectangleCornerRadii& radii = {}) const -> Path;

  auto shape() const noexcept -> const AnyShape& { return shape_; }

  auto role() const -> ShapeRole { return shape_.role(); }

  auto inset(float amount) const -> RotatedShape {
    return RotatedShape{shape_.inset(amount), angle_, anchor_};
  }

  auto layout_direction_behavior() const
      -> composition::LayoutDirectionBehavior {
    return shape_.layout_direction_behavior();
  }

  auto angle() const noexcept -> motion::Angle { return angle_; }

  auto anchor() const noexcept -> motion::UnitPoint { return anchor_; }

  friend auto operator==(const RotatedShape&, const RotatedShape&)
      -> bool = default;

 private:
  AnyShape shape_;
  motion::Angle angle_;
  motion::UnitPoint anchor_;
};

}
