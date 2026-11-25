#include <arisa/components/any_layout.hpp>
#include <arisa/components/grid.hpp>

#include <memory>
#include <utility>

namespace arisa::components {

Grid::Grid(GridOptions options, std::vector<composition::Element> children)
    : Element(std::make_unique<Content>(options), std::move(children)) {}

Grid::Grid(Switchable, GridOptions options,
           std::vector<composition::Element> children)
    : Element(std::make_unique<Content>(options, true), std::move(children)) {}

auto Grid::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto Grid::Content::kind() const noexcept -> composition::ElementType {
  return switchable_ ? composition::ElementType::of<AnyLayout>() : type();
}

auto Grid::Content::paints_like(const composition::ElementContent& other) const
    -> bool {
  if (other.type() != type()) {
    return false;
  }
  return options_ == static_cast<const Content&>(other).options_;
}

}
