#pragma once

#include <arisa/support/bounded_value.hpp>

#include <cstdint>

namespace arisa::text {

struct CacheCapacityTag;

using CacheCapacity =
    support::BoundedValue<std::uint32_t, 1, 65536, CacheCapacityTag>;

struct TypesetterLimits {
  CacheCapacity layouts{CacheCapacity::of<512>()};
  CacheCapacity shaped{CacheCapacity::of<256>()};
};

}
