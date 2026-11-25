#pragma once

#include <arisa/graphics/affine_transform.hpp>
#include <arisa/graphics/any_shape.hpp>

namespace arisa::graphics {

class TransformedShape {
 public:
  TransformedShape(AnyShape shape, AffineTransform transform);

  auto path(composition::Rect rect,
            const theme::RectangleCornerRadii& radii = {}) const -> Path;

  auto shape() const noexcept -> const AnyShape& { return shape_; }

  auto role() const -> ShapeRole { return shape_.role(); }

  auto layout_direction_behavior() const
      -> composition::LayoutDirectionBehavior {
    return shape_.layout_direction_behavior();
  }

  auto transform() const noexcept -> const AffineTransform& {
    return transform_;
  }

  friend auto operator==(const TransformedShape&, const TransformedShape&)
      -> bool = default;

 private:
  AnyShape shape_;
  AffineTransform transform_;
};

}
