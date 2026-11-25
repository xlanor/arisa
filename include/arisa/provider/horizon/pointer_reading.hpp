#pragma once

#include <arisa/provider/input/provider.hpp>

#include <cstddef>
#include <optional>

namespace arisa::provider::horizon {

struct PointerReading {
  std::optional<PointerState> pointer;
  bool claims_shoulders{false};
  std::optional<std::size_t> claimed_pad{};
  bool claims_sticks{false};
};

}
