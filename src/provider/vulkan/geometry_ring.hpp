#pragma once

#include <arisa/provider/error.hpp>
#include <arisa/provider/gpu/draw_data.hpp>
#include <arisa/provider/gpu/vertex.hpp>
#include <arisa/provider/vulkan/vulkan_buffer.hpp>
#include <arisa/provider/vulkan/vulkan_device.hpp>

#include <vulkan/vulkan.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace arisa::provider::vulkan {

template <class V>
class GeometryRing {
 public:
  using Geometry = provider::Geometry<V>;

  static constexpr std::uint32_t minimum_vertices = 4096;
  static constexpr std::uint32_t minimum_indices = 8192;

  class Block {
   public:
    [[nodiscard]] static auto create(const VulkanDevice& device,
                                     std::uint32_t vertex_capacity,
                                     std::uint32_t index_capacity)
        -> std::expected<Block, ProviderError> {
      auto vertices = VulkanBuffer::create(
          device, static_cast<VkDeviceSize>(vertex_capacity) * sizeof(V),
          VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, MemoryUse::upload);
      if (!vertices.has_value()) {
        return std::unexpected(vertices.error());
      }
      auto indices = VulkanBuffer::create(
          device, static_cast<VkDeviceSize>(index_capacity) * sizeof(Index),
          VK_BUFFER_USAGE_INDEX_BUFFER_BIT, MemoryUse::upload);
      if (!indices.has_value()) {
        return std::unexpected(indices.error());
      }
      return Block{std::move(*vertices), std::move(*indices), vertex_capacity,
                   index_capacity};
    }

    auto vertex_capacity() const noexcept -> std::uint32_t {
      return vertex_capacity_;
    }

    auto index_capacity() const noexcept -> std::uint32_t {
      return index_capacity_;
    }

    auto fits(std::uint32_t vertices, std::uint32_t indices) const noexcept
        -> bool {
      return vertices <= vertex_capacity_ - vertices_used_ &&
             indices <= index_capacity_ - indices_used_;
    }

    auto take(std::uint32_t vertices, std::uint32_t indices,
              std::uint32_t index) -> Geometry {
      const Geometry geometry{
          .vertices = mapped_vertices().subspan(vertices_used_, vertices),
          .indices = mapped_indices().subspan(indices_used_, indices),
          .block = index,
          .first_vertex = vertices_used_,
          .first_index = indices_used_};
      vertices_used_ += vertices;
      indices_used_ += indices;
      return geometry;
    }

    auto owns(const Geometry& geometry) const noexcept -> bool {
      return geometry.first_vertex <= vertices_used_ &&
             geometry.vertices.size() <=
                 vertices_used_ - geometry.first_vertex &&
             geometry.first_index <= indices_used_ &&
             geometry.indices.size() <= indices_used_ - geometry.first_index &&
             geometry.vertices.data() ==
                 mapped_vertices().subspan(geometry.first_vertex).data() &&
             geometry.indices.data() ==
                 mapped_indices().subspan(geometry.first_index).data();
    }

    auto used() const noexcept -> bool { return vertices_used_ > 0; }

    void reset() noexcept {
      vertices_used_ = 0;
      indices_used_ = 0;
    }

    void flush() const noexcept {
      vertices_.flush(0, static_cast<VkDeviceSize>(vertices_used_) * sizeof(V));
      indices_.flush(0,
                     static_cast<VkDeviceSize>(indices_used_) * sizeof(Index));
    }

    auto vertex_buffer() const noexcept -> VkBuffer {
      return vertices_.handle();
    }

    auto index_buffer() const noexcept -> VkBuffer { return indices_.handle(); }

   private:
    Block(VulkanBuffer vertices, VulkanBuffer indices,
          std::uint32_t vertex_capacity, std::uint32_t index_capacity) noexcept
        : vertices_(std::move(vertices)),
          indices_(std::move(indices)),
          vertex_capacity_(vertex_capacity),
          index_capacity_(index_capacity) {}

    auto mapped_vertices() const noexcept -> std::span<V> {
      return std::span{
          static_cast<V*>(static_cast<void*>(vertices_.bytes().data())),
          vertex_capacity_};
    }

    auto mapped_indices() const noexcept -> std::span<Index> {
      return std::span{
          static_cast<Index*>(static_cast<void*>(indices_.bytes().data())),
          index_capacity_};
    }

    VulkanBuffer vertices_;
    VulkanBuffer indices_;
    std::uint32_t vertex_capacity_;
    std::uint32_t index_capacity_;
    std::uint32_t vertices_used_{0};
    std::uint32_t indices_used_{0};
  };

  void reset(const VulkanDevice& device) {
    if (blocks_.size() > 1) {
      std::uint64_t vertices = 0;
      std::uint64_t indices = 0;
      for (const Block& block : blocks_) {
        vertices += block.vertex_capacity();
        indices += block.index_capacity();
      }
      blocks_.clear();
      if (auto merged = Block::create(device, clamp(vertices, sizeof(V)),
                                      clamp(indices, sizeof(Index)))) {
        blocks_.push_back(std::move(*merged));
      }
    }
    for (Block& block : blocks_) {
      block.reset();
    }
    cursor_ = 0;
  }

  [[nodiscard]] auto allocate(const VulkanDevice& device,
                              std::uint32_t vertices, std::uint32_t indices)
      -> std::expected<Geometry, ProviderError> {
    for (; cursor_ < blocks_.size(); ++cursor_) {
      if (blocks_[cursor_].fits(vertices, indices)) {
        return take(vertices, indices);
      }
    }
    const std::uint32_t previous_vertices =
        blocks_.empty() ? 0 : blocks_.back().vertex_capacity();
    const std::uint32_t previous_indices =
        blocks_.empty() ? 0 : blocks_.back().index_capacity();
    auto created =
        Block::create(device,
                      clamp(std::max<std::uint64_t>({vertices, minimum_vertices,
                                                     2ULL * previous_vertices}),
                            sizeof(V)),
                      clamp(std::max<std::uint64_t>({indices, minimum_indices,
                                                     2ULL * previous_indices}),
                            sizeof(Index)));
    if (!created.has_value()) {
      return std::unexpected(created.error());
    }
    if (!created->fits(vertices, indices)) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::resource_exhausted});
    }
    blocks_.push_back(std::move(*created));
    cursor_ = blocks_.size() - 1;
    return take(vertices, indices);
  }

  auto owns(const Geometry& geometry) const noexcept -> bool {
    return geometry.block < blocks_.size() &&
           blocks_[geometry.block].owns(geometry);
  }

  void flush() const noexcept {
    for (const Block& block : blocks_) {
      if (block.used()) {
        block.flush();
      }
    }
  }

  auto block(std::uint32_t index) const noexcept -> const Block* {
    return index < blocks_.size() ? &blocks_[index] : nullptr;
  }

 private:
  static auto clamp(std::uint64_t count, std::size_t stride) noexcept
      -> std::uint32_t {
    return static_cast<std::uint32_t>(std::min<std::uint64_t>(
        count, std::numeric_limits<std::uint32_t>::max() / stride));
  }

  auto take(std::uint32_t vertices, std::uint32_t indices) -> Geometry {
    return blocks_[cursor_].take(vertices, indices,
                                 static_cast<std::uint32_t>(cursor_));
  }

  std::vector<Block> blocks_;
  std::size_t cursor_{0};
};

}
