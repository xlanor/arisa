#include <arisa/provider/gpu/free_ranges.hpp>
#include <arisa/provider/gpu/suballocator.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <expected>
#include <random>
#include <utility>
#include <vector>

namespace arisa::provider::gpu {

class Ledger {
 public:
  class Block {
   public:
    Block(Ledger& ledger, std::uint64_t bytes) noexcept
        : ledger_(&ledger), bytes_(bytes) {}

    Block(Block&& other) noexcept
        : ledger_(std::exchange(other.ledger_, nullptr)),
          bytes_(other.bytes_) {}

    auto operator=(Block&& other) noexcept -> Block& {
      std::swap(ledger_, other.ledger_);
      bytes_ = other.bytes_;
      return *this;
    }

    Block(const Block&) = delete;
    auto operator=(const Block&) -> Block& = delete;

    ~Block() {
      if (ledger_ != nullptr) {
        ++ledger_->freed_;
      }
    }

    auto bytes() const noexcept -> std::uint64_t { return bytes_; }

   private:
    Ledger* ledger_;
    std::uint64_t bytes_;
  };

  using Made = Suballocator<Block>::Made;

  explicit Ledger(std::uint64_t base = 0) noexcept : base_(base) {}

  auto make() {
    return [this](BlockRequest request) -> Made {
      requests_.push_back(request);
      if (failing_) {
        return std::unexpected(
            ProviderError{ProviderErrorCode::resource_exhausted});
      }
      ++created_;
      const std::uint64_t usable =
          request.bytes + (request.dedicated ? request.alignment.bytes() : 0);
      return NativeBlock<Block>{.native = Block{*this, request.bytes},
                                .base = base_,
                                .usable = usable,
                                .reserved = usable};
    };
  }

  void fail() noexcept { failing_ = true; }

  auto created() const noexcept -> std::uint64_t { return created_; }

  auto freed() const noexcept -> std::uint64_t { return freed_; }

  auto live() const noexcept -> std::uint64_t { return created_ - freed_; }

  auto requests() const noexcept -> const std::vector<BlockRequest>& {
    return requests_;
  }

