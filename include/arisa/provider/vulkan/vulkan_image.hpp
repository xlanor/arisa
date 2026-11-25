#pragma once

#include <arisa/provider/error.hpp>
#include <arisa/provider/vulkan/device_object.hpp>
#include <arisa/provider/vulkan/memory_allocator.hpp>
#include <arisa/provider/vulkan/vulkan_device.hpp>

#include <vulkan/vulkan.h>

#include <cstdint>
#include <expected>

namespace arisa::provider::vulkan {

class VulkanImage {
 public:
  struct Description {
    std::uint32_t width{0};
    std::uint32_t height{0};
    VkFormat format{VK_FORMAT_R8G8B8A8_UNORM};
    VkImageUsageFlags usage{VK_IMAGE_USAGE_SAMPLED_BIT};
    VkComponentMapping swizzle{};
  };

  [[nodiscard]] static auto create(const VulkanDevice& device,
                                   Description description)
      -> std::expected<VulkanImage, ProviderError>;

  auto image() const noexcept -> VkImage { return image_.get().object; }

  auto view() const noexcept -> VkImageView { return view_.get().object; }

  auto description() const noexcept -> const Description& {
    return description_;
  }

  auto allocation() const noexcept -> const MemoryAllocator::Allocation& {
    return allocation_;
  }

 private:
  VulkanImage(Description description, MemoryAllocator::Allocation allocation,
              VulkanImageHandle image, VulkanImageViewHandle view) noexcept;

  Description description_;
  MemoryAllocator::Allocation allocation_;
  VulkanImageHandle image_;
  VulkanImageViewHandle view_;
};

}
