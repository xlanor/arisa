#pragma once

#include <arisa/graphics/shape_style.hpp>

#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace arisa::graphics {

class StyleHierarchy {
 public:
  static constexpr std::size_t levels = 5;

  static constexpr std::array<float, levels> derived_opacity{1.0F, 0.5F, 0.25F,
                                                             0.18F, 0.18F};

  StyleHierarchy(ShapeStyle primary);

  StyleHierarchy(ShapeStyle primary, ShapeStyle secondary);

  StyleHierarchy(ShapeStyle primary, ShapeStyle secondary, ShapeStyle tertiary);

  auto primary() const noexcept -> const ShapeStyle& { return declared_[0]; }

  auto declared() const noexcept -> std::span<const ShapeStyle> {
    return declared_;
  }

  auto level(HierarchicalShapeStyle tier) const -> ShapeStyle;

  friend auto operator==(const StyleHierarchy&, const StyleHierarchy&)
      -> bool = default;

 private:
  std::vector<ShapeStyle> declared_;
};

}
