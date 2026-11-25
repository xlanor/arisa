#pragma once

#include <arisa/components/button.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/navigation_path.hpp>
#include <arisa/presentation/navigation_stack.hpp>

#include <concepts>
#include <memory>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace arisa::presentation {

class NavigationLink : public components::Button {
 public:
  class Probe;

  using Destination = NavigationStack::ViewDestination::Build;

  template <class T>
    requires std::equality_comparable<T> && std::copy_constructible<T> &&
             (!std::is_invocable_v<const T&>)
  NavigationLink(T value, std::string label)
      : NavigationLink(composition::NavigationPath::Entry::of(std::move(value)),
                       std::move(label)) {}

  template <class T>
    requires std::equality_comparable<T> && std::copy_constructible<T> &&
             (!std::is_invocable_v<const T&>)
  NavigationLink(T value, composition::Element label)
      : NavigationLink(composition::NavigationPath::Entry::of(std::move(value)),
                       std::move(label)) {}

  NavigationLink(composition::NavigationPath::Entry entry, std::string label);

  NavigationLink(composition::NavigationPath::Entry entry,
                 composition::Element label);

  NavigationLink(std::string label, Destination destination);

  NavigationLink(Destination destination, composition::Element label);

  auto is_detail_link(bool detail) && -> NavigationLink;

 private:
  struct Target {
    std::optional<NavigationStack::Navigator> navigator;
    bool detail{true};
  };

  using Cell = std::shared_ptr<Target>;

  NavigationLink(composition::NavigationPath::Entry entry,
                 composition::Element label, Cell cell);

  static auto push(composition::NavigationPath::Entry entry, Cell cell)
      -> Action;

  static auto labelled(composition::Element label, Cell cell)
      -> composition::Element;

  Cell cell_;
};

class NavigationLink::Probe : public composition::Element {
 public:
  class Content;

  explicit Probe(Cell cell);
};

class NavigationLink::Probe::Content final
    : public composition::ElementContent {
 public:
  explicit Content(Cell cell) noexcept : cell_(std::move(cell)) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override;

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

 private:
  Cell cell_;
};

}
