#include <vulkan_timeline.hpp>

#include <utility>

namespace arisa::examples {

VulkanTimeline::VulkanTimeline(
    provider::vulkan::VulkanSemaphoreHandle semaphore) noexcept
    : semaphore_(std::move(semaphore)) {}

auto VulkanTimeline::create(const provider::vulkan::VulkanDevice& device)
    -> std::expected<VulkanTimeline, std::string> {
  if (!device.capabilities().timeline_semaphores) {
    return std::unexpected(
        std::string{"the video needs timeline semaphores on this device"});
  }
  const VkSemaphoreTypeCreateInfo type{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO,
      .pNext = nullptr,
      .semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE,
      .initialValue = 0};
  const VkSemaphoreCreateInfo info{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = &type,
      .flags = 0};
  VkSemaphore made = VK_NULL_HANDLE;
  if (vkCreateSemaphore(device.handle(), &info, nullptr, &made) != VK_SUCCESS) {
    return std::unexpected(std::string{"the video timeline was not made"});
  }
  return VulkanTimeline{provider::vulkan::VulkanSemaphoreHandle{
      {.device = device.handle(), .object = made}}};
}

auto VulkanTimeline::next() noexcept -> provider::vulkan::ReadySemaphore {
  return provider::vulkan::ReadySemaphore{.semaphore = semaphore_.get().object,
                                          .value = ++value_};
}

}
