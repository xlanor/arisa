#include <arisa/composition/navigation_chrome.hpp>

namespace arisa::composition {

auto NavigationChrome::bar(ToolbarPlacement placement) const noexcept
    -> const Bar& {
  switch (placement) {
    case ToolbarPlacement::bottom_bar:
      return bottom_bar_;
    case ToolbarPlacement::tab_bar:
      return tab_bar_;
    case ToolbarPlacement::status_bar:
      return status_bar_;
    case ToolbarPlacement::automatic:
    case ToolbarPlacement::navigation_bar:
      break;
  }
  return navigation_bar_;
}

auto NavigationChrome::edit_bar(ToolbarPlacement placement) noexcept -> Bar& {
  switch (placement) {
    case ToolbarPlacement::bottom_bar:
      return bottom_bar_;
    case ToolbarPlacement::tab_bar:
      return tab_bar_;
    case ToolbarPlacement::status_bar:
      return status_bar_;
    case ToolbarPlacement::automatic:
    case ToolbarPlacement::navigation_bar:
      break;
  }
  return navigation_bar_;
}

void NavigationChrome::override_bar(Bar& inner, const Bar& outer) {
  if (outer.visibility.has_value()) {
    inner.visibility = outer.visibility;
  }
  if (outer.background_visibility.has_value()) {
    inner.background_visibility = outer.background_visibility;
  }
  if (outer.background.has_value()) {
    inner.background = outer.background;
  }
  if (outer.color_scheme.has_value()) {
    inner.color_scheme = outer.color_scheme;
  }
  if (outer.foreground.has_value()) {
    inner.foreground = outer.foreground;
  }
  if (outer.minimization.has_value()) {
    inner.minimization = outer.minimization;
  }
  if (outer.restoration.has_value()) {
    inner.restoration = outer.restoration;
  }
  if (outer.safe_area_adjustment.has_value()) {
    inner.safe_area_adjustment = outer.safe_area_adjustment;
  }
  if (outer.scroll_edge_effect_style.has_value()) {
    inner.scroll_edge_effect_style = outer.scroll_edge_effect_style;
  }
  if (outer.scroll_edge_effect_hidden.has_value()) {
    inner.scroll_edge_effect_hidden = outer.scroll_edge_effect_hidden;
  }
}

void NavigationChrome::override_with(const NavigationChrome& outer) {
  if (outer.subtitle_.has_value()) {
    subtitle_ = outer.subtitle_;
  }
  if (outer.title_display_mode_.has_value()) {
    title_display_mode_ = outer.title_display_mode_;
  }
  if (outer.back_button_hidden_.has_value()) {
    back_button_hidden_ = outer.back_button_hidden_;
  }
  if (outer.role_.has_value()) {
    role_ = outer.role_;
  }
  if (outer.title_alignment_.has_value()) {
    title_alignment_ = outer.title_alignment_;
  }
  if (outer.vertical_behavior_.has_value()) {
    vertical_behavior_ = outer.vertical_behavior_;
  }
  if (outer.vertical_compression_.has_value()) {
    vertical_compression_ = outer.vertical_compression_;
  }
  override_bar(navigation_bar_, outer.navigation_bar_);
  override_bar(bottom_bar_, outer.bottom_bar_);
  override_bar(tab_bar_, outer.tab_bar_);
  override_bar(status_bar_, outer.status_bar_);
}

auto NavigationChrome::empty() const noexcept -> bool {
  return *this == NavigationChrome{};
}

}
