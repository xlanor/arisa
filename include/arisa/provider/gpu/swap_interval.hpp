#pragma once

#include <arisa/support/bounded_value.hpp>

#include <cstdint>

namespace arisa::provider {

struct SwapIntervalTag;

using SwapInterval =
    support::BoundedValue<std::uint32_t, 0, 4, SwapIntervalTag>;

}
