#pragma once

#include <arisa/support/strong_value.hpp>

#include <cstdint>

namespace arisa::text {

struct FaceIdTag;

using FaceId = support::StrongValue<std::uint64_t, FaceIdTag>;

}
