#pragma once

#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/structure/group_box_style.hpp>

#include <memory>
#include <string>
#include <vector>

namespace arisa::structure {

class GroupBox : public composition::Element {
 public:
  class Content;

  explicit GroupBox(composition::Element content);

  GroupBox(std::string title, composition::Element content);

  GroupBox(composition::Element content, composition::Element label);
};

class GroupBox::Content final : public composition::ElementContent {
 public:
  using Source = std::shared_ptr<composition::PlaceholderContentView::Source>;

  Content(Source label, Source content) noexcept
      : label_(std::move(label)), content_(std::move(content)) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

 private:
  Source label_;
  Source content_;
};

}
