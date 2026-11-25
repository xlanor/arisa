#include <arisa/graphics/offset_shape.hpp>

#include <utility>

namespace arisa::graphics {

OffsetShape::OffsetShape(AnyShape shape, composition::Size offset)
    : shape_(std::move(shape)), offset_(offset) {}

auto OffsetShape::path(composition::Rect rect,
                       const theme::RectangleCornerRadii& radii) const -> Path {
  return shape_.path(rect, radii)
      .applying(AffineTransform::translation(offset_.width, offset_.height));
}

}
