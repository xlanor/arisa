#pragma once

#include <arisa/composition/axis.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/focus_binding.hpp>
#include <arisa/composition/focus_state.hpp>
#include <arisa/theme/colour_value.hpp>

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::structure {

class DockBar : public composition::Element {
 public:
  class Content;

  struct Item {
    std::string title;
    std::string system_image;
  };

  struct Action {
    std::string title;
    std::string system_image;
    std::function<void()> perform;
  };

  struct Options {
    composition::Axis axis{composition::Axis::horizontal};
    bool follows_focus{false};
    bool scrolls{false};
  };

  struct Metrics {
    float tab_width;
    float tab_height;
    float icon_tab_width;
    float icon_tab_height;
    float icon_size;
    float label_spacing;
    float tab_spacing;
    float padding;
    float divider_length;
  };

  static constexpr float tab_width = 86.0F;
  static constexpr float tab_height = 54.0F;
  static constexpr float icon_tab_width = 54.0F;
  static constexpr float icon_tab_height = 44.0F;
  static constexpr float icon_size = 18.0F;
  static constexpr float label_spacing = 6.0F;
  static constexpr float tab_spacing = 6.0F;
  static constexpr float padding = 7.0F;
  static constexpr float divider_height = 36.0F;
  static constexpr float divider_padding = 4.0F;
  static constexpr float divider_width = 1.0F;
  static constexpr theme::ColourRole selection_fill =
      theme::ColourRole::text_primary;
  static constexpr float selection_opacity = 0.12F;
  static constexpr theme::ColourRole rule_colour =
      theme::ColourRole::text_secondary;
  static constexpr float rule_opacity = 0.5F;
  static constexpr float shadow_opacity = 0.22F;
  static constexpr float shadow_radius = 20.0F;
  static constexpr float shadow_offset = 8.0F;
  static constexpr float compact_tab_width = 64.0F;
  static constexpr float compact_tab_height = 44.0F;
  static constexpr float compact_icon_tab_width = 44.0F;
  static constexpr float compact_icon_tab_height = 36.0F;
  static constexpr float compact_icon_size = 16.0F;
  static constexpr float compact_label_spacing = 3.0F;
  static constexpr float compact_tab_spacing = 4.0F;
  static constexpr float compact_padding = 5.0F;
  static constexpr float compact_divider_height = 28.0F;
  static constexpr float rail_radius = 24.0F;
  static constexpr std::string_view tab_key = "arisa.dock.tab";
  static constexpr std::string_view bar_key = "arisa.dock.bar";
  static constexpr std::string_view action_key = "arisa.dock.action";

  DockBar(composition::Binding<std::size_t> selection, std::vector<Item> items);

  DockBar(composition::Binding<std::size_t> selection, std::vector<Item> items,
          std::vector<Action> trailing);

  DockBar(composition::Binding<std::size_t> selection, std::vector<Item> items,
          std::vector<Action> trailing, Options options);

  static auto key_of(std::string_view kind, std::size_t index) -> std::string;

  static constexpr auto metrics(bool compact) noexcept -> Metrics {
    if (compact) {
      return Metrics{.tab_width = compact_tab_width,
                     .tab_height = compact_tab_height,
                     .icon_tab_width = compact_icon_tab_width,
                     .icon_tab_height = compact_icon_tab_height,
                     .icon_size = compact_icon_size,
                     .label_spacing = compact_label_spacing,
                     .tab_spacing = compact_tab_spacing,
                     .padding = compact_padding,
                     .divider_length = compact_divider_height};
    }
    return Metrics{.tab_width = tab_width,
                   .tab_height = tab_height,
                   .icon_tab_width = icon_tab_width,
                   .icon_tab_height = icon_tab_height,
                   .icon_size = icon_size,
                   .label_spacing = label_spacing,
                   .tab_spacing = tab_spacing,
                   .padding = padding,
                   .divider_length = divider_height};
  }

  static constexpr composition::FocusState<std::size_t> tab_focus{
      "arisa-dock-tab-focus"};
};

class DockBar::Content final : public composition::ElementContent {
 public:
  Content(composition::Binding<std::size_t> selection, std::vector<Item> items,
          std::vector<Action> trailing, Options options) noexcept
      : selection_(std::move(selection)),
        items_(std::move(items)),
        trailing_(std::move(trailing)),
        options_(options) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

  auto items() const noexcept -> const std::vector<Item>& { return items_; }

  auto options() const noexcept -> Options { return options_; }

 private:
  static auto label(const std::string& title, const std::string& symbol,
                    bool labelled, const Metrics& sizes)
      -> composition::Element;

  auto tab(std::size_t index, bool selected, bool labelled,
           const Metrics& sizes,
           const composition::FocusBinding<std::size_t>& focus) const
      -> composition::Element;

  static auto action(const Action& chosen, std::size_t index, bool labelled,
                     const Metrics& sizes) -> composition::Element;

  auto divider(const Metrics& sizes) const -> composition::Element;

  auto row_width(bool labelled, const Metrics& sizes) const noexcept -> float;

  composition::Binding<std::size_t> selection_;
  std::vector<Item> items_;
  std::vector<Action> trailing_;
  Options options_;
};

}
