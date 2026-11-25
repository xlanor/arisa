#include <arisa/components/lazy_vstack.hpp>
#include <arisa/components/vstack.hpp>

#include <arisa/composition/length.hpp>

#include <memory>
#include <utility>

namespace arisa::components {

LazyVStack::LazyVStack(LazyVStackOptions options, LazyStack::Sources sources)
    : Element(std::make_unique<LazyStack::Content>(
          LazyStack::arrangement(options), std::move(sources))) {}

LazyVStack::LazyVStack(LazyVStackOptions options, LazyStack::Parts parts)
    : Element(std::make_unique<VStack::Content>(VStackOptions{
          .alignment = options.alignment, .spacing = options.spacing})) {
  edit_modifiers().frame.max_width = composition::infinity;
  replace_children(LazyStack::composed(
      parts, options.pinned_views, composition::Axis::vertical, options.spacing,
      [&options](LazyStack::Sources sources) {
        return composition::Element{LazyVStack(options, std::move(sources))};
      }));
}

LazyVStack::LazyVStack(LazyVStackOptions options,
                       std::vector<GridSection> sections)
    : Element(std::make_unique<VStack::Content>(
          VStackOptions{.spacing = options.spacing})) {
  edit_modifiers().frame.max_width = composition::infinity;
  replace_children(LazyStack::sections(
      sections, options.pinned_views, composition::Axis::vertical,
      options.spacing, [&options](LazyStack::Sources sources) {
        return composition::Element{LazyVStack(options, std::move(sources))};
      }));
}

}
