#pragma once

#include <arisa/components/button_style.hpp>
#include <arisa/components/image.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/controls/menu_style.hpp>
#include <arisa/presentation/menu_panel.hpp>
#include <arisa/theme/space.hpp>

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::controls {

class Menu : public composition::Element {
 public:
  class Content;

  using Builder = std::function<composition::Element()>;

  using Action = std::function<void()>;

  using ItemStyle = presentation::MenuPanel::ItemStyle;

  static constexpr float panel_width = presentation::MenuPanel::panel_width;
  static constexpr float chevron_extent = 10.0F;
  static constexpr std::string_view anchor_key = "arisa.menu.anchor";

  Menu(std::string title, Builder content);

  Menu(Builder content, composition::Element label);

  Menu(std::string title, components::Image::SystemName system_image,
       Builder content);

  Menu(std::string title, components::Image::Resource image, Builder content);

  Menu(std::string title, Builder content, Action primary_action);

  Menu(Builder content, composition::Element label, Action primary_action);

  static constexpr std::string_view menu_label = "Menu";

  static auto items(composition::Element declared,
                    composition::Binding<bool> shown) -> composition::Element;

 private:
  Menu(std::optional<std::string> title, Builder content,
       std::optional<composition::Element> label, Action primary_action);
};

class Menu::Content final : public composition::ElementContent {
 public:
  using Source = std::shared_ptr<composition::PlaceholderContentView::Source>;

  static constexpr composition::StateKey<bool> shown{"menu-shown", false};

  static constexpr composition::StateKey<std::optional<theme::Edge>> placed{
      "menu-placed", std::nullopt};

  Content(std::optional<std::string> title, Builder content, Source label,
          Action primary_action) noexcept
      : title_(std::move(title)),
        content_(std::move(content)),
        label_(std::move(label)),
        primary_action_(std::move(primary_action)) {}

  auto has_primary_action() const noexcept -> bool {
    return static_cast<bool>(primary_action_);
  }

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

 private:
  std::optional<std::string> title_;
  Builder content_;
  Source label_;
  Action primary_action_;
};

}
