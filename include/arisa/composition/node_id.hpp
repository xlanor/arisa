#pragma once

#include <arisa/support/strong_value.hpp>

#include <cstdint>

namespace arisa::composition {

struct NodeIdTag;

using NodeId = support::StrongValue<std::uint64_t, NodeIdTag>;

}
