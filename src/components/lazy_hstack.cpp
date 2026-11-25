#include <arisa/components/hstack.hpp>
#include <arisa/components/lazy_hstack.hpp>

#include <arisa/composition/length.hpp>

#include <memory>
#include <utility>

namespace arisa::components {

LazyHStack::LazyHStack(LazyHStackOptions options, LazyStack::Sources sources)
    : Element(std::make_unique<LazyStack::Content>(
          LazyStack::arrangement(options), std::move(sources))) {}

LazyHStack::LazyHStack(LazyHStackOptions options, LazyStack::Parts parts)
    : Element(std::make_unique<HStack::Content>(HStackOptions{
          .alignment = options.alignment, .spacing = options.spacing})) {
  edit_modifiers().frame.max_height = composition::infinity;
  replace_children(LazyStack::composed(
      parts, options.pinned_views, composition::Axis::horizontal,
      options.spacing, [&options](LazyStack::Sources sources) {
        return composition::Element{LazyHStack(options, std::move(sources))};
      }));
}

LazyHStack::LazyHStack(LazyHStackOptions options,
                       std::vector<GridSection> sections)
    : Element(std::make_unique<HStack::Content>(
          HStackOptions{.spacing = options.spacing})) {
  edit_modifiers().frame.max_height = composition::infinity;
  replace_children(LazyStack::sections(
      sections, options.pinned_views, composition::Axis::horizontal,
      options.spacing, [&options](LazyStack::Sources sources) {
        return composition::Element{LazyHStack(options, std::move(sources))};
      }));
}

}
