#pragma once

#include <arisa/composition/scroll_edge_effect_style.hpp>
#include <arisa/composition/toolbar_minimization.hpp>
#include <arisa/composition/toolbar_placement.hpp>
#include <arisa/composition/toolbar_role.hpp>
#include <arisa/composition/toolbar_title_alignment.hpp>
#include <arisa/composition/toolbar_title_display_mode.hpp>
#include <arisa/composition/toolbar_vertical_behavior.hpp>
#include <arisa/composition/visibility.hpp>
#include <arisa/theme/color_scheme.hpp>
#include <arisa/theme/colour_value.hpp>

#include <optional>
#include <string>

namespace arisa::composition {

class NavigationChrome {
 public:
  struct Bar {
    std::optional<Visibility> visibility;
    std::optional<Visibility> background_visibility;
    std::optional<theme::ColourValue> background;
    std::optional<theme::ColorScheme> color_scheme;
    std::optional<theme::ColourValue> foreground;
    std::optional<ToolbarMinimizationBehavior> minimization;
    std::optional<ToolbarMinimizationRestoration> restoration;
    std::optional<ToolbarMinimizationSafeAreaAdjustment> safe_area_adjustment;
    std::optional<ScrollEdgeEffectStyle> scroll_edge_effect_style;
    std::optional<bool> scroll_edge_effect_hidden;

    auto hidden() const noexcept -> bool {
      return visibility == Visibility::hidden;
    }

    friend auto operator==(const Bar&, const Bar&) -> bool = default;
  };

  auto subtitle() const noexcept -> const std::optional<std::string>& {
    return subtitle_;
  }

  auto title_display_mode() const noexcept -> ToolbarTitleDisplayMode {
    return title_display_mode_.value_or(ToolbarTitleDisplayMode::automatic);
  }

  auto back_button_hidden() const noexcept -> bool {
    return back_button_hidden_.value_or(false);
  }

  auto role() const noexcept -> ToolbarRole {
    return role_.value_or(ToolbarRole::automatic);
  }

  auto title_alignment() const noexcept -> ToolbarTitleAlignment {
    return title_alignment_.value_or(ToolbarTitleAlignment::automatic);
  }

  auto vertical_behavior() const noexcept -> ToolbarVerticalBehavior {
    return vertical_behavior_.value_or(ToolbarVerticalBehavior::automatic);
  }

  auto vertical_compression() const noexcept
      -> ToolbarVerticalCompressionBehavior {
    return vertical_compression_.value_or(
        ToolbarVerticalCompressionBehavior::automatic);
  }

  auto declares_title_display_mode() const noexcept -> bool {
    return title_display_mode_.has_value();
  }

  auto bar(ToolbarPlacement placement) const noexcept -> const Bar&;

  void set_subtitle(std::string subtitle) { subtitle_ = std::move(subtitle); }

  void set_title_display_mode(ToolbarTitleDisplayMode mode) noexcept {
    title_display_mode_ = mode;
  }

  void set_back_button_hidden(bool hidden) noexcept {
    back_button_hidden_ = hidden;
  }

  void set_role(ToolbarRole role) noexcept { role_ = role; }

  void set_title_alignment(ToolbarTitleAlignment alignment) noexcept {
    title_alignment_ = alignment;
  }

  void set_vertical_behavior(ToolbarVerticalBehavior behavior) noexcept {
    vertical_behavior_ = behavior;
  }

  void set_vertical_compression(
      ToolbarVerticalCompressionBehavior behavior) noexcept {
    vertical_compression_ = behavior;
  }

  auto edit_bar(ToolbarPlacement placement) noexcept -> Bar&;

  void override_with(const NavigationChrome& outer);

  auto empty() const noexcept -> bool;

  friend auto operator==(const NavigationChrome&, const NavigationChrome&)
      -> bool = default;

 private:
  static void override_bar(Bar& inner, const Bar& outer);

  std::optional<std::string> subtitle_;
  std::optional<ToolbarTitleDisplayMode> title_display_mode_;
  std::optional<bool> back_button_hidden_;
  std::optional<ToolbarRole> role_;
  std::optional<ToolbarTitleAlignment> title_alignment_;
  std::optional<ToolbarVerticalBehavior> vertical_behavior_;
  std::optional<ToolbarVerticalCompressionBehavior> vertical_compression_;
  Bar navigation_bar_;
  Bar bottom_bar_;
  Bar tab_bar_;
  Bar status_bar_;
};

}
