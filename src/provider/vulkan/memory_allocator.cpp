#include <arisa/provider/vulkan/device_object.hpp>
#include <arisa/provider/vulkan/memory_allocator.hpp>
#include <arisa/provider/vulkan/vulkan_status.hpp>

#include <algorithm>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <ranges>
#include <utility>
#include <vector>

namespace arisa::provider::vulkan {

class MemoryAllocator::Block {
 public:
  struct Lane {
    std::uint32_t type{0};
    MemoryTiling tiling{MemoryTiling::linear};

    friend constexpr auto operator<=>(const Lane&, const Lane&) = default;
  };

  Block(VulkanMemoryHandle memory, Lane lane, VkDeviceSize size,
        std::span<std::byte> mapped, bool coherent) noexcept
      : memory_(std::move(memory)),
        lane_(lane),
        size_(size),
        mapped_(mapped),
        coherent_(coherent) {}

  auto memory() const noexcept -> VkDeviceMemory {
    return memory_.get().object;
  }

  auto device() const noexcept -> VkDevice { return memory_.get().device; }

  auto lane() const noexcept -> Lane { return lane_; }

  auto size() const noexcept -> VkDeviceSize { return size_; }

  auto mapped() const noexcept -> std::span<std::byte> { return mapped_; }

  auto coherent() const noexcept -> bool { return coherent_; }

 private:
  VulkanMemoryHandle memory_;
  Lane lane_;
  VkDeviceSize size_;
  std::span<std::byte> mapped_;
  bool coherent_;
};

class MemoryAllocator::Pool {
 public:
  using Lanes = std::map<Block::Lane, gpu::Suballocator<Block>>;

  static constexpr gpu::SuballocatorPolicy policy{
      .block_size = block_size, .dedicated_above = dedicated_threshold};

  Pool(VkDevice device, VkDeviceSize atom) noexcept
      : device_(device), atom_(std::max<VkDeviceSize>(atom, 1)) {}

  auto allocate(const std::shared_ptr<Pool>& self, const Request& request)
      -> std::expected<Allocation, ProviderError> {
    const bool coherent =
        (request.flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;
    const bool visible =
        (request.flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;
    VkDeviceSize alignment =
        std::max<VkDeviceSize>(request.requirements.alignment, 1);
    VkDeviceSize size = std::max<VkDeviceSize>(request.requirements.size, 1);
    if (visible && !coherent) {
      alignment = std::max(alignment, atom_);
      size = (size + atom_ - 1) / atom_ * atom_;
    }
    const std::optional<gpu::Alignment> aligned =
        gpu::Alignment::create(alignment);
    if (!aligned.has_value()) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument});
    }
    const Block::Lane lane{.type = request.type, .tiling = request.tiling};
    const std::scoped_lock lock{mutex_};
    auto placed =
        lanes_.try_emplace(lane, policy)
            .first->second.allocate(
                size, *aligned, [this, &request, lane](gpu::BlockRequest made) {
                  return create(request, lane, made.bytes);
                });
    if (!placed.has_value()) {
      return std::unexpected(placed.error());
    }
    return Allocation{self, placed->native.get(), placed->block, placed->range};
  }

  void release(const Block& block, gpu::BlockId id,
               gpu::ByteRange range) noexcept {
    const Block::Lane lane = block.lane();
    const std::scoped_lock lock{mutex_};
    if (const auto found = lanes_.find(lane); found != lanes_.end()) {
      found->second.release(id, range);
    }
  }

  auto statistics() const -> Statistics {
    const std::scoped_lock lock{mutex_};
    Statistics total;
    for (const gpu::Suballocator<Block>& each : lanes_ | std::views::values) {
      const gpu::SuballocatorStatistics counted = each.statistics();
      total.blocks_created += counted.blocks_created;
      total.blocks_live += counted.blocks_live;
      total.allocations_made += counted.allocations_made;
      total.allocations_live += counted.allocations_live;
    }
    return total;
  }

  auto atom() const noexcept -> VkDeviceSize { return atom_; }

