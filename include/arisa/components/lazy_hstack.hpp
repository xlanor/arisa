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

class LazyHStack : public composition::Element {
 public:
  template <class... Each>
    requires(std::same_as<std::remove_cvref_t<Each>, ForEach> && ...)
  explicit LazyHStack(Each&&... each)
      : LazyHStack(LazyHStackOptions{},
                   LazyStack::Sources{LazyStack::source_of(each)...}) {}

  template <class... Each>
    requires(std::same_as<std::remove_cvref_t<Each>, ForEach> && ...)
  explicit LazyHStack(LazyHStackOptions options, Each&&... each)
      : LazyHStack(options, LazyStack::Sources{LazyStack::source_of(each)...}) {
  }

  template <class... Sections>
    requires(sizeof...(Sections) > 0 &&
             (std::same_as<std::remove_cvref_t<Sections>, GridSection> && ...))
  explicit LazyHStack(LazyHStackOptions options, Sections&&... sections)
      : LazyHStack(options,
                   GridSection::collect(std::forward<Sections>(sections)...)) {}

  template <class... Children>
    requires MixedLazyChildren<Children...>
  explicit LazyHStack(Children&&... children)
      : LazyHStack(LazyHStackOptions{},
                   LazyStack::parts_of(std::forward<Children>(children)...)) {}

  template <class... Children>
    requires MixedLazyChildren<Children...>
  explicit LazyHStack(LazyHStackOptions options, Children&&... children)
      : LazyHStack(options,
                   LazyStack::parts_of(std::forward<Children>(children)...)) {}

  LazyHStack(LazyHStackOptions options, LazyStack::Sources sources);

  LazyHStack(LazyHStackOptions options, LazyStack::Parts parts);

  LazyHStack(LazyHStackOptions options, std::vector<GridSection> sections);
};

}
