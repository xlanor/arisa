#pragma once

#include <arisa/composition/element.hpp>
#include <arisa/composition/environment_key.hpp>

#include <cstdint>
#include <utility>

namespace arisa::structure {

class DefaultTabViewStyle;
class TabBarOnlyTabViewStyle;
class SidebarAdaptableTabViewStyle;
class PageTabViewStyle;

class TabViewStyle {
 public:
  enum class Kind : std::uint8_t {
    tab_bar,
    sidebar,
    page,
  };

  enum class IndexDisplayMode : std::uint8_t {
    automatic,
    always,
    never,
  };

  static const composition::EnvironmentKey<TabViewStyle> key;

  TabViewStyle(DefaultTabViewStyle style) noexcept;

  TabViewStyle(TabBarOnlyTabViewStyle style) noexcept;

  TabViewStyle(SidebarAdaptableTabViewStyle style) noexcept;

  TabViewStyle(PageTabViewStyle style) noexcept;

  static auto automatic() noexcept -> TabViewStyle;

  static auto tab_bar_only() noexcept -> TabViewStyle;

  static auto sidebar_adaptable() noexcept -> TabViewStyle;

  static auto page(IndexDisplayMode index_display_mode =
                       IndexDisplayMode::automatic) noexcept -> TabViewStyle;

  auto kind() const noexcept -> Kind { return kind_; }

  auto index_display_mode() const noexcept -> IndexDisplayMode {
    return index_display_mode_;
  }

  friend auto operator==(const TabViewStyle&, const TabViewStyle&)
      -> bool = default;

 private:
  explicit TabViewStyle(Kind kind) noexcept : kind_(kind) {}

  Kind kind_;
  IndexDisplayMode index_display_mode_{IndexDisplayMode::automatic};
};

class DefaultTabViewStyle {
 public:
  friend auto operator==(const DefaultTabViewStyle&, const DefaultTabViewStyle&)
      -> bool = default;
};

class TabBarOnlyTabViewStyle {
 public:
  friend auto operator==(const TabBarOnlyTabViewStyle&,
                         const TabBarOnlyTabViewStyle&) -> bool = default;
};

class SidebarAdaptableTabViewStyle {
 public:
  friend auto operator==(const SidebarAdaptableTabViewStyle&,
                         const SidebarAdaptableTabViewStyle&) -> bool = default;
};

class PageTabViewStyle {
 public:
  using IndexDisplayMode = TabViewStyle::IndexDisplayMode;

  constexpr explicit PageTabViewStyle(IndexDisplayMode index_display_mode =
                                          IndexDisplayMode::automatic) noexcept
      : index_display_mode_(index_display_mode) {}

  constexpr auto index_display_mode() const noexcept -> IndexDisplayMode {
    return index_display_mode_;
  }

  friend constexpr auto operator==(const PageTabViewStyle&,
                                   const PageTabViewStyle&) -> bool = default;

 private:
  IndexDisplayMode index_display_mode_;
};

}

namespace arisa::composition {

template <class Style>
auto Element::tab_view_style(Style style) && -> Element {
  return std::move(*this).environment(
      structure::TabViewStyle::key, structure::TabViewStyle{std::move(style)});
}

}
