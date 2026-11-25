#pragma once

#include <array>
#include <cstdint>

namespace arisa::async {

enum class JobPriority : std::uint8_t {
  low,
  normal,
  high,
};

inline constexpr std::array all_job_priorities{
    JobPriority::high, JobPriority::normal, JobPriority::low};

}
