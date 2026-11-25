#pragma once

#include <arisa/composition/size.hpp>
#include <arisa/graphics/any_shape.hpp>

namespace arisa::graphics {

class OffsetShape {
 public:
  OffsetShape(AnyShape shape, composition::Size offset);

  auto path(composition::Rect rect,
            const theme::RectangleCornerRadii& radii = {}) const -> Path;

  auto shape() const noexcept -> const AnyShape& { return shape_; }

  auto role() const -> ShapeRole { return shape_.role(); }

  auto inset(float amount) const -> OffsetShape {
    return OffsetShape{shape_.inset(amount), offset_};
  }

  auto layout_direction_behavior() const
      -> composition::LayoutDirectionBehavior {
    return shape_.layout_direction_behavior();
  }

  auto offset() const noexcept -> composition::Size { return offset_; }

  friend auto operator==(const OffsetShape&, const OffsetShape&)
      -> bool = default;

 private:
  AnyShape shape_;
  composition::Size offset_;
};

}
