#include <arisa/provider/vulkan/vulkan_image.hpp>
#include <arisa/provider/vulkan/vulkan_status.hpp>

#include <utility>

namespace arisa::provider::vulkan {

VulkanImage::VulkanImage(Description description,
                         MemoryAllocator::Allocation allocation,
                         VulkanImageHandle image,
                         VulkanImageViewHandle view) noexcept
    : description_(description),
      allocation_(std::move(allocation)),
      image_(std::move(image)),
      view_(std::move(view)) {}

auto VulkanImage::create(const VulkanDevice& device, Description description)
    -> std::expected<VulkanImage, ProviderError> {
  if (description.width == 0 || description.height == 0) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument});
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
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = description.usage,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0,
      .pQueueFamilyIndices = nullptr,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};
  VkImage raw = VK_NULL_HANDLE;
  if (const VkResult result = vkCreateImage(handle, &info, nullptr, &raw);
      result != VK_SUCCESS) {
    return std::unexpected(VulkanStatus::error(result));
  }
  VulkanImageHandle image{{.device = handle, .object = raw}};
  VkMemoryRequirements requirements{};
  vkGetImageMemoryRequirements(handle, raw, &requirements);
  auto allocation = device.allocate(requirements, MemoryUse::device_local,
                                    MemoryTiling::optimal);
  if (!allocation.has_value()) {
    return std::unexpected(allocation.error());
  }
  if (const VkResult result = vkBindImageMemory(
          handle, raw, allocation->memory(), allocation->offset());
      result != VK_SUCCESS) {
    return std::unexpected(VulkanStatus::error(result));
  }
  const VkImageViewCreateInfo view_info{
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .image = raw,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = description.format,
      .components = description.swizzle,
      .subresourceRange =
          VkImageSubresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                  .baseMipLevel = 0,
                                  .levelCount = 1,
                                  .baseArrayLayer = 0,
                                  .layerCount = 1}};
  VkImageView view_raw = VK_NULL_HANDLE;
  if (const VkResult result =
          vkCreateImageView(handle, &view_info, nullptr, &view_raw);
      result != VK_SUCCESS) {
    return std::unexpected(VulkanStatus::error(result));
  }
  return VulkanImage{
      description, std::move(*allocation), std::move(image),
      VulkanImageViewHandle{{.device = handle, .object = view_raw}}};
}

}
