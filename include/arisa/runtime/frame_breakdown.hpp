#pragma once

#include <arisa/runtime/frame_timings.hpp>

#include <chrono>
#include <cstdint>
#include <optional>
#include <string>

namespace arisa::runtime {

class FrameBreakdown {
 public:
  static auto describe(const FrameTimings& frame,
                       std::optional<std::uint64_t> allocations) -> std::string;

 private:
  static auto milliseconds(std::chrono::nanoseconds value) noexcept -> double;
};

}
