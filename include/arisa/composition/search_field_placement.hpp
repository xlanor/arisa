#pragma once

#include <cstdint>

namespace arisa::composition {

class SearchFieldPlacement {
 public:
  enum class Kind : std::uint8_t {
    automatic,
    toolbar,
    sidebar,
    navigation_bar_drawer,
    toolbar_principal,
  };

  enum class NavigationBarDrawerDisplayMode : std::uint8_t {
    automatic,
    always,
  };

  class Drawer;

  static const SearchFieldPlacement automatic;
  static const SearchFieldPlacement toolbar;
  static const SearchFieldPlacement sidebar;
  static const SearchFieldPlacement toolbar_principal;
  static const Drawer navigation_bar_drawer;

  constexpr auto kind() const noexcept -> Kind { return kind_; }

  constexpr auto display_mode() const noexcept
      -> NavigationBarDrawerDisplayMode {
    return mode_;
  }

  constexpr auto in_toolbar() const noexcept -> bool {
    return kind_ == Kind::toolbar || kind_ == Kind::toolbar_principal;
  }

  friend constexpr auto operator==(SearchFieldPlacement, SearchFieldPlacement)
      -> bool = default;

 private:
  constexpr SearchFieldPlacement(Kind kind,
                                 NavigationBarDrawerDisplayMode mode) noexcept
      : kind_(kind), mode_(mode) {}

  Kind kind_;
  NavigationBarDrawerDisplayMode mode_;
};

class SearchFieldPlacement::Drawer {
 public:
  constexpr Drawer() noexcept = default;

  constexpr operator SearchFieldPlacement() const noexcept {
    return SearchFieldPlacement{Kind::navigation_bar_drawer,
                                NavigationBarDrawerDisplayMode::automatic};
  }

  constexpr auto operator()(NavigationBarDrawerDisplayMode mode) const noexcept
      -> SearchFieldPlacement {
    return SearchFieldPlacement{Kind::navigation_bar_drawer, mode};
  }
};

inline constexpr SearchFieldPlacement SearchFieldPlacement::automatic{
    Kind::automatic, NavigationBarDrawerDisplayMode::automatic};
inline constexpr SearchFieldPlacement SearchFieldPlacement::toolbar{
    Kind::toolbar, NavigationBarDrawerDisplayMode::automatic};
inline constexpr SearchFieldPlacement SearchFieldPlacement::sidebar{
    Kind::sidebar, NavigationBarDrawerDisplayMode::automatic};
inline constexpr SearchFieldPlacement SearchFieldPlacement::toolbar_principal{
    Kind::toolbar_principal, NavigationBarDrawerDisplayMode::automatic};
inline constexpr SearchFieldPlacement::Drawer
    SearchFieldPlacement::navigation_bar_drawer{};

}
