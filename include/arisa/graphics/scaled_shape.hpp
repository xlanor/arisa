#pragma once

#include <arisa/composition/size.hpp>
#include <arisa/graphics/any_shape.hpp>

namespace arisa::graphics {

class ScaledShape {
 public:
  ScaledShape(AnyShape shape, composition::Size scale,
              motion::UnitPoint anchor = motion::UnitPoint::center);

  auto path(composition::Rect rect,
            const theme::RectangleCornerRadii& radii = {}) const -> Path;

  auto shape() const noexcept -> const AnyShape& { return shape_; }

  auto role() const -> ShapeRole { return shape_.role(); }

  auto layout_direction_behavior() const
      -> composition::LayoutDirectionBehavior {
    return shape_.layout_direction_behavior();
  }

  auto scale() const noexcept -> composition::Size { return scale_; }

  auto anchor() const noexcept -> motion::UnitPoint { return anchor_; }

  friend auto operator==(const ScaledShape&, const ScaledShape&)
      -> bool = default;

 private:
  AnyShape shape_;
  composition::Size scale_;
  motion::UnitPoint anchor_;
};

}
