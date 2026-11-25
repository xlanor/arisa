#include <arisa/provider/gpu/upload_arena.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <utility>
#include <vector>

namespace arisa::provider::gpu {

class StagingChunk {
 public:
  struct Flushed {
    std::uint64_t offset{0};
    std::uint64_t length{0};
  };

  StagingChunk(std::uint64_t size, std::vector<Flushed>& flushed)
      : storage_(std::make_unique<std::byte[]>(size)),
        size_(size),
        flushed_(&flushed) {}

  auto bytes() const noexcept -> std::span<std::byte> {
    return {storage_.get(), size_};
  }

  void flush(std::uint64_t offset, std::uint64_t length) const noexcept {
    flushed_->push_back(Flushed{.offset = offset, .length = length});
  }

 private:
  std::unique_ptr<std::byte[]> storage_;
  std::uint64_t size_;
  std::vector<Flushed>* flushed_;
};

class UploadArenaTest : public ::testing::Test {
 protected:
  using Arena = UploadArena<StagingChunk>;

  static constexpr std::uint64_t minimum = 256;

  auto make() {
    return [this](std::uint64_t size) -> Arena::Made {
      requested_.push_back(size);
      if (failing_) {
        return std::unexpected(
            ProviderError{ProviderErrorCode::resource_exhausted});
      }
      return StagingChunk{size, flushed_};
    };
  }

  Arena arena_{Alignment::of<16>(), minimum};
  std::vector<std::uint64_t> requested_;
  std::vector<StagingChunk::Flushed> flushed_;
  bool failing_{false};
};

TEST_F(UploadArenaTest, TakesAlignedSpacesFromOneChunk) {
  const auto first = arena_.take(10, make());
  const auto second = arena_.take(20, make());
  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(second.has_value());
  EXPECT_EQ(first->offset, 0U);
  EXPECT_EQ(first->bytes.size(), 10U);
  EXPECT_EQ(second->offset, 16U);
  EXPECT_EQ(second->bytes.size(), 20U);
  EXPECT_EQ(&first->chunk.get(), &second->chunk.get());
  EXPECT_EQ(second->bytes.data(), first->bytes.data() + 16);
  ASSERT_EQ(requested_.size(), 1U);
  EXPECT_EQ(requested_.front(), minimum);
  EXPECT_EQ(arena_.used(), 48U);
}

TEST_F(UploadArenaTest, ResetReusesTheSameMemory) {
  const auto before = arena_.take(100, make());
  ASSERT_TRUE(before.has_value());
  std::byte* const start = before->bytes.data();
  arena_.reset();
  EXPECT_EQ(arena_.used(), 0U);
  const auto after = arena_.take(100, make());
  ASSERT_TRUE(after.has_value());
  EXPECT_EQ(after->bytes.data(), start);
  EXPECT_EQ(requested_.size(), 1U);
}

TEST_F(UploadArenaTest, GrowsByDoublingAndResetKeepsTheLargestChunk) {
  ASSERT_TRUE(arena_.take(200, make()).has_value());
  ASSERT_TRUE(arena_.take(200, make()).has_value());
  ASSERT_TRUE(arena_.take(400, make()).has_value());
  ASSERT_EQ(requested_.size(), 3U);
  EXPECT_EQ(requested_[1], 2 * minimum);
  EXPECT_EQ(requested_[2], 4 * minimum);
  EXPECT_EQ(arena_.chunk_count(), 3U);
  arena_.reset();
  EXPECT_EQ(arena_.chunk_count(), 1U);
  EXPECT_EQ(arena_.capacity(), 4 * minimum);
  for (int frame = 0; frame < 8; ++frame) {
    ASSERT_TRUE(arena_.take(400, make()).has_value());
    ASSERT_TRUE(arena_.take(400, make()).has_value());
    arena_.reset();
  }
  EXPECT_EQ(requested_.size(), 3U);
}

TEST_F(UploadArenaTest, OversizeRequestsGetAChunkThatFits) {
  const auto space = arena_.take(minimum * 10 + 3, make());
  ASSERT_TRUE(space.has_value());
  EXPECT_EQ(space->offset, 0U);
  EXPECT_EQ(space->bytes.size(), minimum * 10 + 3);
  ASSERT_EQ(requested_.size(), 1U);
  EXPECT_EQ(requested_.front(), minimum * 10 + 16);
}

TEST_F(UploadArenaTest, FlushCoversWhatEachChunkUsed) {
  ASSERT_TRUE(arena_.take(40, make()).has_value());
  ASSERT_TRUE(arena_.take(minimum, make()).has_value());
  arena_.flush();
  ASSERT_EQ(flushed_.size(), 2U);
  EXPECT_EQ(flushed_[0].offset, 0U);
  EXPECT_EQ(flushed_[0].length, 48U);
  EXPECT_EQ(flushed_[1].length, minimum);
}

TEST_F(UploadArenaTest, ChunkFailuresAreReported) {
  failing_ = true;
  const auto space = arena_.take(16, make());
  ASSERT_FALSE(space.has_value());
  EXPECT_EQ(space.error().code(), ProviderErrorCode::resource_exhausted);
  EXPECT_EQ(arena_.chunk_count(), 0U);
}

}
