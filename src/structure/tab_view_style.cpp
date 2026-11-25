#include <arisa/structure/index_view_style.hpp>
#include <arisa/structure/tab_view_style.hpp>

namespace arisa::structure {

const composition::EnvironmentKey<IndexViewStyle> IndexViewStyle::key{
    "index-view-style", IndexViewStyle::page()};

const composition::EnvironmentKey<TabViewStyle> TabViewStyle::key{
    "tab_view_style", TabViewStyle::automatic()};

TabViewStyle::TabViewStyle(DefaultTabViewStyle) noexcept
    : kind_(Kind::tab_bar) {}

TabViewStyle::TabViewStyle(TabBarOnlyTabViewStyle) noexcept
    : kind_(Kind::tab_bar) {}

TabViewStyle::TabViewStyle(SidebarAdaptableTabViewStyle) noexcept
    : kind_(Kind::sidebar) {}

TabViewStyle::TabViewStyle(PageTabViewStyle style) noexcept
    : kind_(Kind::page), index_display_mode_(style.index_display_mode()) {}

auto TabViewStyle::page(IndexDisplayMode index_display_mode) noexcept
    -> TabViewStyle {
  return TabViewStyle{PageTabViewStyle{index_display_mode}};
}

auto TabViewStyle::automatic() noexcept -> TabViewStyle {
  return TabViewStyle{Kind::tab_bar};
}

auto TabViewStyle::tab_bar_only() noexcept -> TabViewStyle {
  return TabViewStyle{Kind::tab_bar};
}

auto TabViewStyle::sidebar_adaptable() noexcept -> TabViewStyle {
  return TabViewStyle{Kind::sidebar};
}

}
