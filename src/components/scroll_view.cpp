#include <arisa/components/hstack.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/scroll_environment.hpp>

#include <memory>
#include <utility>

namespace arisa::components {

ScrollView::ScrollView(ScrollViewOptions options,
                       std::vector<composition::Element> children)
    : Element(std::make_unique<Content>(options),
              arrange(options, std::move(children))) {
  declared().scrolling.container = composition::Scrolling::Container{
      .axes = options.axes, .shows_indicators = options.shows_indicators};
  if (options.axes != composition::Axes::horizontal) {
    declared().focusable = true;
    declared().focus_fallback = true;
  }
}

auto ScrollView::arrange(const ScrollViewOptions& options,
                         std::vector<composition::Element> children)
    -> std::vector<composition::Element> {
  std::vector<composition::Element> wrapped;
  if (children.size() == 1) {
    wrapped.push_back(std::move(children.front()));
  } else if (options.axes == composition::Axes::horizontal) {
    wrapped.push_back(HStack(std::move(children)));
  } else {
    wrapped.push_back(VStack(std::move(children)));
  }
  wrapped.front() = consumed(options.axes, std::move(wrapped.front()));
  return wrapped;
}

auto ScrollView::consumed(composition::Axes axes, composition::Element content)
    -> composition::Element {
  using composition::ScrollEnvironment;
  const auto reset = [&content](const auto& key, const auto& value) {
    if (!content.declared_environment(key).has_value()) {
      content = std::move(content).environment(key, value);
    }
  };
  reset(ScrollEnvironment::is_scroll_enabled, true);
  if (axes.contains(composition::Axis::horizontal)) {
    reset(ScrollEnvironment::horizontal_scroll_indicator_visibility,
          composition::ScrollIndicatorVisibility::automatic);
    reset(ScrollEnvironment::horizontal_scroll_bounce_behavior,
          composition::ScrollBounceBehavior::automatic);
  }
  if (axes.contains(composition::Axis::vertical)) {
    reset(ScrollEnvironment::vertical_scroll_indicator_visibility,
          composition::ScrollIndicatorVisibility::automatic);
    reset(ScrollEnvironment::vertical_scroll_bounce_behavior,
          composition::ScrollBounceBehavior::automatic);
  }
  return content;
}

auto ScrollView::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto ScrollView::Content::paints_like(
    const composition::ElementContent& other) const -> bool {
  if (other.type() != type()) {
    return false;
  }
  const auto& theirs = static_cast<const Content&>(other);
  return options_ == theirs.options_;
}

}
