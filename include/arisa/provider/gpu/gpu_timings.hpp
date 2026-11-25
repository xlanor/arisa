#pragma once

#include <chrono>
#include <cstdint>
#include <optional>

namespace arisa::provider {

enum class EncodeStage : std::uint8_t {
  none,
  pass_target,
  sampled_texture,
};

struct EncodeMiss {
  std::uint64_t count{0};
  std::uint64_t texture{0};
  EncodeStage stage{EncodeStage::none};

  friend constexpr auto operator==(const EncodeMiss&, const EncodeMiss&)
      -> bool = default;
};

struct GpuTimings {
  std::optional<std::chrono::nanoseconds> effects{};
  std::optional<std::chrono::nanoseconds> interface{};
  std::uint32_t passes{0};
  bool measured_on_gpu{false};
  std::optional<std::chrono::nanoseconds> hooks{};
  std::uint64_t missed_vsyncs{0};
  EncodeMiss encode_misses{};

  friend constexpr auto operator==(const GpuTimings&, const GpuTimings&)
      -> bool = default;
};

}
