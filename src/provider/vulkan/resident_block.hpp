#pragma once

#include <arisa/provider/error.hpp>
#include <arisa/provider/gpu/draw_data.hpp>
#include <arisa/provider/gpu/shape_vertex.hpp>
#include <arisa/provider/gpu/vertex.hpp>
#include <arisa/provider/vulkan/vulkan_buffer.hpp>
#include <arisa/provider/vulkan/vulkan_device.hpp>

#include <vulkan/vulkan.h>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <vector>

namespace arisa::provider::vulkan {

class ResidentBlock {
 public:
  static constexpr std::size_t maximum_ranges = 256;
  static constexpr std::uint32_t minimum_vertices = 4096;
  static constexpr std::uint32_t minimum_indices = 8192;

  [[nodiscard]] auto reserve_indices(const VulkanDevice& device,
                                     std::uint32_t count)
      -> std::expected<void, ProviderError>;

  auto indices(std::uint32_t count) const noexcept -> std::span<Index>;

  void stale(VertexRange range);

  [[nodiscard]] auto refresh(const VulkanDevice& device,
                             std::span<const ShapeVertex> mirror,
                             std::uint32_t index_count)
      -> std::expected<std::size_t, ProviderError>;

  void flush_indices(std::uint32_t count);

  auto vertex_buffer() const noexcept -> VkBuffer;

  auto index_buffer() const noexcept -> VkBuffer;

 private:
  std::optional<VulkanBuffer> vertices_;
  std::optional<VulkanBuffer> indices_;
  std::uint32_t vertex_capacity_{0};
  std::uint32_t index_capacity_{0};
  std::vector<VertexRange> ranges_;
  bool whole_{true};
};

}
