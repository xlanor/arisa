#pragma once

#include <arisa/provider/error.hpp>
#include <arisa/provider/surface/surface_size.hpp>
#include <arisa/provider/vulkan/vulkan_device.hpp>
#include <arisa/provider/vulkan/vulkan_image.hpp>
#include <arisa/provider/vulkan/vulkan_target.hpp>

#include <vulkan/vulkan.h>

#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace arisa::provider::vulkan {

class OffscreenTarget final : public VulkanTarget {
 public:
  static constexpr VkFormat image_format = VK_FORMAT_R8G8B8A8_UNORM;
  static constexpr VkImageUsageFlags image_usage =
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  static constexpr std::uint32_t minimum_images = 1;
  static constexpr std::uint32_t maximum_images = 3;

  struct Options {
    SurfaceSize size{};
    std::uint32_t images{3};
    bool preserved{true};
  };

  [[nodiscard]] static auto create(std::shared_ptr<const VulkanDevice> device,
                                   Options options)
      -> std::expected<OffscreenTarget, ProviderError>;

  [[nodiscard]] auto prepare(SurfaceSize wanted)
      -> std::expected<bool, ProviderError> override;

  auto generation() const noexcept -> std::uint64_t override {
    return generation_;
  }

  auto format() const noexcept -> VkFormat override { return image_format; }

  auto usage() const noexcept -> VkImageUsageFlags override {
    return image_usage;
  }

  auto extent() const noexcept -> SurfaceSize override { return size_; }

  auto views() const noexcept -> std::span<const VkImageView> override {
    return views_;
  }

  [[nodiscard]] auto acquire(VkSemaphore signal)
      -> std::expected<std::optional<Image>, ProviderError> override;

  auto finished(std::uint32_t index) const noexcept -> VkSemaphore override;

  auto presented_layout() const noexcept -> VkImageLayout override {
    return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  }

  [[nodiscard]] auto present(VkQueue queue, std::uint32_t index)
      -> std::expected<void, ProviderError> override;

  auto readable() const noexcept -> std::optional<Image> override;

  auto preserves() const noexcept -> bool override { return preserved_; }

 private:
  OffscreenTarget(std::shared_ptr<const VulkanDevice> device, SurfaceSize size,
                  std::vector<VulkanImage> images, bool preserved) noexcept;

  [[nodiscard]] static auto images(const VulkanDevice& device, SurfaceSize size,
                                   std::uint32_t count)
      -> std::expected<std::vector<VulkanImage>, ProviderError>;

  void adopt(SurfaceSize size, std::vector<VulkanImage> images);

  std::shared_ptr<const VulkanDevice> device_;
  SurfaceSize size_;
  std::vector<VulkanImage> images_;
  std::vector<VkImageView> views_;
  std::vector<bool> drawn_;
  std::optional<std::uint32_t> presented_;
  std::uint32_t next_{0};
  std::uint64_t generation_{1};
  bool preserved_;
};

}
