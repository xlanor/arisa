#include <arisa/components/lazy_vgrid.hpp>
#include <arisa/components/vstack.hpp>

#include <cstdint>
#include <format>
#include <memory>
#include <utility>

namespace arisa::components {

LazyVGrid::LazyVGrid(LazyVGridOptions options, LazyGrid::Sources sources)
    : Element(std::make_unique<LazyGrid::Content>(
          LazyGrid::arrangement(options), std::move(sources))) {
  declared().focus_section = true;
}

LazyVGrid::LazyVGrid(LazyVGridOptions options,
                     std::vector<composition::Element> cells)
    : Element(std::make_unique<LazyGrid::Flow>(LazyGrid::arrangement(options),
                                               0.0F, 0.0F),
              std::move(cells)) {
  declared().focus_section = true;
}

LazyVGrid::LazyVGrid(LazyVGridOptions options,
                     std::vector<GridSection> sections)
    : Element(std::make_unique<VStack::Content>(
          VStackOptions{.spacing = options.spacing.value_or(
                            theme::Space{theme::metric::stack_spacing})})) {
  const theme::Space spacing =
      options.spacing.value_or(theme::Space{theme::metric::stack_spacing});
  replace_children(LazyStack::sections(
      sections, options.pinned_views, composition::Axis::vertical, spacing,
      [&options](LazyGrid::Sources sources) {
        return composition::Element{LazyVGrid(options, std::move(sources))};
      }));
}

}
