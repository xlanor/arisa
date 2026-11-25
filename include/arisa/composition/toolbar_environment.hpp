#pragma once

#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/composition/toolbar_title_alignment.hpp>
#include <arisa/composition/vertical_edge.hpp>

#include <memory>
#include <optional>
#include <vector>

namespace arisa::composition {

class ToolbarEnvironment {
 public:
  class HiddenItem;

  static const EnvironmentKey<ToolbarTitleAlignment> title_alignment;
  static const EnvironmentKey<std::optional<VerticalEdge>> vertical_edge;
  static const EnvironmentKey<bool> customizing;
  static const EnvironmentKey<bool> compressed;
};

class ToolbarEnvironment::HiddenItem : public Element {
 public:
  class Content;

  explicit HiddenItem(Element item);
};

class ToolbarEnvironment::HiddenItem::Content final : public ElementContent {
 public:
  explicit Content(
      std::shared_ptr<PlaceholderContentView::Source> item) noexcept
      : item_(std::move(item)) {}

  auto type() const noexcept -> ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(Context& context) const -> std::vector<Element> override;

 private:
  std::shared_ptr<PlaceholderContentView::Source> item_;
};

}
