#pragma once

#include <cstdint>

namespace arisa::text {

enum class PluralCategory : std::uint8_t {
  zero,
  one,
  two,
  few,
  many,
  other,
};

}
