#pragma once

#include <cstdint>

namespace arisa::structure {

enum class TabBarPlacement : std::uint8_t {
  bottom_bar,
  top_bar,
  sidebar,
  ornament,
  page_indicator,
};

enum class AdaptableTabBarPlacement : std::uint8_t {
  automatic,
  tab_bar,
  sidebar,
};

enum class TabPlacement : std::uint8_t {
  automatic,
  pinned,
  sidebar_only,
};

enum class TabViewBottomAccessoryPlacement : std::uint8_t {
  inline_placement,
  expanded,
};

enum class TabSearchActivation : std::uint8_t {
  automatic,
  search_tab_selection,
};

enum class TabBarMinimizeBehavior : std::uint8_t {
  automatic,
  never,
  on_scroll_down,
  on_scroll_up,
};

enum class ContentToolbarPlacement : std::uint8_t {
  tab_view_sidebar,
};

}
