#include <arisa/composition/placeholder_content_view.hpp>

#include <utility>

namespace arisa::composition {

auto PlaceholderContentView::Source::take() noexcept -> std::optional<Element> {
  return std::exchange(element_, std::nullopt);
}

void PlaceholderContentView::Source::forward(Source& newer) noexcept {
  element_ = std::exchange(newer.element_, std::nullopt);
}

void PlaceholderContentView::Source::replace(Source& newer) {
  if (&newer == this || !newer.element_.has_value()) {
    return;
  }
  element_ = std::exchange(newer.element_, std::nullopt);
  if (const std::shared_ptr<const NodeAnchor> shown = shown_.lock()) {
    shown->mark_dirty();
  }
}

auto PlaceholderContentView::Source::renew() const -> bool {
  if (element_.has_value()) {
    return true;
  }
  if (const std::shared_ptr<const NodeAnchor> declarer = declared_.lock()) {
    declarer->mark_dirty();
    return true;
  }
  return false;
}

PlaceholderContentView::PlaceholderContentView(std::shared_ptr<Source> source)
    : Element(std::make_unique<Content>(std::move(source))) {}

auto PlaceholderContentView::Content::type() const noexcept -> ElementType {
  return ElementType::of<Content>();
}

auto PlaceholderContentView::Content::body(Context&) const
    -> std::vector<Element> {
  std::vector<Element> produced;
  if (auto element = source_->take()) {
    produced.push_back(std::move(*element));
  }
  return produced;
}

auto PlaceholderContentView::Content::same_as(const ElementContent& other) const
    -> bool {
  return other.type() == type() &&
         !static_cast<const Content&>(other).source_->pending();
}

}
