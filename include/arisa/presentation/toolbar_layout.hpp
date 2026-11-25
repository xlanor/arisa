#pragma once

#include <arisa/composition/binding.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/navigation_chrome.hpp>
#include <arisa/composition/navigation_declarations.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/toolbar_customization.hpp>
#include <arisa/composition/toolbar_item.hpp>
#include <arisa/composition/toolbar_role.hpp>
#include <arisa/composition/toolbar_title_display_mode.hpp>
#include <arisa/theme/theme.hpp>
#include <arisa/theme/typography.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::presentation {

class ToolbarLayout {
 public:
  using Items = composition::NavigationDeclarations::Toolbar;
  using Source = composition::ToolbarItem::Source;
  using Builder = composition::ToolbarItem::Builder;
  using Customization = composition::Binding<composition::ToolbarCustomization>;

  enum class Side : std::uint8_t {
    leading,
    principal,
    trailing,
    bottom,
    status,
    vertical,
    title,
    subtitle,
    large_title,
    large_subtitle,
    keyboard,
  };

  struct Segment {
    std::vector<composition::Element> items;
    bool flexible_before{false};
    bool bare{false};
    bool tight{false};
  };

  struct Metrics {
    float height{0.0F};
    float padding{0.0F};
    float spacing{0.0F};
    float inset{0.0F};
    float gap{0.0F};
  };

  struct Options {
    std::optional<Customization> customization;
    bool vertical{false};
  };

  class Overflow;
  class TitleMenu;
  class Fitted;
  class Editor;

  static constexpr std::string_view overflow_key = "arisa.toolbar.overflow";
  static constexpr std::string_view title_menu_key = "arisa.toolbar.title-menu";
  static constexpr std::string_view overflow_symbol = "more_horiz";
  static constexpr std::string_view customize_key = "arisa.toolbar.customize";
  static constexpr std::string_view customize_label = "Customize Toolbar";
  static constexpr std::string_view editor_key = "arisa.toolbar.editor";
  static constexpr std::string_view editor_done_label = "Done";
  static constexpr std::string_view editor_reset_label = "Reset";
  static constexpr std::string_view fitted_key = "arisa.toolbar.fitted";
  static constexpr std::string_view vertical_key = "arisa.toolbar.vertical";
  static constexpr float title_gap = 8.0F;

  ToolbarLayout(Items items, composition::ToolbarRole role);

  ToolbarLayout(Items items, composition::ToolbarRole role, Options options);

  auto has(Side side) const -> bool;

  auto segments(Side side) const -> std::vector<Segment>;

  auto flattened(Side side, float gap) const
      -> std::vector<composition::Element>;

  auto overflow() const -> std::optional<composition::Element>;

  auto fitted(float gap) const -> std::optional<composition::Element>;

  auto title_menu() const -> Builder;

  auto relocated(composition::ToolbarDefaultItemKind kind) const
      -> std::optional<composition::ToolbarItemPlacement>;

  auto vertical() const noexcept -> bool { return options_.vertical; }

  static auto customization_id(const Items& items)
      -> std::shared_ptr<const std::string>;

  static auto storage_key(const std::string& id) -> std::string;

  static auto prefers_vertical(const Items& items) -> bool;

  auto titled(std::optional<std::string> title,
              const std::optional<std::string>& subtitle, theme::Font font,
              bool large) const -> std::optional<composition::Element>;

  static auto heading(std::optional<std::string> title,
                      const std::optional<std::string>& subtitle,
                      theme::Font font, Builder menu)
      -> std::optional<composition::Element>;

  static auto title_font(composition::ToolbarTitleDisplayMode mode,
                         theme::Font inline_font) noexcept -> theme::Font;

  static auto metrics(const theme::Theme& current) noexcept -> Metrics;

  static auto styled(composition::Element bar,
                     const composition::NavigationChrome::Bar& style)
      -> composition::Element;

  static auto group(std::vector<composition::Element> items,
                    const Metrics& metrics) -> composition::Element;

