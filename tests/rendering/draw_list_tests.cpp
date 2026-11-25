#include <layout_fixture.hpp>

#include <arisa/components/vstack.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/provider/gpu/draw_data.hpp>
#include <arisa/provider/headless/null_gpu.hpp>
#include <arisa/rendering/draw_list_cache.hpp>
#include <arisa/rendering/painter.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <utility>

namespace arisa::rendering {

class DrawListTest : public ::testing::Test, protected layout::LayoutFixture {
 protected:
  using Key = DrawListCache::Key;
  using Bounds = DrawListCache::Bounds;

  static auto run(std::uint64_t owner, std::uint64_t content,
                  std::uint32_t count = 4) -> DrawListCache::Run {
    return DrawListCache::Run{.key = Key{.owner = owner, .ordinal = 0},
                              .content = content,
                              .order = 0,
                              .count = count,
                              .fresh = false};
  }

  static auto extent(Bounds drawn, Bounds sense = {}) -> DrawListCache::Extent {
    return DrawListCache::Extent{
        .drawn = drawn,
        .local = drawn,
        .clip =
            Bounds{
                .left = 0.0F, .top = 0.0F, .right = 1000.0F, .bottom = 1000.0F},
        .sense = sense};
  }

  static auto box_at(float left, float top) -> Bounds {
    return Bounds{
        .left = left, .top = top, .right = left + 10.0F, .bottom = top + 10.0F};
  }

