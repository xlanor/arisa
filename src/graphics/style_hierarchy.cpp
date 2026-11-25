#include <arisa/graphics/style_hierarchy.hpp>

#include <algorithm>
#include <utility>

namespace arisa::graphics {

StyleHierarchy::StyleHierarchy(ShapeStyle primary) {
  declared_.push_back(std::move(primary));
}

StyleHierarchy::StyleHierarchy(ShapeStyle primary, ShapeStyle secondary) {
  declared_.reserve(2);
  declared_.push_back(std::move(primary));
  declared_.push_back(std::move(secondary));
}

StyleHierarchy::StyleHierarchy(ShapeStyle primary, ShapeStyle secondary,
                               ShapeStyle tertiary) {
  declared_.reserve(3);
  declared_.push_back(std::move(primary));
  declared_.push_back(std::move(secondary));
  declared_.push_back(std::move(tertiary));
}

auto StyleHierarchy::level(HierarchicalShapeStyle tier) const -> ShapeStyle {
  const auto index = static_cast<std::size_t>(tier);
  if (declared_.size() == 1) {
    return declared_.front().opacity(derived_opacity[index]);
  }
  return declared_[std::min(index, declared_.size() - 1)];
}

}
