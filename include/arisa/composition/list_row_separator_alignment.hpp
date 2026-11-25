#pragma once

#include <arisa/composition/alignment_id.hpp>
#include <arisa/composition/axis.hpp>
#include <arisa/composition/view_dimensions.hpp>

namespace arisa::composition {

inline constexpr AlignmentID list_row_separator_leading{
    "listRowSeparatorLeading", Axis::horizontal,
    [](const ViewDimensions&) { return 0.0F; }};

inline constexpr AlignmentID list_row_separator_trailing{
    "listRowSeparatorTrailing", Axis::horizontal,
    [](const ViewDimensions& dimensions) { return dimensions.width(); }};

}
