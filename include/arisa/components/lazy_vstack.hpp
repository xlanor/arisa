#pragma once

#include <arisa/components/for_each.hpp>
#include <arisa/components/grid_section.hpp>
#include <arisa/components/lazy_stack.hpp>
#include <arisa/components/stack_options.hpp>
#include <arisa/composition/element.hpp>

#include <concepts>
#include <type_traits>
#include <utility>
#include <vector>

namespace arisa::components {

class LazyVStack : public composition::Element {
 public:
  template <class... Each>
    requires(std::same_as<std::remove_cvref_t<Each>, ForEach> && ...)
  explicit LazyVStack(Each&&... each)
      : LazyVStack(LazyVStackOptions{},
                   LazyStack::Sources{LazyStack::source_of(each)...}) {}

  template <class... Each>
    requires(std::same_as<std::remove_cvref_t<Each>, ForEach> && ...)
  explicit LazyVStack(LazyVStackOptions options, Each&&... each)
      : LazyVStack(options, LazyStack::Sources{LazyStack::source_of(each)...}) {
  }

  template <class... Sections>
    requires(sizeof...(Sections) > 0 &&
             (std::same_as<std::remove_cvref_t<Sections>, GridSection> && ...))
  explicit LazyVStack(LazyVStackOptions options, Sections&&... sections)
      : LazyVStack(options,
                   GridSection::collect(std::forward<Sections>(sections)...)) {}

  template <class... Children>
    requires MixedLazyChildren<Children...>
  explicit LazyVStack(Children&&... children)
      : LazyVStack(LazyVStackOptions{},
                   LazyStack::parts_of(std::forward<Children>(children)...)) {}

  template <class... Children>
    requires MixedLazyChildren<Children...>
  explicit LazyVStack(LazyVStackOptions options, Children&&... children)
      : LazyVStack(options,
                   LazyStack::parts_of(std::forward<Children>(children)...)) {}

  LazyVStack(LazyVStackOptions options, LazyStack::Sources sources);

  LazyVStack(LazyVStackOptions options, LazyStack::Parts parts);

  LazyVStack(LazyVStackOptions options, std::vector<GridSection> sections);
};

}
