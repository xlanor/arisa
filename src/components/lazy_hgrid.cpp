#include <arisa/components/hstack.hpp>
#include <arisa/components/lazy_hgrid.hpp>

#include <cstdint>
#include <format>
#include <memory>
#include <utility>

namespace arisa::components {

LazyHGrid::LazyHGrid(LazyHGridOptions options, LazyGrid::Sources sources)
    : Element(std::make_unique<LazyGrid::Content>(
          LazyGrid::arrangement(options), std::move(sources))) {
  declared().focus_section = true;
}

LazyHGrid::LazyHGrid(LazyHGridOptions options,
                     std::vector<composition::Element> cells)
    : Element(std::make_unique<LazyGrid::Flow>(LazyGrid::arrangement(options),
                                               0.0F, 0.0F),
              std::move(cells)) {
  declared().focus_section = true;
}

LazyHGrid::LazyHGrid(LazyHGridOptions options,
                     std::vector<GridSection> sections)
    : Element(std::make_unique<HStack::Content>(
          HStackOptions{.spacing = options.spacing.value_or(
                            theme::Space{theme::metric::stack_spacing})})) {
  const theme::Space spacing =
      options.spacing.value_or(theme::Space{theme::metric::stack_spacing});
  replace_children(LazyStack::sections(
      sections, options.pinned_views, composition::Axis::horizontal, spacing,
      [&options](LazyGrid::Sources sources) {
        return composition::Element{LazyHGrid(options, std::move(sources))};
      }));
}

}
