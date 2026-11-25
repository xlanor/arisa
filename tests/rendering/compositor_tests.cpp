#include <arisa/provider/gpu/render_pass.hpp>
#include <arisa/provider/headless/null_gpu.hpp>
#include <arisa/rendering/compositor.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <span>

namespace arisa::rendering {

TEST(CompositorTest, MixingAHashWithItselfStillDependsOnTheHash) {
  const std::uint64_t first = 2858422293457412525ULL;
  const std::uint64_t second = 1218071839000008272ULL;
  EXPECT_NE(Compositor::mix(first, std::as_bytes(std::span{&first, 1})),
            Compositor::mix(second, std::as_bytes(std::span{&second, 1})));
}

TEST(CompositorTest, OneKeyAskedForTwoDifferentBackdropsGetsTwoTargets) {
  provider::headless::NullGpu gpu;
  Compositor compositor{gpu};
  compositor.advance_frame();
  const provider::PassRegion small{
      .x = 0.0F, .y = 0.0F, .width = 100.0F, .height = 60.0F};
  const provider::PassRegion large{
      .x = 0.0F, .y = 0.0F, .width = 240.0F, .height = 160.0F};
  const auto first = compositor.backdrop(7, small, small, 12.0F, 0);
  const auto second = compositor.backdrop(7, large, large, 12.0F, 4);
  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(second.has_value());
  EXPECT_NE(first->texture, second->texture);
  EXPECT_EQ(first->key, 7U);
  EXPECT_NE(second->key, 7U);
  EXPECT_TRUE(compositor.predicts(first->key, small, 12.0F, *first));
  EXPECT_TRUE(compositor.predicts(second->key, large, 12.0F, *second));
}

TEST(CompositorTest, ARepeatedIdenticalBackdropSharesItsTarget) {
  provider::headless::NullGpu gpu;
  Compositor compositor{gpu};
  compositor.advance_frame();
  const provider::PassRegion region{
      .x = 10.0F, .y = 10.0F, .width = 100.0F, .height = 60.0F};
  const auto first = compositor.backdrop(9, region, region, 12.0F, 0);
  const auto second = compositor.backdrop(9, region, region, 12.0F, 2);
  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(second.has_value());
  EXPECT_EQ(first->texture, second->texture);
  EXPECT_EQ(second->key, 9U);
  EXPECT_EQ(compositor.pool().statistics().created, 1U);
}

TEST(CompositorTest, AWithdrawnTextureIsListedUntilTheFrameAdvances) {
  provider::headless::NullGpu gpu;
  Compositor compositor{gpu};
  compositor.withdraw(provider::TextureId{5});
  compositor.withdraw(provider::TextureId{5});
  compositor.withdraw(provider::TextureId{0});
  ASSERT_EQ(compositor.withdrawn().size(), 1U);
  EXPECT_EQ(compositor.withdrawn().front(), provider::TextureId{5});
  compositor.advance_frame();
  EXPECT_TRUE(compositor.withdrawn().empty());
}

}
