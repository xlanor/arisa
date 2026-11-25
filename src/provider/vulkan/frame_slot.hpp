#pragma once

#include <arisa/provider/error.hpp>
#include <arisa/provider/gpu/alignment.hpp>
#include <arisa/provider/gpu/shape_vertex.hpp>
#include <arisa/provider/gpu/upload_arena.hpp>
#include <arisa/provider/gpu/vertex.hpp>
#include <arisa/provider/vulkan/device_object.hpp>
#include <arisa/provider/vulkan/vulkan_buffer.hpp>
#include <arisa/provider/vulkan/vulkan_device.hpp>
#include <provider/vulkan/geometry_ring.hpp>
#include <provider/vulkan/resident_block.hpp>
#include <provider/vulkan/timestamp_queries.hpp>

#include <vulkan/vulkan.h>

#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <vector>

namespace arisa::provider::vulkan {

class FrameSlot {
 public:
  using Staging = gpu::UploadArena<VulkanBuffer>;

  static constexpr gpu::Alignment staging_alignment = gpu::Alignment::of<16>();
  static constexpr VkDeviceSize staging_chunk = 256 * 1024;

  struct Wait {
    VkSemaphore semaphore{VK_NULL_HANDLE};
    VkPipelineStageFlags stage{VK_PIPELINE_STAGE_ALL_COMMANDS_BIT};
    std::optional<std::uint64_t> value{};
  };

  [[nodiscard]] static auto create(const VulkanDevice& device)
      -> std::expected<FrameSlot, ProviderError>;

  [[nodiscard]] auto ready(const VulkanDevice& device)
      -> std::expected<bool, ProviderError>;

  void reset_geometry(const VulkanDevice& device);

  [[nodiscard]] auto submit(const VulkanDevice& device,
                            std::span<const Wait> waits, VkSemaphore signal,
                            std::uint64_t serial)
      -> std::expected<void, ProviderError>;

  auto poll(VkDevice device) noexcept -> std::optional<std::uint64_t>;

  void release_acquired(const VulkanDevice& device) noexcept;

  auto recording() const noexcept -> bool { return recording_; }

  auto pending() const noexcept -> bool { return pending_; }

  auto commands() const noexcept -> VkCommandBuffer { return commands_; }

  auto fence() const noexcept -> VkFence { return fence_.get().object; }

  auto acquired() const noexcept -> VkSemaphore {
    return acquired_.get().object;
  }

  auto serial() const noexcept -> std::uint64_t { return serial_; }

  auto geometry() noexcept -> GeometryRing<Vertex>& { return geometry_; }

  auto geometry() const noexcept -> const GeometryRing<Vertex>& {
    return geometry_;
  }

  auto shapes() noexcept -> GeometryRing<ShapeVertex>& { return shapes_; }

  auto shapes() const noexcept -> const GeometryRing<ShapeVertex>& {
    return shapes_;
  }

  auto resident() noexcept -> ResidentBlock& { return resident_; }

  auto resident() const noexcept -> const ResidentBlock& { return resident_; }

  auto staging() noexcept -> Staging& { return staging_; }

  auto timestamps() noexcept -> std::optional<TimestampQueries>& {
    return timestamps_;
  }

 private:
  FrameSlot(VulkanCommandPoolHandle pool, VkCommandBuffer commands,
            VulkanFenceHandle fence, VulkanSemaphoreHandle acquired,
            std::optional<TimestampQueries> timestamps) noexcept;

  VulkanCommandPoolHandle pool_;
  VkCommandBuffer commands_;
  VulkanFenceHandle fence_;
  VulkanSemaphoreHandle acquired_;
  GeometryRing<Vertex> geometry_;
  GeometryRing<ShapeVertex> shapes_;
  ResidentBlock resident_;
  Staging staging_{staging_alignment, staging_chunk};
  std::optional<TimestampQueries> timestamps_;
  std::vector<VkSemaphore> wait_semaphores_;
  std::vector<VkPipelineStageFlags> wait_stages_;
  std::vector<std::uint64_t> wait_values_;
  std::uint64_t serial_{0};
  bool recording_{false};
  bool pending_{false};
  bool dirty_{false};
};

}
