#pragma once

#include <arisa/provider/vulkan/device_object.hpp>
#include <arisa/provider/vulkan/ready_semaphore.hpp>
#include <arisa/provider/vulkan/vulkan_device.hpp>

#include <vulkan/vulkan.h>

#include <cstdint>
#include <expected>
#include <string>

namespace arisa::examples {

class VulkanTimeline {
 public:
  [[nodiscard]] static auto create(const provider::vulkan::VulkanDevice& device)
      -> std::expected<VulkanTimeline, std::string>;

  auto next() noexcept -> provider::vulkan::ReadySemaphore;

 private:
  explicit VulkanTimeline(
      provider::vulkan::VulkanSemaphoreHandle semaphore) noexcept;

  provider::vulkan::VulkanSemaphoreHandle semaphore_;
  std::uint64_t value_{0};
};

}
