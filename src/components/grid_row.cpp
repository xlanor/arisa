#include <arisa/components/grid_row.hpp>

#include <memory>
#include <utility>

namespace arisa::components {

GridRow::GridRow(GridRowOptions options,
                 std::vector<composition::Element> cells)
    : Element(std::make_unique<Content>(options), std::move(cells)) {}

auto GridRow::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto GridRow::Content::paints_like(
    const composition::ElementContent& other) const -> bool {
  if (other.type() != type()) {
    return false;
  }
  return options_ == static_cast<const Content&>(other).options_;
}

}
