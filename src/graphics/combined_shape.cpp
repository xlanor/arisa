#include <arisa/graphics/combined_shape.hpp>

#include <utility>

namespace arisa::graphics {

CombinedShape::CombinedShape(AnyShape shape, AnyShape other,
                             Operation operation, bool eo_fill)
    : shape_(std::move(shape)),
      other_(std::move(other)),
      operation_(operation),
      eo_fill_(eo_fill) {}

auto CombinedShape::path(composition::Rect rect,
                         const theme::RectangleCornerRadii& radii) const
    -> Path {
  const Path first = shape_.path(rect, radii);
  const Path second = other_.path(rect, radii);
  switch (operation_) {
    case Operation::union_with:
      return first.union_with(second, eo_fill_);
    case Operation::intersection:
      return first.intersection(second, eo_fill_);
    case Operation::subtracting:
      return first.subtracting(second, eo_fill_);
    case Operation::symmetric_difference:
      return first.symmetric_difference(second, eo_fill_);
    case Operation::line_intersection:
      return first.line_intersection(second, eo_fill_);
    case Operation::line_subtraction:
      return first.line_subtraction(second, eo_fill_);
  }
  return first;
}

auto CombinedShape::role() const -> ShapeRole { return shape_.role(); }

}
