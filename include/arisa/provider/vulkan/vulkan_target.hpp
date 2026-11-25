#pragma once

#include <arisa/provider/error.hpp>
#include <arisa/provider/surface/surface_size.hpp>

#include <vulkan/vulkan.h>

#include <cstdint>
#include <expected>
#include <optional>
#include <span>

namespace arisa::provider::vulkan {

class VulkanTarget {
 public:
  struct Image {
    VkImage image{VK_NULL_HANDLE};
    std::uint32_t index{0};
    VkImageLayout layout{VK_IMAGE_LAYOUT_UNDEFINED};
    bool signals{false};
  };

  virtual ~VulkanTarget() = default;

  [[nodiscard]] virtual auto prepare(SurfaceSize wanted)
      -> std::expected<bool, ProviderError> = 0;

  virtual auto generation() const noexcept -> std::uint64_t = 0;

  virtual auto format() const noexcept -> VkFormat = 0;

  virtual auto usage() const noexcept -> VkImageUsageFlags = 0;

  virtual auto extent() const noexcept -> SurfaceSize = 0;

  virtual auto views() const noexcept -> std::span<const VkImageView> = 0;

  [[nodiscard]] virtual auto acquire(VkSemaphore signal)
      -> std::expected<std::optional<Image>, ProviderError> = 0;

  virtual auto finished(std::uint32_t index) const noexcept -> VkSemaphore = 0;

  virtual auto presented_layout() const noexcept -> VkImageLayout = 0;

  [[nodiscard]] virtual auto present(VkQueue queue, std::uint32_t index)
      -> std::expected<void, ProviderError> = 0;

  virtual auto readable() const noexcept -> std::optional<Image> = 0;

  virtual auto preserves() const noexcept -> bool { return true; }

  virtual auto missed_vsyncs() const noexcept -> std::uint64_t { return 0; }

  virtual auto paced() const noexcept -> bool { return false; }

  virtual void set_interval(std::uint32_t interval) { interval_ = interval; }

  virtual void invalidate() noexcept {}

  auto interval() const noexcept -> std::uint32_t { return interval_; }

 private:
  std::uint32_t interval_{1};
};

}
