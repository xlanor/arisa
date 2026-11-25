#pragma once

#include <arisa/provider/gpu/draw_data.hpp>
#include <arisa/provider/surface/surface_size.hpp>
#include <arisa/support/strong_value.hpp>

#include <chrono>
#include <cstdint>
#include <optional>

namespace arisa::provider {

struct GpuSerialTag;

using GpuSerial = support::StrongValue<std::uint64_t, GpuSerialTag>;

struct FramePlan {
  std::chrono::nanoseconds predicted_present{0};
  bool replay{false};
  bool exclusive{false};
  SurfaceSize extent{};
  std::uint32_t swap_interval{1};
  bool capture{false};

  friend constexpr auto operator==(const FramePlan&, const FramePlan&)
      -> bool = default;
};

struct FrameInfo {
  GpuSerial serial{0};
  std::chrono::nanoseconds predicted_present{0};
  SurfaceSize extent{};
  bool replay{false};
  std::optional<ClipRect> damage{};

  friend constexpr auto operator==(const FrameInfo&, const FrameInfo&)
      -> bool = default;
};

}
