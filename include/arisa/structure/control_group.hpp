#pragma once

#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/structure/control_group_style.hpp>

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace arisa::structure {

class ControlGroup : public composition::Element {
 public:
  class Content;

  using Builder = std::function<composition::Element()>;

  explicit ControlGroup(Builder content);

  ControlGroup(std::string title, Builder content);

  ControlGroup(Builder content, composition::Element label);
};

class ControlGroup::Content final : public composition::ElementContent {
 public:
  using Source = std::shared_ptr<composition::PlaceholderContentView::Source>;

  Content(Source label, Builder content) noexcept
      : label_(std::move(label)), content_(std::move(content)) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

 private:
  Source label_;
  Builder content_;
};

}