 private:
  auto create(const Request& request, Block::Lane lane, VkDeviceSize size)
      -> std::expected<gpu::NativeBlock<Block>, ProviderError> {
    const VkMemoryAllocateInfo info{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = size,
        .memoryTypeIndex = request.type};
    VkDeviceMemory raw = VK_NULL_HANDLE;
    if (const VkResult result = vkAllocateMemory(device_, &info, nullptr, &raw);
        result != VK_SUCCESS) {
      return std::unexpected(VulkanStatus::error(result));
    }
    VulkanMemoryHandle memory{{.device = device_, .object = raw}};
    std::span<std::byte> mapped;
    if ((request.flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0) {
      void* pointer = nullptr;
      if (const VkResult result =
              vkMapMemory(device_, raw, 0, VK_WHOLE_SIZE, 0, &pointer);
          result != VK_SUCCESS) {
        return std::unexpected(VulkanStatus::error(result));
      }
      const std::size_t length = size;
      mapped = std::span{static_cast<std::byte*>(pointer), length};
    }
    return gpu::NativeBlock<Block>{
        .native =
            Block{std::move(memory), lane, size, mapped,
                  (request.flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0},
        .base = 0,
        .usable = size,
        .reserved = size};
  }

  mutable std::mutex mutex_;
  VkDevice device_;
  VkDeviceSize atom_;
  Lanes lanes_;
};

MemoryAllocator::Allocation::Allocation(std::shared_ptr<Pool> pool,
                                        Block& block, gpu::BlockId id,
                                        gpu::ByteRange range) noexcept
    : pool_(std::move(pool)), block_(&block), id_(id), range_(range) {}

MemoryAllocator::Allocation::Allocation(Allocation&& other) noexcept
    : pool_(std::move(other.pool_)),
      block_(std::exchange(other.block_, nullptr)),
      id_(std::exchange(other.id_, gpu::BlockId{0})),
      range_(std::exchange(other.range_, gpu::ByteRange{})) {}

auto MemoryAllocator::Allocation::operator=(Allocation&& other) noexcept
    -> Allocation& {
  if (this != &other) {
    release();
    pool_ = std::move(other.pool_);
    block_ = std::exchange(other.block_, nullptr);
    id_ = std::exchange(other.id_, gpu::BlockId{0});
    range_ = std::exchange(other.range_, gpu::ByteRange{});
  }
  return *this;
}

MemoryAllocator::Allocation::~Allocation() { release(); }

void MemoryAllocator::Allocation::release() noexcept {
  if (pool_ != nullptr && block_ != nullptr) {
    pool_->release(*block_, id_, range_);
  }
  block_ = nullptr;
  pool_.reset();
}

auto MemoryAllocator::Allocation::memory() const noexcept -> VkDeviceMemory {
  return block_ != nullptr ? block_->memory() : VK_NULL_HANDLE;
}

auto MemoryAllocator::Allocation::block() const noexcept -> std::uint64_t {
  return block_ != nullptr ? id_.value() : 0;
}

auto MemoryAllocator::Allocation::mapped() const noexcept
    -> std::span<std::byte> {
  if (block_ == nullptr || block_->mapped().empty()) {
    return {};
  }
  const std::size_t start = range_.offset;
  const std::size_t length = range_.size;
  return block_->mapped().subspan(start, length);
}

void MemoryAllocator::Allocation::flush(VkDeviceSize offset,
                                        VkDeviceSize length) const noexcept {
  if (block_ == nullptr || block_->coherent() || block_->mapped().empty() ||
      length == 0) {
    return;
  }
  const VkDeviceSize atom = pool_->atom();
  const VkDeviceSize limit = block_->size();
  const VkDeviceSize first = range_.offset + offset;
  const VkDeviceSize start = first - (first % atom);
  const VkDeviceSize end =
      std::min((first + length + atom - 1) / atom * atom, limit);
  const VkMappedMemoryRange range{
      .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
      .pNext = nullptr,
      .memory = block_->memory(),
      .offset = start,
      .size = end - start};
  static_cast<void>(vkFlushMappedMemoryRanges(block_->device(), 1, &range));
}

void MemoryAllocator::Allocation::invalidate() const noexcept {
  if (block_ == nullptr || block_->coherent() || block_->mapped().empty()) {
    return;
  }
  const VkDeviceSize atom = pool_->atom();
  const VkDeviceSize start = range_.offset - (range_.offset % atom);
  const VkDeviceSize end = std::min(
      (range_.offset + range_.size + atom - 1) / atom * atom, block_->size());
  const VkMappedMemoryRange range{
      .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
      .pNext = nullptr,
      .memory = block_->memory(),
      .offset = start,
      .size = end - start};
  static_cast<void>(
      vkInvalidateMappedMemoryRanges(block_->device(), 1, &range));
}

MemoryAllocator::MemoryAllocator(VkDevice device, VkDeviceSize atom)
    : pool_(std::make_shared<Pool>(device, atom)) {}

auto MemoryAllocator::allocate(const Request& request) const
    -> std::expected<Allocation, ProviderError> {
  return pool_->allocate(pool_, request);
}

auto MemoryAllocator::statistics() const -> Statistics {
  return pool_->statistics();
}

}
