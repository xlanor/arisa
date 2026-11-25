#pragma once

#include <arisa/provider/error.hpp>
#include <arisa/provider/gpu/alignment.hpp>
#include <arisa/provider/gpu/free_ranges.hpp>
#include <arisa/support/strong_value.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace arisa::provider::gpu {

struct BlockIdTag;

using BlockId = support::StrongValue<std::uint64_t, BlockIdTag>;

struct BlockRequest {
  std::uint64_t bytes{0};
  Alignment alignment{Alignment::of<1>()};
  bool dedicated{false};
};

template <class Native>
struct NativeBlock {
  Native native;
  std::uint64_t base{0};
  std::uint64_t usable{0};
  std::uint64_t reserved{0};
};

struct SuballocatorPolicy {
  std::uint64_t block_size{0};
  std::uint64_t dedicated_above{0};
};

struct SuballocatorStatistics {
  std::uint64_t blocks_created{0};
  std::uint64_t blocks_live{0};
  std::uint64_t allocations_made{0};
  std::uint64_t allocations_live{0};
  std::uint64_t reserved_bytes{0};

  friend constexpr auto operator==(const SuballocatorStatistics&,
                                   const SuballocatorStatistics&)
      -> bool = default;
};

template <class Native>
  requires std::is_nothrow_move_constructible_v<Native>
class Suballocator {
 public:
  using Policy = SuballocatorPolicy;
  using Statistics = SuballocatorStatistics;
  using Made = std::expected<NativeBlock<Native>, ProviderError>;

  struct Placement {
    BlockId block;
    std::reference_wrapper<Native> native;
    ByteRange range;
  };

  explicit Suballocator(Policy policy) noexcept : policy_(policy) {}

  template <class Make>
    requires std::is_invocable_r_v<Made, Make&, BlockRequest>
  [[nodiscard]] auto allocate(std::uint64_t size, Alignment alignment,
                              Make&& make)
      -> std::expected<Placement, ProviderError> {
    if (size == 0) {
      return std::unexpected(
          ProviderError{ProviderErrorCode::invalid_argument});
    }
    if (size <= policy_.dedicated_above) {
      for (const std::unique_ptr<Block>& block : blocks_) {
        if (block->dedicated()) {
          continue;
        }
        if (const std::optional<ByteRange> range =
                block->take(size, alignment)) {
          return placed(*block, *range);
        }
      }
      if (auto created = create(make, BlockRequest{.bytes = policy_.block_size,
                                                   .alignment = alignment,
                                                   .dedicated = false})) {
        if (const std::optional<ByteRange> range =
                (*created)->take(size, alignment)) {
          return placed(**created, *range);
        }
      }
    }
    auto created = create(
        make,
        BlockRequest{.bytes = size, .alignment = alignment, .dedicated = true});
    if (!created.has_value()) {
      return std::unexpected(created.error());
    }
    const std::optional<ByteRange> range = (*created)->take(size, alignment);
    if (!range.has_value()) {
      erase((*created)->id());
      return std::unexpected(
          ProviderError{ProviderErrorCode::resource_exhausted});
    }
    return placed(**created, *range);
  }

  void release(BlockId id, ByteRange range) noexcept {
    const auto found = std::ranges::find(blocks_, id, &Suballocator::identify);
    if (found == blocks_.end()) {
      return;
    }
    Block& block = **found;
    block.give(range);
    --statistics_.allocations_live;
    if (!block.empty()) {
      return;
    }
    if (block.dedicated() ||
        std::ranges::any_of(blocks_, [&block](
                                         const std::unique_ptr<Block>& other) {
          return other.get() != &block && !other->dedicated() && other->empty();
        })) {
      erase(id);
    }
  }

  auto statistics() const noexcept -> Statistics { return statistics_; }

  auto policy() const noexcept -> Policy { return policy_; }

 private:
  class Block {
   public:
    Block(BlockId id, NativeBlock<Native> made, bool dedicated)
        : id_(id),
          native_(std::move(made.native)),
          free_(made.base, made.usable),
          reserved_(made.reserved),
          dedicated_(dedicated) {}

    auto take(std::uint64_t size, Alignment alignment)
        -> std::optional<ByteRange> {
      std::optional<ByteRange> range = free_.take(size, alignment);
      if (range.has_value()) {
        ++live_;
      }
      return range;
    }

    void give(ByteRange range) {
      free_.give(range);
      --live_;
    }

    auto id() const noexcept -> BlockId { return id_; }

    auto native() noexcept -> Native& { return native_; }

    auto reserved() const noexcept -> std::uint64_t { return reserved_; }

    auto dedicated() const noexcept -> bool { return dedicated_; }

    auto empty() const noexcept -> bool { return live_ == 0; }

   private:
    BlockId id_;
    Native native_;
    FreeRanges free_;
    std::uint64_t reserved_;
    std::size_t live_{0};
    bool dedicated_;
  };

  template <class Make>
  auto create(Make& make, BlockRequest request)
      -> std::expected<Block*, ProviderError> {
    Made made = std::invoke(make, request);
    if (!made.has_value()) {
      return std::unexpected(made.error());
    }
    blocks_.push_back(std::make_unique<Block>(
        BlockId{next_id_++}, std::move(*made), request.dedicated));
    ++statistics_.blocks_created;
    ++statistics_.blocks_live;
    statistics_.reserved_bytes += blocks_.back()->reserved();
    return blocks_.back().get();
  }

  void erase(BlockId id) noexcept {
    const auto found = std::ranges::find(blocks_, id, &Suballocator::identify);
    if (found == blocks_.end()) {
      return;
    }
    statistics_.reserved_bytes -= (*found)->reserved();
    --statistics_.blocks_live;
    blocks_.erase(found);
  }

  static auto identify(const std::unique_ptr<Block>& block) noexcept
      -> BlockId {
    return block->id();
  }

  auto placed(Block& block, ByteRange range) -> Placement {
    ++statistics_.allocations_made;
    ++statistics_.allocations_live;
    return Placement{.block = block.id(),
                     .native = std::ref(block.native()),
                     .range = range};
  }

  Policy policy_;
  std::vector<std::unique_ptr<Block>> blocks_;
  Statistics statistics_;
  std::uint64_t next_id_{1};
};

}
