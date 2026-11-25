#pragma once

#include <arisa/composition/element.hpp>
#include <arisa/composition/navigation_path.hpp>

#include <concepts>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace arisa::composition {

template <class T>
inline constexpr std::type_identity<T> for_type{};

class NavigationDestination {
 public:
  template <class T, class Build>
  class Typed;

  NavigationDestination() = default;
  NavigationDestination(const NavigationDestination&) = delete;
  auto operator=(const NavigationDestination&)
      -> NavigationDestination& = delete;
  NavigationDestination(NavigationDestination&&) = delete;
  auto operator=(NavigationDestination&&) -> NavigationDestination& = delete;
  virtual ~NavigationDestination() = default;

  virtual auto accepts(const NavigationPath::Entry& entry) const -> bool = 0;

  virtual auto build(const NavigationPath::Entry& entry) const -> Element = 0;
};

template <class T, class Build>
class NavigationDestination::Typed final : public NavigationDestination {
 public:
  explicit Typed(Build build) : build_(std::move(build)) {}

  auto accepts(const NavigationPath::Entry& entry) const -> bool override {
    return entry.get<T>() != nullptr;
  }

  auto build(const NavigationPath::Entry& entry) const -> Element override {
    return Element{std::invoke(build_, *entry.get<T>())};
  }

 private:
  Build build_;
};

template <class T, class Build>
  requires std::convertible_to<std::invoke_result_t<const Build&, const T&>,
                               Element>
auto Element::navigation_destination(std::type_identity<T>,
                                     Build build) && -> Element {
  navigation_.edit().destinations.push_back(
      std::make_shared<const NavigationDestination::Typed<T, Build>>(
          std::move(build)));
  return std::move(*this);
}

}
