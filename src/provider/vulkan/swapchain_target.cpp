#include <arisa/provider/vulkan/present_mode.hpp>
#include <arisa/provider/vulkan/swapchain_target.hpp>
#include <arisa/provider/vulkan/vulkan_status.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace arisa::provider::vulkan {

SwapchainTarget::SwapchainTarget(std::shared_ptr<const VulkanDevice> device,
                                 VulkanSurface surface,
                                 const SurfaceProvider& window,
                                 Options options) noexcept
    : device_(std::move(device)),
      surface_(std::move(surface)),
      window_(window),
      options_(std::move(options)) {}

auto SwapchainTarget::create(std::shared_ptr<const VulkanDevice> device,
                             VulkanSurface surface,
                             const SurfaceProvider& window, Options options)
    -> std::expected<SwapchainTarget, ProviderError> {
  if (device == nullptr || !device->capabilities().presents ||
      surface.handle() == VK_NULL_HANDLE) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument});
  }
  SwapchainTarget target{std::move(device), std::move(surface), window,
                         std::move(options)};
  if (auto prepared = target.prepare({}); !prepared.has_value()) {
    return std::unexpected(prepared.error());
  }
  return target;
}

auto SwapchainTarget::prepare(SurfaceSize wanted)
    -> std::expected<bool, ProviderError> {
  const SurfaceSize size = wanted.empty() ? window_.get().size() : wanted;
  if (size.empty()) {
    return false;
  }
  if (swapchain_.valid() && !stale_ && size == requested_) {
    return true;
  }
  if (auto rebuilt = rebuild(size); !rebuilt.has_value()) {
    return std::unexpected(rebuilt.error());
  }
  return true;
}

auto SwapchainTarget::chosen_format() const
    -> std::optional<VkSurfaceFormatKHR> {
  VkPhysicalDevice const physical = device_->physical();
  std::uint32_t count = 0;
  if (vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface_.handle(), &count,
                                           nullptr) != VK_SUCCESS ||
      count == 0) {
    return std::nullopt;
  }
  std::vector<VkSurfaceFormatKHR> formats(count);
  if (vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface_.handle(), &count,
                                           formats.data()) != VK_SUCCESS) {
    return std::nullopt;
  }
  if (formats.size() == 1 && formats.front().format == VK_FORMAT_UNDEFINED) {
    return VkSurfaceFormatKHR{.format = VK_FORMAT_R8G8B8A8_UNORM,
                              .colorSpace = formats.front().colorSpace};
  }
  for (const VkFormat preferred :
       {VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_B8G8R8A8_UNORM}) {
    const auto found = std::ranges::find_if(
        formats, [preferred](const VkSurfaceFormatKHR& format) {
          return format.format == preferred;
        });
    if (found != formats.end()) {
      return *found;
    }
  }
  return formats.front();
}

auto SwapchainTarget::chosen_mode() const -> VkPresentModeKHR {
  if (options_.interval != 0) {
    return PresentMode::choose(options_.interval, {});
  }
  VkPhysicalDevice const physical = device_->physical();
  std::uint32_t count = 0;
  if (vkGetPhysicalDeviceSurfacePresentModesKHR(
          physical, surface_.handle(), &count, nullptr) != VK_SUCCESS) {
    return VK_PRESENT_MODE_FIFO_KHR;
  }
  std::vector<VkPresentModeKHR> modes(count);
  if (vkGetPhysicalDeviceSurfacePresentModesKHR(
          physical, surface_.handle(), &count, modes.data()) != VK_SUCCESS) {
    return VK_PRESENT_MODE_FIFO_KHR;
  }
  return PresentMode::choose(options_.interval, modes);
}

