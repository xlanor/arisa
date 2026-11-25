#include <arisa/provider/vulkan/vulkan_host_image.hpp>
#include <arisa/provider/vulkan/vulkan_status.hpp>

#include <utility>

namespace arisa::provider::vulkan {

VulkanHostImage::VulkanHostImage(Description description,
                                 MemoryAllocator::Allocation allocation,
                                 VulkanImageHandle image, VkDeviceSize offset,
                                 std::span<std::byte> bytes,
                                 std::size_t row_pitch) noexcept
    : description_(description),
      allocation_(std::move(allocation)),
      image_(std::move(image)),
      offset_(offset),
      bytes_(bytes),
      row_pitch_(row_pitch) {}

auto VulkanHostImage::samples_linear(const VulkanDevice& device,
                                     VkFormat format) noexcept -> bool {
  VkFormatProperties properties{};
  vkGetPhysicalDeviceFormatProperties(device.physical(), format, &properties);
  return (properties.linearTilingFeatures &
          VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0;
}

auto VulkanHostImage::create(const VulkanDevice& device,
                             Description description)
    -> std::expected<VulkanHostImage, ProviderError> {
  if (description.width == 0 || description.height == 0) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument});
  }
  if (!samples_linear(device, description.format)) {
    return std::unexpected(ProviderError::unsupported_capability());
  }
  VkDevice const handle = device.handle();
  const VkImageCreateInfo info{
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = description.format,
      .extent = VkExtent3D{.width = description.width,
                           .height = description.height,
                           .depth = 1},
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .tiling = VK_IMAGE_TILING_LINEAR,
      .usage = VK_IMAGE_USAGE_SAMPLED_BIT,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0,
      .pQueueFamilyIndices = nullptr,
      .initialLayout = initial_layout};
  VkImage raw = VK_NULL_HANDLE;
  if (const VkResult result = vkCreateImage(handle, &info, nullptr, &raw);
      result != VK_SUCCESS) {
    return std::unexpected(VulkanStatus::error(result));
  }
  VulkanImageHandle image{{.device = handle, .object = raw}};
  VkMemoryRequirements requirements{};
  vkGetImageMemoryRequirements(handle, raw, &requirements);
  auto allocation =
      device.allocate(requirements, MemoryUse::upload, MemoryTiling::linear);
  if (!allocation.has_value()) {
    return std::unexpected(allocation.error());
  }
  if (allocation->mapped().empty()) {
    return std::unexpected(ProviderError::unsupported_capability());
  }
  if (const VkResult result = vkBindImageMemory(
          handle, raw, allocation->memory(), allocation->offset());
      result != VK_SUCCESS) {
    return std::unexpected(VulkanStatus::error(result));
  }
  const VkImageSubresource subresource{
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT, .mipLevel = 0, .arrayLayer = 0};
  VkSubresourceLayout layout{};
  vkGetImageSubresourceLayout(handle, raw, &subresource, &layout);
  const std::size_t start = layout.offset;
  const std::size_t length = layout.size;
  const std::size_t pitch = layout.rowPitch;
  const std::span<std::byte> mapped = allocation->mapped();
  if (start > mapped.size() || length > mapped.size() - start) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument});
  }
  return VulkanHostImage{
      description,   std::move(*allocation),        std::move(image),
      layout.offset, mapped.subspan(start, length), pitch};
}

void VulkanHostImage::flush() const noexcept {
  allocation_.flush(offset_, bytes_.size());
}

}
