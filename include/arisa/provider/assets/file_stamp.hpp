#pragma once

#include <arisa/support/strong_value.hpp>

#include <cstdint>

namespace arisa::provider {

struct FileStampTag;

using FileStamp = support::StrongValue<std::uint64_t, FileStampTag>;

}