auto SwapchainTarget::rebuild(SurfaceSize wanted)
    -> std::expected<void, ProviderError> {
  device_->wait_idle();
  VkPhysicalDevice const physical = device_->physical();
  VkDevice const handle = device_->handle();
  VkSurfaceCapabilitiesKHR capabilities{};
  if (const VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
          physical, surface_.handle(), &capabilities);
      result != VK_SUCCESS) {
    return std::unexpected(VulkanStatus::error(result));
  }
  const std::optional<VkSurfaceFormatKHR> format = chosen_format();
  if (!format.has_value()) {
    return std::unexpected(ProviderError::unsupported_capability());
  }
  const VkExtent2D extent{
      .width =
          std::clamp(wanted.width.value(), capabilities.minImageExtent.width,
                     capabilities.maxImageExtent.width),
      .height =
          std::clamp(wanted.height.value(), capabilities.minImageExtent.height,
                     capabilities.maxImageExtent.height)};
  const std::uint32_t most = capabilities.maxImageCount == 0
                                 ? std::numeric_limits<std::uint32_t>::max()
                                 : capabilities.maxImageCount;
  const std::uint32_t count =
      std::clamp(options_.images, capabilities.minImageCount, most);
  VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  if ((capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) !=
      0) {
    usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  }
  VkCompositeAlphaFlagBitsKHR composite = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  for (const VkCompositeAlphaFlagBitsKHR candidate :
       {VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR}) {
    if ((capabilities.supportedCompositeAlpha & candidate) != 0) {
      composite = candidate;
      break;
    }
  }
  const VkSurfaceTransformFlagBitsKHR transform =
      (capabilities.supportedTransforms &
       VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) != 0
          ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR
          : capabilities.currentTransform;
  const VkSwapchainCreateInfoKHR info{
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .pNext = nullptr,
      .flags = 0,
      .surface = surface_.handle(),
      .minImageCount = count,
      .imageFormat = format->format,
      .imageColorSpace = format->colorSpace,
      .imageExtent = extent,
      .imageArrayLayers = 1,
      .imageUsage = usage,
      .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0,
      .pQueueFamilyIndices = nullptr,
      .preTransform = transform,
      .compositeAlpha = composite,
      .presentMode = chosen_mode(),
      .clipped = VK_TRUE,
      .oldSwapchain = swapchain_.get().object};
  VkSwapchainKHR raw = VK_NULL_HANDLE;
  if (const VkResult result =
          vkCreateSwapchainKHR(handle, &info, nullptr, &raw);
      result != VK_SUCCESS) {
    return std::unexpected(VulkanStatus::error(result));
  }
  usage_ = usage;
  finished_.clear();
  raw_views_.clear();
  views_.clear();
  images_.clear();
  swapchain_ = VulkanSwapchainHandle{{.device = handle, .object = raw}};
  std::uint32_t image_count = 0;
  if (const VkResult result =
          vkGetSwapchainImagesKHR(handle, raw, &image_count, nullptr);
      result != VK_SUCCESS) {
    return std::unexpected(VulkanStatus::error(result));
  }
  images_.resize(image_count);
  if (const VkResult result =
          vkGetSwapchainImagesKHR(handle, raw, &image_count, images_.data());
      result != VK_SUCCESS) {
    return std::unexpected(VulkanStatus::error(result));
  }
  for (VkImage image : images_) {
    const VkImageViewCreateInfo view_info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format->format,
        .components = {},
        .subresourceRange =
            VkImageSubresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                    .baseMipLevel = 0,
                                    .levelCount = 1,
                                    .baseArrayLayer = 0,
                                    .layerCount = 1}};
    VkImageView view = VK_NULL_HANDLE;
    if (const VkResult result =
            vkCreateImageView(handle, &view_info, nullptr, &view);
        result != VK_SUCCESS) {
      return std::unexpected(VulkanStatus::error(result));
    }
    views_.emplace_back(
        VulkanImageViewHandle::handle_type{.device = handle, .object = view});
    raw_views_.push_back(view);
    const VkSemaphoreCreateInfo semaphore_info{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0};
    VkSemaphore semaphore = VK_NULL_HANDLE;
    if (const VkResult result =
            vkCreateSemaphore(handle, &semaphore_info, nullptr, &semaphore);
        result != VK_SUCCESS) {
      return std::unexpected(VulkanStatus::error(result));
    }
    finished_.emplace_back(VulkanSemaphoreHandle::handle_type{
        .device = handle, .object = semaphore});
  }
  drawn_.assign(images_.size(), false);
  format_ = format->format;
  extent_ = SurfaceSize{.width = SurfaceWidth{extent.width},
                        .height = SurfaceHeight{extent.height}};
  requested_ = wanted;
  stale_ = false;
  last_present_.reset();
  ++generation_;
  return {};
}

