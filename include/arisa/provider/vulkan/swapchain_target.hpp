#pragma once

#include <arisa/provider/error.hpp>
#include <arisa/provider/surface/provider.hpp>
#include <arisa/provider/surface/surface_size.hpp>
#include <arisa/provider/vulkan/device_object.hpp>
#include <arisa/provider/vulkan/vulkan_device.hpp>
#include <arisa/provider/vulkan/vulkan_surface.hpp>
#include <arisa/provider/vulkan/vulkan_target.hpp>

#include <vulkan/vulkan.h>

#include <chrono>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <vector>

namespace arisa::provider::vulkan {

class SwapchainTarget final : public VulkanTarget {
 public:
  static constexpr std::chrono::nanoseconds vsync_period{16'666'667};
  static constexpr std::int64_t pause_periods = 30;

  struct Options {
    std::uint32_t images{2};
    std::uint32_t interval{1};
    std::function<void(std::uint32_t)> pace{};
  };

  [[nodiscard]] static auto create(std::shared_ptr<const VulkanDevice> device,
                                   VulkanSurface surface,
                                   const SurfaceProvider& window,
                                   Options options)
      -> std::expected<SwapchainTarget, ProviderError>;

  [[nodiscard]] auto prepare(SurfaceSize wanted)
      -> std::expected<bool, ProviderError> override;

  auto generation() const noexcept -> std::uint64_t override {
    return generation_;
  }

  auto format() const noexcept -> VkFormat override { return format_; }

  auto usage() const noexcept -> VkImageUsageFlags override { return usage_; }

  auto extent() const noexcept -> SurfaceSize override { return extent_; }

  auto views() const noexcept -> std::span<const VkImageView> override {
    return raw_views_;
  }

  [[nodiscard]] auto acquire(VkSemaphore signal)
      -> std::expected<std::optional<Image>, ProviderError> override;

  auto finished(std::uint32_t index) const noexcept -> VkSemaphore override;

  auto presented_layout() const noexcept -> VkImageLayout override {
    return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  }

  [[nodiscard]] auto present(VkQueue queue, std::uint32_t index)
      -> std::expected<void, ProviderError> override;

  auto readable() const noexcept -> std::optional<Image> override {
    return std::nullopt;
  }

  auto missed_vsyncs() const noexcept -> std::uint64_t override {
    return missed_;
  }

  auto paced() const noexcept -> bool override { return true; }

  void set_interval(std::uint32_t interval) override;

  void invalidate() noexcept override { stale_ = true; }

 private:
  SwapchainTarget(std::shared_ptr<const VulkanDevice> device,
                  VulkanSurface surface, const SurfaceProvider& window,
                  Options options) noexcept;

  [[nodiscard]] auto rebuild(SurfaceSize wanted)
      -> std::expected<void, ProviderError>;

  auto chosen_format() const -> std::optional<VkSurfaceFormatKHR>;

  auto chosen_mode() const -> VkPresentModeKHR;

  void count_vsync(std::chrono::steady_clock::time_point began);

  std::shared_ptr<const VulkanDevice> device_;
  VulkanSurface surface_;
  std::reference_wrapper<const SurfaceProvider> window_;
  Options options_;
  VulkanSwapchainHandle swapchain_;
  std::vector<VkImage> images_;
  std::vector<VulkanImageViewHandle> views_;
  std::vector<VkImageView> raw_views_;
  std::vector<VulkanSemaphoreHandle> finished_;
  std::vector<bool> drawn_;
  VkFormat format_{VK_FORMAT_UNDEFINED};
  VkImageUsageFlags usage_{0};
  SurfaceSize extent_{};
  SurfaceSize requested_{};
  std::uint64_t generation_{0};
  bool stale_{true};
  std::optional<std::chrono::steady_clock::time_point> last_present_;
  std::optional<std::chrono::steady_clock::time_point> acquired_at_;
  std::uint64_t missed_{0};
};

}
