#include <arisa/components/for_each.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/subviews_collection.hpp>
#include <arisa/composition/modifiers.hpp>

#include <utility>

namespace arisa::components {

SubviewsCollection::SubviewsCollection(composition::Element content) {
  flatten(std::move(content), composition::KeyedValues{});
}

SubviewsCollection::SubviewsCollection(
    std::vector<composition::Element> content) {
  for (composition::Element& element : content) {
    flatten(std::move(element), composition::KeyedValues{});
  }
}

SubviewsCollection::SubviewsCollection(composition::Context& context,
                                       composition::Element content)
    : context_(context) {
  flatten(std::move(content), composition::KeyedValues{});
}

SubviewsCollection::SubviewsCollection(
    composition::Context& context, std::vector<composition::Element> content)
    : context_(context) {
  for (composition::Element& element : content) {
    flatten(std::move(element), composition::KeyedValues{});
  }
}

auto SubviewsCollection::expands(const composition::Element& element) const
    -> bool {
  return context_.has_value() && element.content().composes() &&
         !element.key().has_value() &&
         element.modifiers() == composition::Modifiers{};
}

void SubviewsCollection::flatten(composition::Element element,
                                 const composition::KeyedValues& inherited) {
  composition::KeyedValues values = element.container_values();
  if (element.tag() != nullptr) {
    values.set_tag(element.tag());
  }
  values.merge_missing(inherited);
  if (element.type() == composition::ElementType::of<Group::Content>()) {
    for (composition::Element& member : Group::members(std::move(element))) {
      flatten(std::move(member), values);
    }
    return;
  }
  if (element.type() == composition::ElementType::of<ForEach::Content>()) {
    const auto& source =
        static_cast<const ForEach::Content&>(element.content()).source();
    for (std::size_t index = 0; index < source->count(); ++index) {
      flatten(source->element(index), values);
    }
    return;
  }
  if (expands(element)) {
    for (composition::Element& made : element.content().body(*context_)) {
      flatten(std::move(made), values);
    }
    return;
  }
  composition::ElementKey id =
      element.key().value_or(composition::ElementKey{subviews_.size()});
  subviews_.emplace_back(std::move(element), std::move(id), std::move(values));
}

}
