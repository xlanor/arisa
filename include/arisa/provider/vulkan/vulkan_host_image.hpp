#pragma once

#include <arisa/provider/error.hpp>
#include <arisa/provider/vulkan/device_object.hpp>
#include <arisa/provider/vulkan/memory_allocator.hpp>
#include <arisa/provider/vulkan/vulkan_device.hpp>

#include <vulkan/vulkan.h>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>

namespace arisa::provider::vulkan {

class VulkanHostImage {
 public:
  static constexpr VkImageLayout initial_layout =
      VK_IMAGE_LAYOUT_PREINITIALIZED;

  struct Description {
    std::uint32_t width{0};
    std::uint32_t height{0};
    VkFormat format{VK_FORMAT_R8_UNORM};
  };

  [[nodiscard]] static auto create(const VulkanDevice& device,
                                   Description description)
      -> std::expected<VulkanHostImage, ProviderError>;

  auto image() const noexcept -> VkImage { return image_.get().object; }

  auto description() const noexcept -> const Description& {
    return description_;
  }

  auto bytes() const noexcept -> std::span<std::byte> { return bytes_; }

  auto row_pitch() const noexcept -> std::size_t { return row_pitch_; }

  void flush() const noexcept;

 private:
  VulkanHostImage(Description description,
                  MemoryAllocator::Allocation allocation,
                  VulkanImageHandle image, VkDeviceSize offset,
                  std::span<std::byte> bytes, std::size_t row_pitch) noexcept;

  [[nodiscard]] static auto samples_linear(const VulkanDevice& device,
                                           VkFormat format) noexcept -> bool;

  Description description_;
  MemoryAllocator::Allocation allocation_;
  VulkanImageHandle image_;
  VkDeviceSize offset_{0};
  std::span<std::byte> bytes_;
  std::size_t row_pitch_{0};
};

}
