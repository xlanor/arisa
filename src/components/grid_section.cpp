#include <arisa/components/grid_section.hpp>
#include <arisa/components/lazy_stack.hpp>

#include <utility>

namespace arisa::components {

GridSection::GridSection(const ForEach& each)
    : source_(LazyStack::source_of(each)) {}

GridSection::GridSection(composition::Element header, const ForEach& each)
    : header_(std::move(header)), source_(LazyStack::source_of(each)) {}

GridSection::GridSection(composition::Element header, const ForEach& each,
                         composition::Element footer)
    : header_(std::move(header)),
      source_(LazyStack::source_of(each)),
      footer_(std::move(footer)) {}

}
