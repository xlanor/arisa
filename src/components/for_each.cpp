#include <arisa/components/for_each.hpp>

#include <utility>

namespace arisa::components {

auto ForEach::describe(std::shared_ptr<const Source> source)
    -> std::unique_ptr<composition::ElementContent> {
  return std::make_unique<Content>(std::move(source));
}

auto ForEach::on_move(MoveAction action) && -> ForEach {
  edit<Content>().edits().move = std::move(action);
  return std::move(*this);
}

auto ForEach::on_delete(DeleteAction action) && -> ForEach {
  edit<Content>().edits().remove = std::move(action);
  return std::move(*this);
}

auto ForEach::reorderable() && -> ForEach {
  edit<Content>().reorderable(std::nullopt);
  return std::move(*this);
}

auto ForEach::reorderable(composition::ElementKey collection_id) && -> ForEach {
  edit<Content>().reorderable(std::move(collection_id));
  return std::move(*this);
}

void ForEach::Content::reorderable(
    std::optional<composition::ElementKey> collection) {
  source_ = std::make_shared<const Reorderable>(source_, std::move(collection));
}

auto ForEach::Reorderable::build(std::size_t index) const
    -> composition::Element {
  return inner_->build(index).reorder_item(inner_->key(index), collection_);
}

auto ForEach::Reorderable::same_item(std::size_t index, const Source& other,
                                     std::size_t other_index) const -> bool {
  if (&other == this) {
    return other_index == index;
  }
  if (other.tag() != tag()) {
    return false;
  }
  const auto& wrapped = static_cast<const Reorderable&>(other);
  return wrapped.collection_ == collection_ &&
         inner_->same_item(index, *wrapped.inner_, other_index);
}

auto ForEach::with_move(composition::Element element, MoveAction action)
    -> composition::Element {
  if (element.type() == composition::ElementType::of<Content>()) {
    static_cast<Content&>(content_of(element)).edits().move = std::move(action);
  }
  return element;
}

auto ForEach::with_delete(composition::Element element, DeleteAction action)
    -> composition::Element {
  if (element.type() == composition::ElementType::of<Content>()) {
    static_cast<Content&>(content_of(element)).edits().remove =
        std::move(action);
  }
  return element;
}

auto ForEach::source() const noexcept -> const Source* {
  if (type() != composition::ElementType::of<Content>()) {
    return nullptr;
  }
  return static_cast<const Content&>(Element::content()).source().get();
}

auto ForEach::edits_of(const composition::Element& element) -> Edits {
  if (element.type() != composition::ElementType::of<Content>()) {
    return {};
  }
  return static_cast<const Content&>(element.content()).edits();
}

auto ForEach::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto ForEach::Content::composes() const noexcept -> bool { return true; }

auto ForEach::Content::body(composition::Context&) const
    -> std::vector<composition::Element> {
  std::vector<composition::Element> children;
  const std::size_t count = source_->count();
  children.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    children.push_back(source_->element(index));
  }
  return children;
}

}