auto SwapchainTarget::acquire(VkSemaphore signal)
    -> std::expected<std::optional<Image>, ProviderError> {
  if (!swapchain_.valid()) {
    return std::optional<Image>{};
  }
  acquired_at_ = std::chrono::steady_clock::now();
  std::uint32_t index = 0;
  const VkResult result =
      vkAcquireNextImageKHR(device_->handle(), swapchain_.get().object,
                            std::numeric_limits<std::uint64_t>::max(), signal,
                            VK_NULL_HANDLE, &index);
  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    stale_ = true;
    return std::optional<Image>{};
  }
  if (result == VK_SUBOPTIMAL_KHR) {
    stale_ = true;
  } else if (result != VK_SUCCESS) {
    return std::unexpected(VulkanStatus::error(result));
  }
  if (index >= images_.size()) {
    return std::unexpected(ProviderError{ProviderErrorCode::platform_failure});
  }
  return Image{
      .image = images_[index],
      .index = index,
      .layout = drawn_[index] ? presented_layout() : VK_IMAGE_LAYOUT_UNDEFINED,
      .signals = true};
}

auto SwapchainTarget::finished(std::uint32_t index) const noexcept
    -> VkSemaphore {
  return index < finished_.size() ? finished_[index].get().object
                                  : VK_NULL_HANDLE;
}

auto SwapchainTarget::present(VkQueue queue, std::uint32_t index)
    -> std::expected<void, ProviderError> {
  if (index >= images_.size()) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument});
  }
  VkSemaphore const wait = finished_[index].get().object;
  VkSwapchainKHR const swapchain = swapchain_.get().object;
  const VkPresentInfoKHR info{.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                              .pNext = nullptr,
                              .waitSemaphoreCount = 1,
                              .pWaitSemaphores = &wait,
                              .swapchainCount = 1,
                              .pSwapchains = &swapchain,
                              .pImageIndices = &index,
                              .pResults = nullptr};
  const VkResult result = vkQueuePresentKHR(queue, &info);
  drawn_[index] = true;
  if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    stale_ = true;
  } else if (result != VK_SUCCESS) {
    return std::unexpected(VulkanStatus::error(result));
  }
  if (acquired_at_.has_value()) {
    count_vsync(*acquired_at_);
  }
  return {};
}

void SwapchainTarget::set_interval(std::uint32_t interval) {
  VulkanTarget::set_interval(interval);
  if (interval == options_.interval) {
    return;
  }
  if (PresentMode::rebuilds(options_.interval, interval)) {
    stale_ = true;
  }
  options_.interval = interval;
  last_present_.reset();
  if (options_.pace) {
    options_.pace(interval);
  }
}

void SwapchainTarget::count_vsync(std::chrono::steady_clock::time_point began) {
  const auto now = std::chrono::steady_clock::now();
  const std::optional<std::chrono::steady_clock::time_point> previous =
      std::exchange(last_present_, now);
  if (!previous.has_value() || options_.interval == 0) {
    return;
  }
  const auto interval = static_cast<double>(options_.interval);
  const auto period = static_cast<double>(vsync_period.count());
  const auto waited = static_cast<double>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(began - *previous)
          .count());
  if (waited > (interval + 0.5) * period) {
    return;
  }
  const auto elapsed = static_cast<double>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(now - *previous)
          .count());
  const std::int64_t periods = std::llround(elapsed / period);
  if (periods > static_cast<std::int64_t>(options_.interval) &&
      periods <= pause_periods) {
    missed_ += static_cast<std::uint64_t>(
        periods - static_cast<std::int64_t>(options_.interval));
  }
}

}
