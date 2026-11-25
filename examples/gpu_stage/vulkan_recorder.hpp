#pragma once

#include <arisa/provider/vulkan/device_object.hpp>
#include <arisa/provider/vulkan/ready_semaphore.hpp>
#include <arisa/provider/vulkan/vulkan_device.hpp>

#include <vulkan/vulkan.h>

#include <cstdint>
#include <expected>
#include <functional>
#include <string>

namespace arisa::examples {

class VulkanRecorder {
 public:
  [[nodiscard]] static auto create(const provider::vulkan::VulkanDevice& device)
      -> std::expected<VulkanRecorder, std::string>;

  VulkanRecorder(VulkanRecorder&& other) noexcept;
  auto operator=(VulkanRecorder&& other) noexcept -> VulkanRecorder&;
  VulkanRecorder(const VulkanRecorder&) = delete;
  auto operator=(const VulkanRecorder&) -> VulkanRecorder& = delete;
  ~VulkanRecorder();

  [[nodiscard]] auto begin() -> std::expected<VkCommandBuffer, std::string>;

  [[nodiscard]] auto submit(provider::vulkan::ReadySemaphore signal)
      -> std::expected<void, std::string>;

  static void to_attachment(VkCommandBuffer commands, VkImage image,
                            VkImageLayout from) noexcept;

  static void to_sampled(VkCommandBuffer commands, VkImage image) noexcept;

  static void to_host_sampled(VkCommandBuffer commands, VkImage image) noexcept;

 private:
  VulkanRecorder(const provider::vulkan::VulkanDevice& device,
                 provider::vulkan::VulkanCommandPoolHandle pool,
                 VkCommandBuffer commands,
                 provider::vulkan::VulkanFenceHandle done) noexcept;

  void wait() noexcept;

  static void transition(VkCommandBuffer commands, VkImage image,
                         VkImageLayout from, VkImageLayout to,
                         VkPipelineStageFlags before, VkAccessFlags written,
                         VkPipelineStageFlags after,
                         VkAccessFlags reading) noexcept;

  std::reference_wrapper<const provider::vulkan::VulkanDevice> device_;
  provider::vulkan::VulkanCommandPoolHandle pool_;
  VkCommandBuffer commands_{VK_NULL_HANDLE};
  provider::vulkan::VulkanFenceHandle done_;
  bool pending_{false};
};

}
