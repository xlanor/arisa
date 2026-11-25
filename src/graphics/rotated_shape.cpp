#include <arisa/graphics/rotated_shape.hpp>

#include <utility>

namespace arisa::graphics {

RotatedShape::RotatedShape(AnyShape shape, motion::Angle angle,
                           motion::UnitPoint anchor)
    : shape_(std::move(shape)), angle_(angle), anchor_(anchor) {}

auto RotatedShape::path(composition::Rect rect,
                        const theme::RectangleCornerRadii& radii) const
    -> Path {
  const float x = rect.x + (anchor_.x * rect.width);
  const float y = rect.y + (anchor_.y * rect.height);
  return shape_.path(rect, radii)
      .applying(AffineTransform::translation(-x, -y)
                    .concatenating(AffineTransform::rotation(angle_))
                    .concatenating(AffineTransform::translation(x, y)));
}

}
