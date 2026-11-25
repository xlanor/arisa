#pragma once

#include <arisa/provider/error.hpp>
#include <arisa/provider/gpu/free_ranges.hpp>
#include <arisa/provider/gpu/suballocator.hpp>

#include <vulkan/vulkan.h>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>

namespace arisa::provider::vulkan {

enum class MemoryTiling : std::uint8_t {
  linear,
  optimal,
};

class MemoryAllocator {
 public:
  static constexpr VkDeviceSize block_size = VkDeviceSize{16} << 20U;
  static constexpr VkDeviceSize dedicated_threshold = block_size / 2;

  struct Request {
    VkMemoryRequirements requirements{};
    std::uint32_t type{0};
    VkMemoryPropertyFlags flags{0};
    MemoryTiling tiling{MemoryTiling::linear};
  };

  struct Statistics {
    std::uint64_t blocks_created{0};
    std::uint64_t blocks_live{0};
    std::uint64_t allocations_made{0};
    std::uint64_t allocations_live{0};

    friend constexpr auto operator==(const Statistics&, const Statistics&)
        -> bool = default;
  };

  class Pool;
  class Block;

  class Allocation {
   public:
    Allocation(std::shared_ptr<Pool> pool, Block& block, gpu::BlockId id,
               gpu::ByteRange range) noexcept;
    Allocation(Allocation&& other) noexcept;
    auto operator=(Allocation&& other) noexcept -> Allocation&;
    Allocation(const Allocation&) = delete;
    auto operator=(const Allocation&) -> Allocation& = delete;
    ~Allocation();

    auto memory() const noexcept -> VkDeviceMemory;

    auto offset() const noexcept -> VkDeviceSize { return range_.offset; }

    auto size() const noexcept -> VkDeviceSize { return range_.size; }

    auto block() const noexcept -> std::uint64_t;

    auto mapped() const noexcept -> std::span<std::byte>;

    void flush(VkDeviceSize offset, VkDeviceSize length) const noexcept;

    void invalidate() const noexcept;

   private:
    void release() noexcept;

    std::shared_ptr<Pool> pool_;
    Block* block_{nullptr};
    gpu::BlockId id_{0};
    gpu::ByteRange range_{};
  };

  MemoryAllocator(VkDevice device, VkDeviceSize atom);

  [[nodiscard]] auto allocate(const Request& request) const
      -> std::expected<Allocation, ProviderError>;

  auto statistics() const -> Statistics;

 private:
  std::shared_ptr<Pool> pool_;
};

}
