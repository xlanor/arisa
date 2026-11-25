#include <arisa/components/group.hpp>

#include <memory>
#include <utility>

namespace arisa::components {

Group::Group(std::vector<composition::Element> children)
    : Element(std::make_unique<Content>(), std::move(children)) {}

auto Group::members(composition::Element element)
    -> std::vector<composition::Element> {
  if (element.type() != composition::ElementType::of<Content>()) {
    std::vector<composition::Element> single;
    single.push_back(std::move(element));
    return single;
  }
  return release_children(element);
}

auto Group::is_empty(const composition::Element& element) noexcept -> bool {
  return element.type() == composition::ElementType::of<Content>() &&
         element.children().empty();
}

auto Group::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

}
