#pragma once

#include <arisa/composition/binding.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/structure/disclosure_group_style.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace arisa::structure {

class DisclosureGroup : public composition::Element {
 public:
  class Content;

  using Builder = std::function<composition::Element()>;

  static constexpr float indent = DisclosureGroupStyle::indent;
  static constexpr float chevron_extent = DisclosureGroupStyle::chevron_extent;
  static constexpr double expanded_degrees =
      DisclosureGroupStyle::expanded_degrees;

  DisclosureGroup(std::string label, Builder content);

  DisclosureGroup(composition::Element label, Builder content);

  DisclosureGroup(std::string label, composition::Binding<bool> is_expanded,
                  Builder content);

  DisclosureGroup(composition::Element label,
                  composition::Binding<bool> is_expanded, Builder content);

 private:
  DisclosureGroup(composition::Element label,
                  std::optional<composition::Binding<bool>> is_expanded,
                  Builder content, bool);
};

class DisclosureGroup::Content final : public composition::ElementContent {
 public:
  using Source = std::shared_ptr<composition::PlaceholderContentView::Source>;

  static constexpr composition::StateKey<bool> expanded{
      "disclosure-group-expanded", false};

  Content(Source label, std::optional<composition::Binding<bool>> is_expanded,
          Builder content) noexcept
      : label_(std::move(label)),
        is_expanded_(std::move(is_expanded)),
        content_(std::move(content)) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

  auto label() const noexcept -> const Source& { return label_; }

  auto is_expanded() const noexcept
      -> const std::optional<composition::Binding<bool>>& {
    return is_expanded_;
  }

  auto builder() const noexcept -> const Builder& { return content_; }

 private:
  Source label_;
  std::optional<composition::Binding<bool>> is_expanded_;
  Builder content_;
};

}
