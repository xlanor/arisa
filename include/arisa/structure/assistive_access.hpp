#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/placeholder_content_view.hpp>

#include <functional>
#include <memory>
#include <utility>

namespace arisa::composition {

class Context;

}

namespace arisa::structure {

class AssistiveAccess {
 public:
  class Host;

  using Content = std::function<composition::Element()>;

  explicit AssistiveAccess(Content content)
      : content_(std::make_shared<const Content>(std::move(content))) {}

  auto content() const -> composition::Element;

  static auto standard_key() -> composition::ElementKey;

  static auto scene_key() -> composition::ElementKey;

  friend auto operator==(const AssistiveAccess&, const AssistiveAccess&)
      -> bool = default;

 private:
  std::shared_ptr<const Content> content_;
};

class AssistiveAccess::Host final : public composition::Component {
 public:
  Host(composition::Element standard, AssistiveAccess scene)
      : source_(std::make_shared<composition::PlaceholderContentView::Source>(
            std::move(standard))),
        scene_(std::move(scene)) {}

  auto operator==(const Host&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  std::shared_ptr<composition::PlaceholderContentView::Source> source_;
  AssistiveAccess scene_;
};

}

namespace arisa::composition {

template <class Scene>
auto Element::assistive_access(Scene scene) && -> Element {
  return structure::AssistiveAccess::Host{std::move(*this),
                                          structure::AssistiveAccess{scene}};
}

}
