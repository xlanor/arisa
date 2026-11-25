#pragma once

#include <arisa/components/image.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/placeholder_content_view.hpp>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace arisa::controls {

class Toggle : public composition::Element {
 public:
  class Content;

  Toggle(std::string title, composition::Binding<bool> is_on);

  Toggle(composition::Binding<bool> is_on, composition::Element label);

  Toggle(std::string title, components::Image::SystemName system_image,
         composition::Binding<bool> is_on);

  Toggle(std::string title, components::Image::Resource image,
         composition::Binding<bool> is_on);

  Toggle(std::string title, std::vector<composition::Binding<bool>> sources);

  Toggle(std::string title, components::Image::SystemName system_image,
         std::vector<composition::Binding<bool>> sources);

  Toggle(std::string title, components::Image::Resource image,
         std::vector<composition::Binding<bool>> sources);

  Toggle(std::vector<composition::Binding<bool>> sources,
         composition::Element label);

  static auto mixed(const std::vector<composition::Binding<bool>>& sources)
      -> composition::Binding<bool>;

  static auto is_mixed(const std::vector<composition::Binding<bool>>& sources)
      -> bool;

 private:
  Toggle(std::optional<std::string> title, composition::Binding<bool> is_on,
         std::optional<composition::Element> label);

  Toggle(std::optional<std::string> title,
         std::vector<composition::Binding<bool>> sources,
         std::optional<composition::Element> label);
};

class Toggle::Content final : public composition::ElementContent {
 public:
  using Source = std::shared_ptr<composition::PlaceholderContentView::Source>;

  Content(std::optional<std::string> title, composition::Binding<bool> is_on,
          Source label,
          std::vector<composition::Binding<bool>> sources = {}) noexcept
      : title_(std::move(title)),
        is_on_(std::move(is_on)),
        label_(std::move(label)),
        sources_(std::move(sources)) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto framed() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

  auto activate() -> bool override;

  auto activatable() const noexcept -> bool override { return true; }

  auto accessibility() const -> composition::AccessibilityDefaults override;

 private:
  std::optional<std::string> title_;
  composition::Binding<bool> is_on_;
  Source label_;
  std::vector<composition::Binding<bool>> sources_;
};

}