  static auto group(Segment segment, const Metrics& metrics)
      -> composition::Element;

  static void capsules(std::vector<composition::Element>& items);

  auto grouped(Side side, const Metrics& metrics) const
      -> std::vector<composition::Element>;

 private:
  auto matches(const composition::ToolbarItem& item, Side side) const noexcept
      -> bool;

  auto shown_in_bar(const composition::ToolbarItem& item) const -> bool;

  static auto shown(const composition::ToolbarItem& item, Side side)
      -> composition::Element;

  static auto overflows(const composition::ToolbarItem& item) noexcept -> bool;

  void relocate();

  Items items_;
  Items available_;
  composition::ToolbarRole role_;
  Options options_;
  std::vector<std::pair<composition::ToolbarDefaultItemKind,
                        composition::ToolbarItemPlacement>>
      relocations_;
};

class ToolbarLayout::Overflow final : public composition::Component {
 public:
  static constexpr composition::StateKey<bool> shown{"toolbar-overflow", false};
  static constexpr composition::StateKey<bool> editing{"toolbar-editing",
                                                       false};

  Overflow(Items items, std::optional<Customization> customization,
           Items customizable, Source label = nullptr) noexcept
      : items_(std::move(items)),
        customization_(std::move(customization)),
        customizable_(std::move(customizable)),
        label_(std::move(label)) {}

  auto operator==(const Overflow& other) const -> bool;

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  Items items_;
  std::optional<Customization> customization_;
  Items customizable_;
  Source label_;
};

class ToolbarLayout::Editor final : public composition::Component {
 public:
  Editor(Customization customization, Items items,
         composition::StateHandle<bool> editing) noexcept
      : customization_(std::move(customization)),
        items_(std::move(items)),
        editing_(std::move(editing)) {}

  auto operator==(const Editor& other) const -> bool;

  auto body(composition::Context& context) const
      -> composition::Element override;

  static auto row_key(std::string_view id) -> composition::ElementKey;

 private:
  Customization customization_;
  Items items_;
  composition::StateHandle<bool> editing_;
};

class ToolbarLayout::Fitted final : public composition::Component {
 public:
  struct Unit {
    Items items;
    composition::ToolbarItemVisibilityPriority priority;
    bool pinned{false};
    bool flexible_before{false};
    Source label;

    friend auto operator==(const Unit&, const Unit&) -> bool = default;
  };

  static const composition::StateKey<std::vector<float>> widths;
  static constexpr composition::StateKey<float> room{"toolbar-fitted-room",
                                                     0.0F};
  static constexpr composition::StateKey<float> more_width{
      "toolbar-fitted-more", 0.0F};

  Fitted(std::vector<Unit> units, Items overflow,
         std::optional<Customization> customization, Items customizable,
         float gap) noexcept
      : units_(std::move(units)),
        overflow_(std::move(overflow)),
        customization_(std::move(customization)),
        customizable_(std::move(customizable)),
        gap_(gap) {}

  auto operator==(const Fitted& other) const -> bool;

  auto body(composition::Context& context) const
      -> composition::Element override;

  static auto kept(const std::vector<Unit>& units,
                   const std::vector<float>& measured, float space, float more,
                   float gap, bool overflowing) -> std::vector<bool>;

 private:
  static auto measured(composition::Element unit, std::size_t index,
                       composition::StateHandle<std::vector<float>> sizes)
      -> composition::Element;

  static auto collapsed(const Unit& unit) -> composition::Element;

  std::vector<Unit> units_;
  Items overflow_;
  std::optional<Customization> customization_;
  Items customizable_;
  float gap_;
};

class ToolbarLayout::TitleMenu final : public composition::Component {
 public:
  static constexpr composition::StateKey<bool> shown{"toolbar-title-menu",
                                                     false};

  TitleMenu(composition::Element title, Builder menu);

  auto operator==(const TitleMenu& other) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  Source title_;
  Builder menu_;
};

}