 private:
  std::uint64_t base_;
  std::uint64_t created_{0};
  std::uint64_t freed_{0};
  std::vector<BlockRequest> requests_;
  bool failing_{false};
};

using TestSuballocator = Suballocator<Ledger::Block>;

constexpr std::uint64_t block_size = 1024;
constexpr SuballocatorPolicy policy{.block_size = block_size,
                                    .dedicated_above = block_size / 2};

TEST(FreeRangesTest, TakesFirstFitAndCoalescesOnGive) {
  FreeRanges ranges{0, 100};
  const auto first = ranges.take(10, Alignment::of<1>());
  const auto second = ranges.take(20, Alignment::of<1>());
  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(second.has_value());
  EXPECT_EQ(*first, (ByteRange{.offset = 0, .size = 10}));
  EXPECT_EQ(*second, (ByteRange{.offset = 10, .size = 20}));
  ranges.give(*first);
  EXPECT_EQ(ranges.fragments(), 2U);
  ranges.give(*second);
  EXPECT_TRUE(ranges.unused());
  EXPECT_EQ(ranges.largest(), 100U);
}

TEST(FreeRangesTest, AlignsTheAbsoluteAddress) {
  FreeRanges ranges{0x1010, 0x400};
  const auto range = ranges.take(16, Alignment::of<0x100>());
  ASSERT_TRUE(range.has_value());
  EXPECT_EQ((0x1010 + range->offset) % 0x100, 0U);
  EXPECT_EQ(range->offset, 0xF0U);
  EXPECT_EQ(ranges.fragments(), 2U);
}

TEST(FreeRangesTest, RejectsWhatDoesNotFit) {
  FreeRanges ranges{0, 64};
  EXPECT_FALSE(ranges.take(65, Alignment::of<1>()).has_value());
  EXPECT_FALSE(ranges.take(0, Alignment::of<1>()).has_value());
  EXPECT_TRUE(ranges.take(64, Alignment::of<1>()).has_value());
  EXPECT_FALSE(ranges.take(1, Alignment::of<1>()).has_value());
  EXPECT_EQ(ranges.largest(), 0U);
}

TEST(AlignmentTest, AcceptsOnlyPowersOfTwo) {
  EXPECT_FALSE(Alignment::create(0).has_value());
  EXPECT_FALSE(Alignment::create(24).has_value());
  ASSERT_TRUE(Alignment::create(64).has_value());
  EXPECT_EQ(Alignment::create(64)->up(65), 128U);
  EXPECT_EQ(Alignment::of<16>().up(32), 32U);
}

TEST(SuballocatorTest, SmallAllocationsShareABlock) {
  Ledger ledger;
  TestSuballocator allocator{policy};
  const auto first =
      allocator.allocate(100, Alignment::of<64>(), ledger.make());
  const auto second =
      allocator.allocate(100, Alignment::of<64>(), ledger.make());
  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(second.has_value());
  EXPECT_EQ(first->block, second->block);
  EXPECT_EQ(&first->native.get(), &second->native.get());
  EXPECT_EQ(first->range.offset, 0U);
  EXPECT_EQ(second->range.offset, 128U);
  EXPECT_EQ(ledger.created(), 1U);
  ASSERT_EQ(ledger.requests().size(), 1U);
  EXPECT_FALSE(ledger.requests().front().dedicated);
  EXPECT_EQ(ledger.requests().front().bytes, block_size);
  EXPECT_EQ(allocator.statistics(),
            (SuballocatorStatistics{.blocks_created = 1,
                                    .blocks_live = 1,
                                    .allocations_made = 2,
                                    .allocations_live = 2,
                                    .reserved_bytes = block_size}));
}

TEST(SuballocatorTest, AlignsAgainstTheBlockBase) {
  Ledger ledger{0x1010};
  TestSuballocator allocator{policy};
  const auto placed =
      allocator.allocate(16, Alignment::of<0x100>(), ledger.make());
  ASSERT_TRUE(placed.has_value());
  EXPECT_EQ((0x1010 + placed->range.offset) % 0x100, 0U);
}

TEST(SuballocatorTest, FreedRangesAreReused) {
  Ledger ledger;
  TestSuballocator allocator{policy};
  const auto first = allocator.allocate(200, Alignment::of<1>(), ledger.make());
  ASSERT_TRUE(first.has_value());
  const auto kept = allocator.allocate(200, Alignment::of<1>(), ledger.make());
  ASSERT_TRUE(kept.has_value());
  allocator.release(first->block, first->range);
  const auto again = allocator.allocate(200, Alignment::of<1>(), ledger.make());
  ASSERT_TRUE(again.has_value());
  EXPECT_EQ(again->range, first->range);
  EXPECT_EQ(ledger.created(), 1U);
}

TEST(SuballocatorTest, OversizeRequestsGetTheirOwnBlock) {
  Ledger ledger;
  TestSuballocator allocator{policy};
  const auto small = allocator.allocate(16, Alignment::of<1>(), ledger.make());
  const auto large =
      allocator.allocate(block_size * 3, Alignment::of<1>(), ledger.make());
  ASSERT_TRUE(small.has_value());
  ASSERT_TRUE(large.has_value());
  EXPECT_NE(small->block, large->block);
  ASSERT_EQ(ledger.requests().size(), 2U);
  EXPECT_TRUE(ledger.requests().back().dedicated);
  EXPECT_EQ(ledger.requests().back().bytes, block_size * 3);
  EXPECT_EQ(large->native.get().bytes(), block_size * 3);
  allocator.release(large->block, large->range);
  EXPECT_EQ(ledger.freed(), 1U);
  EXPECT_EQ(allocator.statistics().blocks_live, 1U);
  EXPECT_EQ(allocator.statistics().reserved_bytes, block_size);
}

TEST(SuballocatorTest, RequestsBetweenThresholdAndBlockAreDedicated) {
  Ledger ledger;
  TestSuballocator allocator{policy};
  const auto placed =
      allocator.allocate(block_size / 2 + 1, Alignment::of<1>(), ledger.make());
  ASSERT_TRUE(placed.has_value());
  ASSERT_EQ(ledger.requests().size(), 1U);
  EXPECT_TRUE(ledger.requests().front().dedicated);
}

TEST(SuballocatorTest,
     AFreshBlockThatCannotHoldTheRequestFallsBackToDedicated) {
  Ledger ledger{0x10};
  TestSuballocator allocator{{.block_size = 1024, .dedicated_above = 1024}};
  const auto placed =
      allocator.allocate(1000, Alignment::of<0x100>(), ledger.make());
  ASSERT_TRUE(placed.has_value());
  ASSERT_EQ(ledger.requests().size(), 2U);
  EXPECT_FALSE(ledger.requests().front().dedicated);
  EXPECT_TRUE(ledger.requests().back().dedicated);
}

TEST(SuballocatorTest, EmptySharedBlocksAreReleasedWhileASpareRemains) {
  Ledger ledger;
  TestSuballocator allocator{policy};
  std::vector<TestSuballocator::Placement> placed;
  for (int index = 0; index < 4; ++index) {
    auto each =
        allocator.allocate(block_size / 2, Alignment::of<1>(), ledger.make());
    ASSERT_TRUE(each.has_value());
    placed.push_back(*each);
  }
  ASSERT_EQ(ledger.live(), 2U);
  allocator.release(placed[0].block, placed[0].range);
  allocator.release(placed[1].block, placed[1].range);
  EXPECT_EQ(ledger.live(), 2U);
  EXPECT_EQ(allocator.statistics().blocks_live, 2U);
  allocator.release(placed[2].block, placed[2].range);
  allocator.release(placed[3].block, placed[3].range);
  EXPECT_EQ(ledger.live(), 1U);
  EXPECT_EQ(allocator.statistics().blocks_live, 1U);
  EXPECT_EQ(allocator.statistics().allocations_live, 0U);
  const auto again = allocator.allocate(16, Alignment::of<1>(), ledger.make());
  ASSERT_TRUE(again.has_value());
  EXPECT_EQ(ledger.created(), 2U);
}

TEST(SuballocatorTest, ChurnKeepsBlocksBounded) {
  Ledger ledger;
  TestSuballocator allocator{policy};
  std::mt19937 random{39};
  std::uniform_int_distribution<std::uint64_t> sizes{1, block_size / 2};
  std::uniform_int_distribution<int> alignments{0, 6};
  std::vector<TestSuballocator::Placement> live;
  constexpr std::size_t live_limit = 24;
  std::uint64_t peak = 0;
  for (int round = 0; round < 20000; ++round) {
    if (live.size() >= live_limit || (!live.empty() && random() % 2 == 0)) {
      const std::size_t index = random() % live.size();
      allocator.release(live[index].block, live[index].range);
      live[index] = live.back();
      live.pop_back();
      continue;
    }
    const auto alignment =
        Alignment::create(std::uint64_t{1} << alignments(random));
    ASSERT_TRUE(alignment.has_value());
    auto placed = allocator.allocate(sizes(random), *alignment, ledger.make());
    ASSERT_TRUE(placed.has_value());
    live.push_back(*placed);
    peak = std::max(peak, allocator.statistics().blocks_live);
  }
  EXPECT_LE(peak, 16U);
  EXPECT_EQ(ledger.live(), allocator.statistics().blocks_live);
  for (const TestSuballocator::Placement& each : live) {
    allocator.release(each.block, each.range);
  }
  EXPECT_EQ(allocator.statistics().blocks_live, 1U);
  EXPECT_EQ(allocator.statistics().allocations_live, 0U);
  EXPECT_EQ(allocator.statistics().reserved_bytes, block_size);
  EXPECT_EQ(ledger.live(), 1U);
  EXPECT_LT(ledger.created(), 800U);
}

TEST(SuballocatorTest, FailedBlocksAreReported) {
  Ledger ledger;
  ledger.fail();
  TestSuballocator allocator{policy};
  const auto placed = allocator.allocate(16, Alignment::of<1>(), ledger.make());
  ASSERT_FALSE(placed.has_value());
  EXPECT_EQ(placed.error().code(), ProviderErrorCode::resource_exhausted);
  EXPECT_EQ(allocator.statistics(), SuballocatorStatistics{});
}

TEST(SuballocatorTest, ZeroSizedRequestsAreRejected) {
  Ledger ledger;
  TestSuballocator allocator{policy};
  const auto placed = allocator.allocate(0, Alignment::of<1>(), ledger.make());
  ASSERT_FALSE(placed.has_value());
  EXPECT_EQ(placed.error().code(), ProviderErrorCode::invalid_argument);
  EXPECT_TRUE(ledger.requests().empty());
}

}
