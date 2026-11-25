#include <arisa/provider/vulkan/vulkan_buffer.hpp>
#include <arisa/provider/vulkan/vulkan_status.hpp>

#include <cstddef>
#include <utility>

namespace arisa::provider::vulkan {

VulkanBuffer::VulkanBuffer(VulkanBufferHandle buffer,
                           MemoryAllocator::Allocation allocation,
                           VkDeviceSize size) noexcept
    : allocation_(std::move(allocation)),
      buffer_(std::move(buffer)),
      size_(size),
      mapped_(allocation_.mapped()) {
  const std::size_t length = size;
  if (mapped_.size() > length) {
    mapped_ = mapped_.first(length);
  }
}

auto VulkanBuffer::create(const VulkanDevice& device, VkDeviceSize size,
                          VkBufferUsageFlags usage, MemoryUse use)
    -> std::expected<VulkanBuffer, ProviderError> {
  if (size == 0) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument});
  }
  VkDevice const handle = device.handle();
  const VkBufferCreateInfo info{.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                                .pNext = nullptr,
                                .flags = 0,
                                .size = size,
                                .usage = usage,
                                .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                .queueFamilyIndexCount = 0,
                                .pQueueFamilyIndices = nullptr};
  VkBuffer raw = VK_NULL_HANDLE;
  if (const VkResult result = vkCreateBuffer(handle, &info, nullptr, &raw);
      result != VK_SUCCESS) {
    return std::unexpected(VulkanStatus::error(result));
  }
  VulkanBufferHandle buffer{{.device = handle, .object = raw}};
  VkMemoryRequirements requirements{};
  vkGetBufferMemoryRequirements(handle, raw, &requirements);
  auto allocation = device.allocate(requirements, use, MemoryTiling::linear);
  if (!allocation.has_value()) {
    return std::unexpected(allocation.error());
  }
  if (const VkResult result = vkBindBufferMemory(
          handle, raw, allocation->memory(), allocation->offset());
      result != VK_SUCCESS) {
    return std::unexpected(VulkanStatus::error(result));
  }
  if (use != MemoryUse::device_local && allocation->mapped().empty()) {
    return std::unexpected(ProviderError::unsupported_capability());
  }
  return VulkanBuffer{std::move(buffer), std::move(*allocation), size};
}

void VulkanBuffer::flush(VkDeviceSize offset,
                         VkDeviceSize length) const noexcept {
  allocation_.flush(offset, length);
}

void VulkanBuffer::invalidate() const noexcept { allocation_.invalidate(); }

}
