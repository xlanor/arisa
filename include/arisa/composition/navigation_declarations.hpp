#pragma once

#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/navigation_chrome.hpp>
#include <arisa/composition/navigation_split_view_column_width.hpp>
#include <arisa/composition/navigation_split_view_style.hpp>
#include <arisa/composition/navigation_transition.hpp>
#include <arisa/composition/preference_key.hpp>
#include <arisa/composition/presentation_background.hpp>
#include <arisa/composition/visibility.hpp>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace arisa::composition {

class NavigationDestination;
class ToolbarItem;

struct NavigationDeclarations {
  using Destinations =
      std::vector<std::shared_ptr<const NavigationDestination>>;
  using Toolbar = std::vector<std::shared_ptr<const ToolbarItem>>;

  static const PreferenceKey<std::optional<std::string>> title_key;
  static const PreferenceKey<Toolbar> toolbar_key;
  static const PreferenceKey<Destinations> destinations_key;
  static const PreferenceKey<std::optional<PresentationBackground>>
      background_key;
  static const PreferenceKey<NavigationChrome> chrome_key;
  static const PreferenceKey<std::optional<NavigationSplitViewColumnWidth>>
      column_width_key;
  static const EnvironmentKey<NavigationSplitViewStyle> split_view_style;
  static const EnvironmentKey<Visibility> link_indicator_visibility;

  static auto isolates(Identity preference) noexcept -> bool;

  std::optional<std::string> title;
  Destinations destinations;
  Toolbar toolbar;
  std::optional<PresentationBackground> container_background;
  NavigationChrome chrome;
  std::optional<NavigationTransition> transition;
};

}
