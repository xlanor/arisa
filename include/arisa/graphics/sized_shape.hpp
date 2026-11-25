#pragma once

#include <arisa/composition/size.hpp>
#include <arisa/graphics/any_shape.hpp>
#include <arisa/graphics/shape_role.hpp>

namespace arisa::graphics {

class SizedShape {
 public:
  SizedShape(AnyShape shape, composition::Size size,
             motion::UnitPoint anchor = motion::UnitPoint::top_leading);

  auto path(composition::Rect rect,
            const theme::RectangleCornerRadii& radii = {}) const -> Path;

  auto role() const -> ShapeRole { return shape_.role(); }

  auto shape() const noexcept -> const AnyShape& { return shape_; }

  auto size() const noexcept -> composition::Size { return size_; }

  auto anchor() const noexcept -> motion::UnitPoint { return anchor_; }

  friend auto operator==(const SizedShape&, const SizedShape&)
      -> bool = default;

 private:
  AnyShape shape_;
  composition::Size size_;
  motion::UnitPoint anchor_;
};

}
