#include <provider/vulkan/resident_block.hpp>

#include <algorithm>
#include <limits>
#include <utility>

namespace arisa::provider::vulkan {

auto ResidentBlock::reserve_indices(const VulkanDevice& device,
                                    std::uint32_t count)
    -> std::expected<void, ProviderError> {
  if (indices_.has_value() && index_capacity_ >= count) {
    return {};
  }
  const std::uint64_t grown =
      std::max<std::uint64_t>({count, minimum_indices, 2ULL * index_capacity_});
  if (grown * sizeof(Index) > std::numeric_limits<std::uint32_t>::max()) {
    return std::unexpected(
        ProviderError{ProviderErrorCode::resource_exhausted});
  }
  auto created =
      VulkanBuffer::create(device, grown * sizeof(Index),
                           VK_BUFFER_USAGE_INDEX_BUFFER_BIT, MemoryUse::upload);
  if (!created.has_value()) {
    return std::unexpected(created.error());
  }
  indices_ = std::move(*created);
  index_capacity_ = static_cast<std::uint32_t>(grown);
  return {};
}

auto ResidentBlock::indices(std::uint32_t count) const noexcept
    -> std::span<Index> {
  if (!indices_.has_value()) {
    return {};
  }
  return std::span{
      static_cast<Index*>(static_cast<void*>(indices_->bytes().data())),
      index_capacity_}
      .first(std::min(count, index_capacity_));
}

void ResidentBlock::stale(VertexRange range) {
  if (whole_) {
    return;
  }
  if (!ranges_.empty() &&
      ranges_.back().first + ranges_.back().count == range.first) {
    ranges_.back().count += range.count;
    return;
  }
  if (ranges_.size() >= maximum_ranges) {
    whole_ = true;
    ranges_.clear();
    return;
  }
  ranges_.push_back(range);
}

auto ResidentBlock::refresh(const VulkanDevice& device,
                            std::span<const ShapeVertex> mirror,
                            std::uint32_t index_count)
    -> std::expected<std::size_t, ProviderError> {
  const auto size = static_cast<std::uint32_t>(mirror.size());
  if (!vertices_.has_value() || vertex_capacity_ < size) {
    const std::uint64_t grown = std::max<std::uint64_t>(
        {size, minimum_vertices, 2ULL * vertex_capacity_});
    if (grown * sizeof(ShapeVertex) >
        std::numeric_limits<std::uint32_t>::max()) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::resource_exhausted});
    }
    auto created = VulkanBuffer::create(device, grown * sizeof(ShapeVertex),
                                        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                                        MemoryUse::upload);
    if (!created.has_value()) {
      return std::unexpected(created.error());
    }
    vertices_ = std::move(*created);
    vertex_capacity_ = static_cast<std::uint32_t>(grown);
    whole_ = true;
  }
  const std::span<std::byte> target = vertices_->bytes();
  std::size_t copied = 0;
  if (whole_) {
    std::ranges::copy(std::as_bytes(mirror), target.begin());
    copied = mirror.size_bytes();
    vertices_->flush(0, copied);
  } else {
    for (const VertexRange& range : ranges_) {
      const std::uint32_t first = std::min(range.first, size);
      const std::uint32_t count = std::min(range.count, size - first);
      const std::span<const std::byte> source =
          std::as_bytes(mirror.subspan(first, count));
      const std::size_t offset =
          static_cast<std::size_t>(first) * sizeof(ShapeVertex);
      std::ranges::copy(source, target.subspan(offset).begin());
      vertices_->flush(offset, source.size());
      copied += source.size();
    }
  }
  if (indices_.has_value()) {
    indices_->flush(0, static_cast<VkDeviceSize>(index_count) * sizeof(Index));
  }
  ranges_.clear();
  whole_ = false;
  return copied;
}

void ResidentBlock::flush_indices(std::uint32_t count) {
  if (indices_.has_value() && count > 0) {
    indices_->flush(0, static_cast<VkDeviceSize>(count) * sizeof(Index));
  }
}

auto ResidentBlock::vertex_buffer() const noexcept -> VkBuffer {
  return vertices_.has_value() ? vertices_->handle() : VK_NULL_HANDLE;
}

auto ResidentBlock::index_buffer() const noexcept -> VkBuffer {
  return indices_.has_value() ? indices_->handle() : VK_NULL_HANDLE;
}

}
