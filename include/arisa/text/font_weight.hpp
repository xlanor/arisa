#pragma once

#include <arisa/support/bounded_value.hpp>

#include <cstdint>

namespace arisa::text {

struct FontWeightTag;

using FontWeight = support::BoundedValue<std::uint16_t, 1, 1000, FontWeightTag>;

namespace font_weight {

inline constexpr FontWeight regular = FontWeight::of<400>();
inline constexpr FontWeight medium = FontWeight::of<500>();
inline constexpr FontWeight semibold = FontWeight::of<600>();
inline constexpr FontWeight bold = FontWeight::of<700>();

}

}
