#pragma once

#include <arisa/support/strong_value.hpp>

#include <cstdint>

namespace arisa::text {

struct GlyphIdTag;

using GlyphId = support::StrongValue<std::uint32_t, GlyphIdTag>;

}
