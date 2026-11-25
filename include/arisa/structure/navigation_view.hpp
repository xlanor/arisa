#pragma once

#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/placeholder_content_view.hpp>

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace arisa::structure {

class DefaultNavigationViewStyle;
class StackNavigationViewStyle;
class DoubleColumnNavigationViewStyle;

class NavigationViewStyle {
 public:
  enum class Kind : std::uint8_t {
    automatic,
    stack,
    columns,
  };

  static const NavigationViewStyle automatic;
  static const NavigationViewStyle stack;
  static const NavigationViewStyle columns;
  static const composition::EnvironmentKey<NavigationViewStyle> key;

  NavigationViewStyle(DefaultNavigationViewStyle style) noexcept;

  NavigationViewStyle(StackNavigationViewStyle style) noexcept;

  NavigationViewStyle(DoubleColumnNavigationViewStyle style) noexcept;

  constexpr auto kind() const noexcept -> Kind { return kind_; }

  friend constexpr auto operator==(NavigationViewStyle, NavigationViewStyle)
      -> bool = default;

 private:
  constexpr explicit NavigationViewStyle(Kind kind) noexcept : kind_(kind) {}

  Kind kind_;
};

inline constexpr NavigationViewStyle NavigationViewStyle::automatic{
    Kind::automatic};
inline constexpr NavigationViewStyle NavigationViewStyle::stack{Kind::stack};
inline constexpr NavigationViewStyle NavigationViewStyle::columns{
    Kind::columns};

class DefaultNavigationViewStyle {
 public:
  friend auto operator==(const DefaultNavigationViewStyle&,
                         const DefaultNavigationViewStyle&) -> bool = default;
};

class StackNavigationViewStyle {
 public:
  friend auto operator==(const StackNavigationViewStyle&,
                         const StackNavigationViewStyle&) -> bool = default;
};

class DoubleColumnNavigationViewStyle {
 public:
  friend auto operator==(const DoubleColumnNavigationViewStyle&,
                         const DoubleColumnNavigationViewStyle&)
      -> bool = default;
};

class NavigationView : public composition::Element {
 public:
  class Content;

  template <class... Children>
    requires(composition::ViewContent<Children> && ...)
  explicit NavigationView(Children&&... children)
      : NavigationView(collect(std::forward<Children>(children)...)) {}

  explicit NavigationView(std::vector<composition::Element> children);
};

class NavigationView::Content final : public composition::ElementContent {
 public:
  using Source = std::shared_ptr<composition::PlaceholderContentView::Source>;

  explicit Content(std::vector<Source> columns) noexcept
      : columns_(std::move(columns)) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto hosts_sidebar() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

  static auto shows_columns(NavigationViewStyle style, std::size_t count,
                            bool compact) noexcept -> bool;

 private:
  std::vector<Source> columns_;
};

}

namespace arisa::composition {

template <class Style>
auto Element::navigation_view_style(Style style) && -> Element {
  return std::move(*this).environment(structure::NavigationViewStyle::key,
                                      structure::NavigationViewStyle{style});
}

}
