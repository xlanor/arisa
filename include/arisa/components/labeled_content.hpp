#pragma once

#include <arisa/components/label_column.hpp>
#include <arisa/components/labeled_content_style.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/placeholder_content_view.hpp>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace arisa::components {

class LabeledContent : public composition::Element {
 public:
  class Content;

  LabeledContent(std::string title, std::string value);

  LabeledContent(std::string title, composition::Element content);

  LabeledContent(composition::Element content, composition::Element label);

  static auto column(composition::Context& context)
      -> std::optional<LabelColumn>;

  static auto columned(const LabelColumn& column, composition::Element label,
                       composition::Element content) -> composition::Element;
};

class LabeledContent::Content final : public composition::ElementContent {
 public:
  using Source = std::shared_ptr<composition::PlaceholderContentView::Source>;

  struct Part {
    std::string text;
    Source source;

    auto element(bool secondary) const -> composition::Element;
  };

  Content(Part label, Part content) noexcept
      : label_(std::move(label)), content_(std::move(content)) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

 private:
  Part label_;
  Part content_;
};

}
