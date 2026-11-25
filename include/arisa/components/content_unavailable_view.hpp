#pragma once

#include <arisa/components/image.hpp>
#include <arisa/components/label_style.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/theme/typography.hpp>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace arisa::components {

class ContentUnavailableView : public composition::Element {
 public:
  class Content;
  class StackedLabelStyle;

  static constexpr float icon_size = 48.0F;
  static constexpr float icon_height = 48.0F;
  static constexpr float label_spacing = 11.0F;
  static constexpr float description_spacing = 6.0F;
  static constexpr float actions_spacing = 18.0F;
  static constexpr float insets = 20.0F;
  static constexpr std::string_view search_symbol = "search";

  ContentUnavailableView(std::string title, std::string system_image);

  ContentUnavailableView(std::string title, std::string system_image,
                         composition::Element description);

  ContentUnavailableView(
      std::string title, Image::Resource image,
      std::optional<composition::Element> description = std::nullopt);

  ContentUnavailableView(composition::Element label,
                         std::optional<composition::Element> description,
                         std::optional<composition::Element> actions);

  static auto search() -> ContentUnavailableView;

  static auto search(std::string text) -> ContentUnavailableView;
};

class ContentUnavailableView::StackedLabelStyle {
 public:
  explicit StackedLabelStyle(theme::Font title) noexcept : title_(title) {}

  auto make_body(LabelStyle::Configuration configuration) const
      -> composition::Element;

  friend auto operator==(const StackedLabelStyle&, const StackedLabelStyle&)
      -> bool = default;

 private:
  theme::Font title_;
};

class ContentUnavailableView::Content final
    : public composition::ElementContent {
 public:
  using Source = std::shared_ptr<composition::PlaceholderContentView::Source>;

  struct Parts {
    std::optional<std::string> title;
    std::optional<std::string> symbol;
    Source label;
    Source description;
    Source actions;
  };

  explicit Content(Parts parts) noexcept : parts_(std::move(parts)) {}

  static auto source(std::optional<composition::Element> element) -> Source;

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto same_as(const composition::ElementContent& other) const -> bool override;

  auto adopt(composition::ElementContent& incoming) -> bool override;

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

 private:
  auto label(composition::Context& context) const -> composition::Element;

  Parts parts_;
};

}
