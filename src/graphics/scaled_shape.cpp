#include <arisa/graphics/scaled_shape.hpp>

#include <utility>

namespace arisa::graphics {

ScaledShape::ScaledShape(AnyShape shape, composition::Size scale,
                         motion::UnitPoint anchor)
    : shape_(std::move(shape)), scale_(scale), anchor_(anchor) {}

auto ScaledShape::path(composition::Rect rect,
                       const theme::RectangleCornerRadii& radii) const -> Path {
  const float x = rect.x + (anchor_.x * rect.width);
  const float y = rect.y + (anchor_.y * rect.height);
  return shape_.path(rect, radii)
      .applying(AffineTransform::translation(-x, -y)
                    .concatenating(
                        AffineTransform::scale(scale_.width, scale_.height))
                    .concatenating(AffineTransform::translation(x, y)));
}

}
