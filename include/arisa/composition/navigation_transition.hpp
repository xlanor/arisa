#pragma once

#include <arisa/composition/element_key.hpp>
#include <arisa/composition/matched_geometry.hpp>
#include <arisa/composition/namespace.hpp>

#include <optional>
#include <utility>

namespace arisa::composition {

class NavigationTransition {
 public:
  static auto automatic() -> NavigationTransition {
    return NavigationTransition{std::nullopt};
  }

  static auto zoom(ElementKey source_id, Namespace::ID space)
      -> NavigationTransition {
    return NavigationTransition{
        MatchedGeometry::Key{.space = space, .id = std::move(source_id)}};
  }

  static auto cross_fade() -> NavigationTransition {
    NavigationTransition made{std::nullopt};
    made.fades_ = true;
    return made;
  }

  auto fades() const noexcept -> bool { return fades_; }

  auto zoom_source() const noexcept
      -> const std::optional<MatchedGeometry::Key>& {
    return source_;
  }

  friend auto operator==(const NavigationTransition&,
                         const NavigationTransition&) -> bool = default;

 private:
  explicit NavigationTransition(std::optional<MatchedGeometry::Key> source)
      : source_(std::move(source)) {}

  std::optional<MatchedGeometry::Key> source_;
  bool fades_{false};
};

using CrossFadeNavigationTransition = NavigationTransition;

}
