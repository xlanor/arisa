#pragma once

#include <arisa/graphics/any_shape.hpp>
#include <arisa/graphics/shape_role.hpp>

#include <cstdint>

namespace arisa::graphics {

class CombinedShape {
 public:
  enum class Operation : std::uint8_t {
    union_with,
    intersection,
    subtracting,
    symmetric_difference,
    line_intersection,
    line_subtraction,
  };

  CombinedShape(AnyShape shape, AnyShape other, Operation operation,
                bool eo_fill = false);

  auto path(composition::Rect rect,
            const theme::RectangleCornerRadii& radii = {}) const -> Path;

  auto role() const -> ShapeRole;

  auto shape() const noexcept -> const AnyShape& { return shape_; }

  auto other() const noexcept -> const AnyShape& { return other_; }

  auto operation() const noexcept -> Operation { return operation_; }

  auto eo_fill() const noexcept -> bool { return eo_fill_; }

  friend auto operator==(const CombinedShape&, const CombinedShape&)
      -> bool = default;

 private:
  AnyShape shape_;
  AnyShape other_;
  Operation operation_;
  bool eo_fill_;
};

}
