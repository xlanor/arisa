#include <arisa/components/for_each.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/section_collection.hpp>
#include <arisa/composition/list_row.hpp>
#include <arisa/composition/section_parts.hpp>

#include <utility>

namespace arisa::components {

SectionCollection::SectionCollection(composition::Element content) {
  flatten(std::move(content));
  close_loose();
}

SectionCollection::SectionCollection(
    std::vector<composition::Element> content) {
  for (composition::Element& element : content) {
    flatten(std::move(element));
  }
  close_loose();
}

void SectionCollection::close_loose() {
  if (loose_.empty()) {
    return;
  }
  sections_.emplace_back(
      SectionConfiguration::ID{composition::ElementKey{sections_.size()}},
      SubviewsCollection{std::vector<composition::Element>{}},
      SubviewsCollection{std::vector<composition::Element>{}},
      SubviewsCollection{std::exchange(loose_, {})},
      SubviewsCollection{std::vector<composition::Element>{}},
      composition::KeyedValues{});
}

void SectionCollection::flatten(composition::Element element) {
  if (element.type() == composition::ElementType::of<Group::Content>()) {
    for (composition::Element& member : Group::members(std::move(element))) {
      flatten(std::move(member));
    }
    return;
  }
  if (element.type() == composition::ElementType::of<ForEach::Content>()) {
    const auto& source =
        static_cast<const ForEach::Content&>(element.content()).source();
    for (std::size_t index = 0; index < source->count(); ++index) {
      flatten(source->element(index));
    }
    return;
  }
  std::optional<composition::SectionParts> parts =
      element.content().section_parts();
  if (!parts.has_value()) {
    loose_.push_back(std::move(element));
    return;
  }
  close_loose();
  const auto lone = [](std::optional<composition::Element>& view) {
    std::vector<composition::Element> found;
    if (view.has_value()) {
      found.push_back(std::move(*view));
    }
    return SubviewsCollection{std::move(found)};
  };
  std::vector<composition::Element> actions;
  if (const std::shared_ptr<const composition::ListRow> row =
          element.list_row();
      row != nullptr && row->section_actions) {
    actions.push_back(row->section_actions());
  }
  composition::ElementKey id =
      element.key().value_or(composition::ElementKey{sections_.size()});
  sections_.emplace_back(
      SectionConfiguration::ID{std::move(id)}, lone(parts->header),
      lone(parts->footer), SubviewsCollection{std::move(parts->content)},
      SubviewsCollection{std::move(actions)}, element.container_values());
}

}
