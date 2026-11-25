#pragma once

#include <arisa/components/for_each.hpp>
#include <arisa/components/grid_section.hpp>
#include <arisa/components/lazy_grid.hpp>
#include <arisa/components/stack_options.hpp>
#include <arisa/composition/element.hpp>

#include <concepts>
#include <type_traits>
#include <utility>
#include <vector>

namespace arisa::components {

class LazyVGrid : public composition::Element {
 public:
  template <class... Each>
    requires(sizeof...(Each) > 0 &&
             (std::same_as<std::remove_cvref_t<Each>, ForEach> && ...))
  explicit LazyVGrid(LazyVGridOptions options, Each&&... each)
      : LazyVGrid(std::move(options),
                  LazyGrid::Sources{LazyStack::source_of(each)...}) {}

  template <class... Sections>
    requires(sizeof...(Sections) > 0 &&
             (std::same_as<std::remove_cvref_t<Sections>, GridSection> && ...))
  explicit LazyVGrid(LazyVGridOptions options, Sections&&... sections)
      : LazyVGrid(std::move(options),
                  GridSection::collect(std::forward<Sections>(sections)...)) {}

  template <class... Cells>
    requires(!(sizeof...(Cells) > 0 &&
               (std::same_as<std::remove_cvref_t<Cells>, ForEach> && ...)) &&
             (composition::ViewContent<Cells> && ...))
  explicit LazyVGrid(LazyVGridOptions options, Cells&&... cells)
      : LazyVGrid(std::move(options), collect(std::forward<Cells>(cells)...)) {}

  LazyVGrid(LazyVGridOptions options, LazyGrid::Sources sources);

  LazyVGrid(LazyVGridOptions options, std::vector<GridSection> sections);

  LazyVGrid(LazyVGridOptions options, std::vector<composition::Element> cells);
};

}
