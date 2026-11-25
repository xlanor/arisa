#pragma once

#include <arisa/provider/gpu/swap_interval.hpp>
#include <arisa/provider/surface/surface_size.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace arisa::provider::horizon {

enum class SwapBuffering : std::uint8_t {
  double_buffering,
  triple_buffering,
};

enum class GpuApi : std::uint8_t {
  deko3d,
  drako3d,
  vulkan,
};

enum class VulkanFeature : std::uint8_t {
  dynamic_rendering,
  synchronization2,
  maintenance4,
};

using SwapInterval = provider::SwapInterval;

struct VulkanDeviceOptions {
  std::vector<std::string> extensions{};
  std::vector<std::string> optional_extensions{};
  std::vector<VulkanFeature> features{};

  friend auto operator==(const VulkanDeviceOptions&, const VulkanDeviceOptions&)
      -> bool = default;
};

struct GpuOptions {
  SwapBuffering buffering{SwapBuffering::double_buffering};
  SwapInterval swap_interval{SwapInterval::of<1>()};
  std::optional<SurfaceSize> docked_cap{};
  std::optional<GpuApi> api{};
  VulkanDeviceOptions vulkan{};

  constexpr auto image_count() const noexcept -> std::uint32_t {
    return buffering == SwapBuffering::triple_buffering ? 3 : 2;
  }

  friend auto operator==(const GpuOptions&, const GpuOptions&)
      -> bool = default;
};

}
