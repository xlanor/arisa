#pragma once

#include <arisa/provider/error.hpp>
#include <arisa/provider/gpu/gpu_timings.hpp>
#include <arisa/provider/vulkan/device_object.hpp>
#include <arisa/provider/vulkan/vulkan_device.hpp>

#include <vulkan/vulkan.h>

#include <cstdint>
#include <expected>
#include <optional>

namespace arisa::provider::vulkan {

class TimestampQueries {
 public:
  enum class Mark : std::uint32_t {
    started,
    effects_drawn,
    under_started,
    under_drawn,
    interface_drawn,
    over_drawn,
  };

  static constexpr std::uint32_t count = 6;

  struct Armed {
    std::uint32_t passes{0};
    bool hooked{false};
  };

  struct Reading {
    std::optional<GpuTimings> timings{};
    bool stalled{false};
  };

  [[nodiscard]] static auto create(const VulkanDevice& device)
      -> std::expected<TimestampQueries, ProviderError>;

  void reset(VkCommandBuffer commands) const noexcept;

  void mark(VkCommandBuffer commands, Mark mark) const noexcept;

  void arm(Armed armed) noexcept { armed_ = armed; }

  void disarm() noexcept { armed_.reset(); }

  [[nodiscard]] auto read() -> Reading;

 private:
  TimestampQueries(VulkanQueryPoolHandle pool, double period,
                   std::uint64_t mask) noexcept;

  auto nanoseconds(std::uint64_t from, std::uint64_t to) const noexcept
      -> std::chrono::nanoseconds;

  VulkanQueryPoolHandle pool_;
  double period_;
  std::uint64_t mask_;
  std::optional<Armed> armed_;
};

}