  static auto paint(provider::headless::NullGpu& gpu, Painter& painter,
                    composition::Tree& target, Painter::Highlights highlights)
      -> Painter::Statistics {
    auto context = layout::LayoutContext::create();
    EXPECT_TRUE(context.has_value());
    EXPECT_TRUE(context->update(target, handheld, std::nullopt).has_value());
    auto frame = gpu.begin_frame();
    EXPECT_TRUE(frame.has_value());
    auto statistics =
        painter.paint(target, *frame,
                      Painter::Target{.pixels = handheld.pixels,
                                      .scale = handheld.scale,
                                      .time = std::chrono::nanoseconds{0}},
                      std::nullopt, highlights);
    EXPECT_TRUE(statistics.has_value());
    EXPECT_TRUE(frame->present().has_value());
    return statistics.value_or(Painter::Statistics{});
  }
};

TEST_F(DrawListTest, UnchangedRunsKeepTheirRangeAndNeedNoWrite) {
  DrawListCache cache;
  cache.begin(true);
  const std::size_t first = cache.place(run(1, 10));
  const std::size_t second = cache.place(run(2, 20, 8));
  EXPECT_TRUE(cache.adopt(1));
  cache.bound(first, extent(box_at(0.0F, 0.0F)));
  cache.bound(second, extent(box_at(50.0F, 0.0F)));
  EXPECT_FALSE(cache.finish().has_value());
  EXPECT_EQ(cache.extent(), 12U);
  cache.begin(false);
  const std::size_t again = cache.place(run(1, 10));
  const std::size_t moved = cache.place(run(2, 20, 8));
  EXPECT_FALSE(cache.adopt(1));
  EXPECT_FALSE(cache.placement(again).write);
  EXPECT_FALSE(cache.placement(moved).write);
  EXPECT_EQ(cache.placement(moved).first, 4U);
  const std::optional<Bounds> damage = cache.finish();
  ASSERT_TRUE(damage.has_value());
  EXPECT_TRUE(damage->empty());
}

TEST_F(DrawListTest, ChangedRunDamagesItsOldAndNewBoundsOnly) {
  DrawListCache cache;
  cache.begin(true);
  cache.bound(cache.place(run(1, 10)), extent(box_at(0.0F, 0.0F)));
  cache.bound(cache.place(run(2, 20)), extent(box_at(100.0F, 100.0F)));
  static_cast<void>(cache.adopt(1));
  static_cast<void>(cache.finish());
  cache.begin(false);
  const std::size_t still = cache.place(run(1, 10));
  const std::size_t changed = cache.place(run(2, 21));
  static_cast<void>(cache.adopt(1));
  EXPECT_FALSE(cache.placement(still).write);
  EXPECT_TRUE(cache.placement(changed).write);
  cache.bound(changed, extent(box_at(120.0F, 100.0F)));
  const std::optional<Bounds> damage = cache.finish();
  ASSERT_TRUE(damage.has_value());
  EXPECT_EQ(
      *damage,
      (Bounds{
          .left = 100.0F, .top = 100.0F, .right = 130.0F, .bottom = 110.0F}));
}

TEST_F(DrawListTest, MovedRunReplaysWithoutAWriteAndDamagesBothPlaces) {
  DrawListCache cache;
  cache.begin(true);
  cache.bound(cache.place(run(1, 10)), extent(box_at(0.0F, 0.0F)));
  cache.bound(cache.place(run(2, 20)), extent(box_at(100.0F, 100.0F)));
  static_cast<void>(cache.adopt(1));
  static_cast<void>(cache.finish());
  cache.begin(false);
  static_cast<void>(cache.place(run(1, 10)));
  DrawListCache::Run shifted = run(2, 20);
  shifted.offset = {0.0F, -30.0F};
  const std::size_t moved = cache.place(shifted);
  static_cast<void>(cache.adopt(1));
  EXPECT_FALSE(cache.placement(moved).write);
  const std::optional<Bounds> damage = cache.finish();
  ASSERT_TRUE(damage.has_value());
  EXPECT_EQ(
      *damage,
      (Bounds{
          .left = 100.0F, .top = 70.0F, .right = 110.0F, .bottom = 110.0F}));
}

TEST_F(DrawListTest, DepartedRunDamagesWhereItWasAndFreesItsRange) {
  DrawListCache cache;
  cache.begin(true);
  cache.bound(cache.place(run(1, 10)), extent(box_at(0.0F, 0.0F)));
  cache.bound(cache.place(run(2, 20)), extent(box_at(40.0F, 40.0F)));
  static_cast<void>(cache.adopt(1));
  static_cast<void>(cache.finish());
  cache.begin(false);
  static_cast<void>(cache.place(run(1, 10)));
  static_cast<void>(cache.adopt(1));
  const std::optional<Bounds> damage = cache.finish();
  ASSERT_TRUE(damage.has_value());
  EXPECT_EQ(*damage, box_at(40.0F, 40.0F));
  EXPECT_EQ(cache.extent(), 4U);
  EXPECT_EQ(cache.entries(), 1U);
}

TEST_F(DrawListTest, BackdropReadersInsideTheDamageAreRecomposited) {
  DrawListCache cache;
  cache.begin(true);
  cache.bound(cache.place(run(1, 10)), extent(box_at(0.0F, 0.0F)));
  cache.bound(
      cache.place(run(2, 20)),
      extent(
          Bounds{.left = 20.0F, .top = 0.0F, .right = 60.0F, .bottom = 40.0F},
          Bounds{.left = 5.0F, .top = 0.0F, .right = 75.0F, .bottom = 55.0F}));
  static_cast<void>(cache.adopt(1));
  static_cast<void>(cache.finish());
  cache.begin(false);
  const std::size_t moving = cache.place(run(1, 11));
  static_cast<void>(cache.place(run(2, 20)));
  static_cast<void>(cache.adopt(1));
  cache.bound(moving, extent(box_at(2.0F, 0.0F)));
  const std::optional<Bounds> damage = cache.finish();
  ASSERT_TRUE(damage.has_value());
  EXPECT_EQ(
      *damage,
      (Bounds{.left = 0.0F, .top = 0.0F, .right = 60.0F, .bottom = 40.0F}));
}

TEST_F(DrawListTest, ChangesAboveABackdropReaderLeaveItAlone) {
  const Bounds reader{
      .left = 0.0F, .top = 0.0F, .right = 100.0F, .bottom = 100.0F};
  DrawListCache cache;
  cache.begin(true);
  cache.bound(cache.place(run(1, 10)), extent(reader, reader));
  cache.bound(cache.place(run(2, 20)), extent(box_at(20.0F, 20.0F)));
  static_cast<void>(cache.adopt(1));
  static_cast<void>(cache.finish());
  cache.begin(false);
  static_cast<void>(cache.place(run(1, 10)));
  const std::size_t above = cache.place(run(2, 21));
  static_cast<void>(cache.adopt(1));
  cache.bound(above, extent(box_at(20.0F, 20.0F)));
  const std::optional<Bounds> damage = cache.finish();
  ASSERT_TRUE(damage.has_value());
  EXPECT_EQ(*damage, box_at(20.0F, 20.0F));
}

TEST_F(DrawListTest, ADepartureAboveABackdropReaderLeavesItAlone) {
  const Bounds reader{
      .left = 0.0F, .top = 0.0F, .right = 100.0F, .bottom = 100.0F};
  DrawListCache cache;
  cache.begin(true);
  cache.bound(cache.place(run(1, 10)), extent(reader, reader));
  cache.bound(cache.place(run(2, 20)), extent(box_at(20.0F, 20.0F)));
  static_cast<void>(cache.adopt(1));
  static_cast<void>(cache.finish());
  cache.begin(false);
  static_cast<void>(cache.place(run(1, 10)));
  static_cast<void>(cache.adopt(1));
  const std::optional<Bounds> damage = cache.finish();
  ASSERT_TRUE(damage.has_value());
  EXPECT_EQ(*damage, box_at(20.0F, 20.0F));
}

TEST_F(DrawListTest, ADepartureBeneathABackdropReaderRecompositesIt) {
  const Bounds reader{
      .left = 0.0F, .top = 0.0F, .right = 100.0F, .bottom = 100.0F};
  DrawListCache cache;
  cache.begin(true);
  cache.bound(cache.place(run(1, 10)), extent(box_at(20.0F, 20.0F)));
  cache.bound(cache.place(run(2, 20)), extent(reader, reader));
  static_cast<void>(cache.adopt(1));
  static_cast<void>(cache.finish());
  cache.begin(false);
  static_cast<void>(cache.place(run(2, 20)));
  static_cast<void>(cache.adopt(1));
  const std::optional<Bounds> damage = cache.finish();
  ASSERT_TRUE(damage.has_value());
  EXPECT_EQ(*damage, reader);
}

TEST_F(DrawListTest, ARunInsertedBeforeAnotherLeavesItUndamaged) {
  DrawListCache cache;
  cache.begin(true);
  cache.bound(cache.place(run(1, 10)), extent(box_at(0.0F, 0.0F)));
  cache.bound(cache.place(run(2, 20)), extent(box_at(100.0F, 100.0F)));
  static_cast<void>(cache.adopt(1));
  static_cast<void>(cache.finish());
  cache.begin(false);
  static_cast<void>(cache.place(run(1, 10)));
  const std::size_t arrived = cache.place(run(3, 30));
  const std::size_t after = cache.place(run(2, 20));
  static_cast<void>(cache.adopt(1));
  cache.bound(arrived, extent(box_at(0.0F, 50.0F)));
  EXPECT_FALSE(cache.placement(after).damaged);
  const std::optional<Bounds> damage = cache.finish();
  ASSERT_TRUE(damage.has_value());
  EXPECT_EQ(*damage, box_at(0.0F, 50.0F));
}

TEST_F(DrawListTest, SwappedRunsAreDamaged) {
  DrawListCache cache;
  cache.begin(true);
  cache.bound(cache.place(run(1, 10)), extent(box_at(0.0F, 0.0F)));
  cache.bound(cache.place(run(2, 20)), extent(box_at(5.0F, 5.0F)));
  static_cast<void>(cache.adopt(1));
  static_cast<void>(cache.finish());
  cache.begin(false);
  const std::size_t first = cache.place(run(2, 20));
  const std::size_t second = cache.place(run(1, 10));
  static_cast<void>(cache.adopt(1));
  EXPECT_FALSE(cache.placement(first).damaged);
  EXPECT_TRUE(cache.placement(second).damaged);
  const std::optional<Bounds> damage = cache.finish();
  ASSERT_TRUE(damage.has_value());
  EXPECT_EQ(*damage, box_at(0.0F, 0.0F));
}

TEST_F(DrawListTest, ANewProviderGenerationRewritesEveryRun) {
  DrawListCache cache;
  cache.begin(true);
  static_cast<void>(cache.place(run(1, 10)));
  static_cast<void>(cache.adopt(1));
  static_cast<void>(cache.finish());
  cache.begin(false);
  const std::size_t placed = cache.place(run(1, 10));
  EXPECT_TRUE(cache.adopt(2));
  EXPECT_TRUE(cache.placement(placed).write);
  static_cast<void>(cache.finish());
  cache.begin(false);
  const std::size_t transient = cache.place(run(1, 10));
  EXPECT_TRUE(cache.adopt(0));
  EXPECT_TRUE(cache.placement(transient).write);
}

TEST_F(DrawListTest, ReleasedRangesAreReused) {
  DrawListCache cache;
  cache.begin(true);
  static_cast<void>(cache.place(run(1, 10)));
  static_cast<void>(cache.place(run(2, 20)));
  static_cast<void>(cache.place(run(3, 30)));
  static_cast<void>(cache.adopt(1));
  static_cast<void>(cache.finish());
  cache.begin(false);
  static_cast<void>(cache.place(run(1, 10)));
  static_cast<void>(cache.place(run(3, 30)));
  static_cast<void>(cache.adopt(1));
  static_cast<void>(cache.finish());
  cache.begin(false);
  static_cast<void>(cache.place(run(1, 10)));
  static_cast<void>(cache.place(run(3, 30)));
  const std::size_t fresh = cache.place(run(4, 40));
  static_cast<void>(cache.adopt(1));
  EXPECT_EQ(cache.placement(fresh).first, 4U);
  EXPECT_EQ(cache.extent(), 12U);
}

TEST_F(DrawListTest, RepaintingAnUnchangedTreeUploadsNoVertices) {
  provider::headless::NullGpu gpu;
  auto created = Painter::create(gpu);
  ASSERT_TRUE(created.has_value());
  Painter painter = std::move(*created);
  const auto layout = tree(components::VStack(
      box("a", 100.0F, 40.0F), box("b", 60.0F, 20.0F), box("c", 80.0F, 30.0F)));
  const Painter::Statistics first = paint(gpu, painter, *layout, {});
  EXPECT_GT(first.uploaded_vertices, 0U);
  EXPECT_FALSE(first.damage.has_value());
  const std::size_t uploads = gpu.vertex_uploads();
  const Painter::Statistics second = paint(gpu, painter, *layout, {});
  EXPECT_EQ(second.uploaded_vertices, 0U);
  EXPECT_EQ(gpu.vertex_uploads(), uploads);
  ASSERT_TRUE(second.damage.has_value());
  EXPECT_EQ(second.damaged_pixels, 0U);
  EXPECT_EQ(second.quads, first.quads);
}

TEST_F(DrawListTest, AFocusRingDamagesOnlyAroundItsNode) {
  provider::headless::NullGpu gpu;
  auto created = Painter::create(gpu);
  ASSERT_TRUE(created.has_value());
  Painter painter = std::move(*created);
  const auto layout = tree(components::VStack(
      box("a", 100.0F, 40.0F), box("b", 60.0F, 20.0F), box("c", 80.0F, 30.0F)));
  const auto focused = layout->find(composition::ElementKey{"b"});
  ASSERT_TRUE(focused.has_value());
  static_cast<void>(paint(gpu, painter, *layout, {}));
  const Painter::Statistics ringed =
      paint(gpu, painter, *layout, Painter::Highlights{.focused = focused});
  ASSERT_TRUE(ringed.damage.has_value());
  const composition::Rect target = frame(*layout, "b");
  constexpr float reach = 8.0F;
  EXPECT_GE(static_cast<float>(ringed.damage->x), target.x - reach);
  EXPECT_GE(static_cast<float>(ringed.damage->y), target.y - reach);
  EXPECT_LE(static_cast<float>(ringed.damage->x + ringed.damage->width),
            target.x + target.width + reach);
  EXPECT_LE(static_cast<float>(ringed.damage->y + ringed.damage->height),
            target.y + target.height + reach);
  EXPECT_GT(ringed.uploaded_vertices, 0U);
  EXPECT_LE(ringed.uploaded_vertices, 8U);
}

}
