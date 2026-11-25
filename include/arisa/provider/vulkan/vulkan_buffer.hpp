#pragma once

#include <arisa/provider/error.hpp>
#include <arisa/provider/vulkan/device_object.hpp>
#include <arisa/provider/vulkan/memory_allocator.hpp>
#include <arisa/provider/vulkan/vulkan_device.hpp>

#include <vulkan/vulkan.h>

#include <cstddef>
#include <expected>
#include <span>

namespace arisa::provider::vulkan {

class VulkanBuffer {
 public:
  [[nodiscard]] static auto create(const VulkanDevice& device,
                                   VkDeviceSize size, VkBufferUsageFlags usage,
                                   MemoryUse use)
      -> std::expected<VulkanBuffer, ProviderError>;

  auto handle() const noexcept -> VkBuffer { return buffer_.get().object; }

  auto size() const noexcept -> VkDeviceSize { return size_; }

  auto bytes() const noexcept -> std::span<std::byte> { return mapped_; }

  auto allocation() const noexcept -> const MemoryAllocator::Allocation& {
    return allocation_;
  }

  void flush(VkDeviceSize offset, VkDeviceSize length) const noexcept;

  void invalidate() const noexcept;

 private:
  VulkanBuffer(VulkanBufferHandle buffer,
               MemoryAllocator::Allocation allocation,
               VkDeviceSize size) noexcept;

  MemoryAllocator::Allocation allocation_;
  VulkanBufferHandle buffer_;
  VkDeviceSize size_;
  std::span<std::byte> mapped_;
};

}
