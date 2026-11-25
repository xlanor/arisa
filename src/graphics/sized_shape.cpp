#include <arisa/graphics/sized_shape.hpp>

#include <utility>

namespace arisa::graphics {

SizedShape::SizedShape(AnyShape shape, composition::Size size,
                       motion::UnitPoint anchor)
    : shape_(std::move(shape)), size_(size), anchor_(anchor) {}

auto SizedShape::path(composition::Rect rect,
                      const theme::RectangleCornerRadii& radii) const -> Path {
  return shape_.path(
      composition::Rect{
          .x = rect.x + ((rect.width - size_.width) * anchor_.x),
          .y = rect.y + ((rect.height - size_.height) * anchor_.y),
          .width = size_.width,
          .height = size_.height},
      radii);
}

}
