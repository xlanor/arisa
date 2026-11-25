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

class LazyHGrid : public composition::Element {
 public:
  template <class... Each>
    requires(sizeof...(Each) > 0 &&
             (std::same_as<std::remove_cvref_t<Each>, ForEach> && ...))
  explicit LazyHGrid(LazyHGridOptions options, Each&&... each)
      : LazyHGrid(std::move(options),
                  LazyGrid::Sources{LazyStack::source_of(each)...}) {}

  template <class... Sections>
    requires(sizeof...(Sections) > 0 &&
             (std::same_as<std::remove_cvref_t<Sections>, GridSection> && ...))
  explicit LazyHGrid(LazyHGridOptions options, Sections&&... sections)
      : LazyHGrid(std::move(options),
                  GridSection::collect(std::forward<Sections>(sections)...)) {}

  template <class... Cells>
    requires(!(sizeof...(Cells) > 0 &&
               (std::same_as<std::remove_cvref_t<Cells>, ForEach> && ...)) &&
             (composition::ViewContent<Cells> && ...))
  explicit LazyHGrid(LazyHGridOptions options, Cells&&... cells)
      : LazyHGrid(std::move(options), collect(std::forward<Cells>(cells)...)) {}

  LazyHGrid(LazyHGridOptions options, LazyGrid::Sources sources);

  LazyHGrid(LazyHGridOptions options, std::vector<GridSection> sections);

  LazyHGrid(LazyHGridOptions options, std::vector<composition::Element> cells);
};

}
