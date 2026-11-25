#include <arisa/graphics/transformed_shape.hpp>

#include <utility>

namespace arisa::graphics {

TransformedShape::TransformedShape(AnyShape shape, AffineTransform transform)
    : shape_(std::move(shape)), transform_(transform) {}

auto TransformedShape::path(composition::Rect rect,
                            const theme::RectangleCornerRadii& radii) const
    -> Path {
  return shape_.path(rect, radii).applying(transform_);
}

}
