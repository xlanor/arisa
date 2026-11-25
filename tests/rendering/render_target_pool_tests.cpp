#include <arisa/provider/headless/null_gpu.hpp>
#include <arisa/rendering/render_target_pool.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <utility>
#include <vector>

namespace arisa::rendering {

TEST(RenderTargetPoolTest, ReusesAReleasedTargetOfTheSameSize) {
  provider::headless::NullGpu gpu;
  RenderTargetPool pool{gpu};
  auto first = pool.acquire(30, 20);
  ASSERT_TRUE(first.has_value());
  EXPECT_EQ(first->width(), 32U);
  EXPECT_EQ(first->height(), 24U);
  const auto id = first->id();
  *first = RenderTargetPool::Lease{};
  pool.advance_frame();
  auto second = pool.acquire(31, 17);
  ASSERT_TRUE(second.has_value());
  EXPECT_EQ(second->id(), id);
  EXPECT_EQ(pool.statistics().created, 1U);
  EXPECT_EQ(pool.statistics().reused, 1U);
  EXPECT_EQ(gpu.render_targets(), 1U);
}

TEST(RenderTargetPoolTest, ALayerLeavesAFullScreenScratchTargetForItsUser) {
  provider::headless::NullGpu gpu;
  RenderTargetPool pool{gpu};
  std::vector<RenderTargetPool::Lease> layers;
  std::size_t recreated = 0;
  for (int frame = 0; frame < 20; ++frame) {
    const std::size_t before = pool.statistics().created;
    auto scratch = pool.acquire(1920, 1080);
    ASSERT_TRUE(scratch.has_value());
    recreated += pool.statistics().created - before;
    pool.recycle(std::move(*scratch));
    auto layer = pool.acquire(200, 120, RenderTargetPool::Fit::at_least);
    ASSERT_TRUE(layer.has_value());
    EXPECT_LT(layer->width(), 1920U);
    layers.push_back(std::move(*layer));
    pool.advance_frame();
  }
  EXPECT_EQ(recreated, 1U) << recreated << " full-screen targets for 20 frames";
}

TEST(RenderTargetPoolTest, LeasedTargetsAreNeverShared) {
  provider::headless::NullGpu gpu;
  RenderTargetPool pool{gpu};
  auto first = pool.acquire(16, 16);
  auto second = pool.acquire(16, 16);
  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(second.has_value());
  EXPECT_NE(first->id(), second->id());
  EXPECT_EQ(pool.statistics().leased, 2U);
  RenderTargetPool::Lease moved = std::move(*second);
  EXPECT_TRUE(moved.valid());
  EXPECT_EQ(pool.statistics().leased, 2U);
}

TEST(RenderTargetPoolTest, ReleasesTargetsLeftIdlePastTheGracePeriod) {
  provider::headless::NullGpu gpu;
  RenderTargetPool pool{gpu};
  const std::size_t before = gpu.live_textures();
  {
    auto lease = pool.acquire(64, 64);
    ASSERT_TRUE(lease.has_value());
    EXPECT_EQ(gpu.live_textures(), before + 1);
  }
  EXPECT_EQ(pool.statistics().idle, 0U);
  EXPECT_EQ(pool.statistics().returning, 1U);
  for (std::uint64_t frame = 0; frame <= RenderTargetPool::grace_frames;
       ++frame) {
    pool.advance_frame();
  }
  EXPECT_EQ(pool.statistics().idle, 0U);
  EXPECT_EQ(pool.statistics().released, 1U);
  EXPECT_EQ(gpu.live_textures(), before);
}

TEST(RenderTargetPoolTest, ATargetGivenBackIsNotReusedInTheSameFrame) {
  provider::headless::NullGpu gpu;
  RenderTargetPool pool{gpu};
  auto first = pool.acquire(64, 32);
  ASSERT_TRUE(first.has_value());
  const auto sampled = first->id();
  *first = RenderTargetPool::Lease{};
  auto second = pool.acquire(64, 32);
  ASSERT_TRUE(second.has_value());
  EXPECT_NE(second->id(), sampled);
  EXPECT_EQ(pool.statistics().reused, 0U);
  *second = RenderTargetPool::Lease{};
  pool.advance_frame();
  auto third = pool.acquire(64, 32);
  ASSERT_TRUE(third.has_value());
  EXPECT_EQ(pool.statistics().reused, 1U);
}

TEST(RenderTargetPoolTest, ARecycledScratchTargetIsReusedInTheSameFrame) {
  provider::headless::NullGpu gpu;
  RenderTargetPool pool{gpu};
  auto first = pool.acquire(16, 16);
  ASSERT_TRUE(first.has_value());
  const auto scratch = first->id();
  pool.recycle(std::move(*first));
  auto second = pool.acquire(16, 16);
  ASSERT_TRUE(second.has_value());
  EXPECT_EQ(second->id(), scratch);
  EXPECT_EQ(pool.statistics().reused, 1U);
}

TEST(RenderTargetPoolTest, AFailedCreateKeepsTargetsGivenBackThisFrame) {
  provider::headless::NullGpu gpu;
  gpu.limit_render_targets(
      provider::headless::NullGpu::RenderTargetLimit{.count = 1});
  RenderTargetPool pool{gpu};
  auto first = pool.acquire(64, 64);
  ASSERT_TRUE(first.has_value());
  const std::size_t live = gpu.live_textures();
  *first = RenderTargetPool::Lease{};
  auto second = pool.acquire(32, 32);
  EXPECT_FALSE(second.has_value());
  EXPECT_EQ(gpu.live_textures(), live);
  EXPECT_EQ(pool.statistics().refused, 1U);
  EXPECT_EQ(pool.statistics().released, 0U);
  pool.advance_frame();
  auto third = pool.acquire(32, 32);
  ASSERT_TRUE(third.has_value());
  EXPECT_EQ(pool.statistics().evicted, 1U);
  EXPECT_EQ(pool.statistics().released, 1U);
  EXPECT_EQ(pool.statistics().created, 2U);
}

TEST(RenderTargetPoolTest, AnEmptyLeaseHasNoTexture) {
  const RenderTargetPool::Lease empty;
  EXPECT_FALSE(empty.valid());
  EXPECT_FALSE(empty.id().has_value());
}

}
