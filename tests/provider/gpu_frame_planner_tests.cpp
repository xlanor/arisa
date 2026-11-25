#include <arisa/provider/gpu/frame_planner.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <variant>
#include <vector>

namespace arisa::provider::gpu {

class TraceExecutor {
 public:
  explicit TraceExecutor(std::span<const FramePlanner::Step> steps) {
    for (const FramePlanner::Step& step : steps) {
      std::visit(*this, step);
    }
  }

  void operator()(const FramePlanner::ClearTexture& step) {
    trace_.push_back(std::format("clear {}", step.texture.value()));
  }

  void operator()(const FramePlanner::RenderSurfaces&) {
    trace_.emplace_back("surfaces");
  }

  void operator()(const FramePlanner::OffscreenPass& step) {
    trace_.push_back(std::format("pass {} into {}{}", step.pass,
                                 step.target.value(),
                                 step.clear.has_value() ? " cleared" : ""));
    segments(step.segments);
  }

  void operator()(const FramePlanner::SkippedPass& step) {
    trace_.push_back(std::format("skip {}", step.pass));
  }

  void operator()(const FramePlanner::ScreenPass& step) {
    trace_.push_back(std::format(
        "screen {} {},{} {}x{}{}", step.whole ? "whole" : "partial",
        step.region.x, step.region.y, step.region.width, step.region.height,
        step.scissor.has_value() ? " scissored" : ""));
    segments(step.segments);
  }

  auto trace() const -> const std::vector<std::string>& { return trace_; }

 private:
  void segments(std::span<const FramePlanner::Segment> parts) {
    for (const FramePlanner::Segment& part : parts) {
      std::visit(
          [this](const auto& each) {
            using Part = std::remove_cvref_t<decltype(each)>;
            if constexpr (std::is_same_v<Part, FramePlanner::DrawRange>) {
              trace_.push_back(std::format("  draw {}:{}+{}", each.batch,
                                           each.first, each.count));
            } else {
              trace_.push_back(std::format(
                  "  fill {} {},{} {}x{}", each.underlay.surface.value(),
                  each.scissor.x, each.scissor.y, each.scissor.width,
                  each.scissor.height));
            }
          },
          part);
    }
  }

  std::vector<std::string> trace_;
};

class FramePlannerTest : public ::testing::Test {
 protected:
  using Trace = std::vector<std::string>;

  static constexpr TextureId texture{7};
  static constexpr TextureDescription square{
      .width = 8, .height = 8, .usage = TextureUsage::render_target};
  static constexpr SurfaceSize target{.width = SurfaceWidth{8},
                                      .height = SurfaceHeight{8}};
  static constexpr SurfaceSize screen_size{.width = SurfaceWidth{100},
                                           .height = SurfaceHeight{50}};

  static auto pass(
      std::optional<Colour> clear = std::nullopt,
      PassRegion region = {.x = 0.0F, .y = 0.0F, .width = 8.0F, .height = 8.0F})
      -> FramePlanner::RecordedPass {
    return FramePlanner::RecordedPass{
        .pass = RenderPass{.target = texture, .region = region, .clear = clear},
        .target = target,
        .batch = FramePlanner::Batch{.commands = 2, .holes = {}}};
  }

  auto run(std::span<const FramePlanner::RecordedPass> passes = {}) -> Trace {
    return TraceExecutor{planner_.plan(FramePlanner::RecordedFrame{
                             .passes = passes, .screen = {}})}
        .trace();
  }

  auto present(std::size_t image, std::size_t images,
               std::optional<ClipRect> damage, bool whole = false)
      -> std::string {
    const std::array<FramePlanner::Batch, 1> batches{
        FramePlanner::Batch{.commands = 1, .holes = {}}};
    const Trace trace =
        TraceExecutor{
            planner_.plan(FramePlanner::RecordedFrame{
                .passes = {},
                .screen = FramePlanner::RecordedScreen{.image = image,
                                                       .images = images,
                                                       .size = screen_size,
                                                       .damage = damage,
                                                       .whole = whole,
                                                       .batches = batches}})}
            .trace();
    return trace.at(1);
  }

  static auto rect(std::uint32_t x, std::uint32_t y, std::uint32_t width,
                   std::uint32_t height) -> ClipRect {
    return ClipRect{.x = x, .y = y, .width = width, .height = height};
  }

  FramePlanner planner_;
};

TEST_F(FramePlannerTest, PartialUploadClearsFirstAndOnlyOnce) {
  planner_.created(texture, square);
  const auto first = planner_.upload(
      texture, TextureRegion{.x = 0, .y = 0, .width = 4, .height = 8});
  ASSERT_TRUE(first.has_value());
  EXPECT_EQ(first->texture, texture);
  EXPECT_FALSE(
      planner_
          .upload(texture,
                  TextureRegion{.x = 4, .y = 0, .width = 4, .height = 8})
          .has_value());
  planner_.seal();
  EXPECT_EQ(run(), Trace{"surfaces"});
}

TEST_F(FramePlannerTest, FullUploadNeedsNoClear) {
  planner_.created(texture, square);
  EXPECT_FALSE(
      planner_
          .upload(texture,
                  TextureRegion{.x = 0, .y = 0, .width = 8, .height = 8})
          .has_value());
  planner_.seal();
  EXPECT_EQ(run(), Trace{"surfaces"});
  EXPECT_EQ(planner_.unwritten(), 0U);
}

TEST_F(FramePlannerTest, UnknownTextureUploadNeedsNoClear) {
  EXPECT_FALSE(
      planner_
          .upload(texture,
                  TextureRegion{.x = 0, .y = 0, .width = 1, .height = 1})
          .has_value());
}

TEST_F(FramePlannerTest, UnwrittenSampledTextureIsClearedBeforeAnyWork) {
  planner_.created(texture, square);
  planner_.seal();
  const std::array passes{pass()};
  EXPECT_EQ(run(passes),
            (Trace{"clear 7", "surfaces", "pass 0 into 7", "  draw 0:0+2"}));
  EXPECT_EQ(run(passes), (Trace{"surfaces", "pass 0 into 7", "  draw 0:0+2"}));
}

TEST_F(FramePlannerTest, TextureNobodyDrawsIsStillClearedNextFrame) {
  planner_.created(texture, square);
  planner_.seal();
  EXPECT_EQ(run(), (Trace{"clear 7", "surfaces"}));
}

TEST_F(FramePlannerTest, SelfClearingPassOmitsTheClear) {
  planner_.created(texture, square);
  planner_.seal();
  const std::array passes{pass(Colour{})};
  EXPECT_EQ(run(passes),
            (Trace{"surfaces", "pass 0 into 7 cleared", "  draw 0:0+2"}));
}

TEST_F(FramePlannerTest, SelfClearingPassAfterAPartialPassStillClears) {
  planner_.created(texture, square);
  planner_.seal();
  const std::array passes{pass(), pass(Colour{})};
  EXPECT_EQ(run(passes).front(), "clear 7");
}

TEST_F(FramePlannerTest, SkippedSelfClearingPassDoesNotCountAsAWrite) {
  planner_.created(texture, square);
  planner_.seal();
  const std::array passes{
      pass(Colour{},
           PassRegion{.x = 0.0F, .y = 0.0F, .width = 0.0F, .height = 8.0F})};
  EXPECT_EQ(run(passes), (Trace{"clear 7", "surfaces", "skip 0"}));
}

TEST_F(FramePlannerTest, EmptyPassesAndMissingTargetsAreSkipped) {
  FramePlanner::RecordedPass missing = pass();
  missing.target.reset();
  const std::array passes{
      pass(std::nullopt,
           PassRegion{.x = 0.0F, .y = 0.0F, .width = 8.0F, .height = 0.0F}),
      missing, pass()};
  EXPECT_EQ(run(passes), (Trace{"surfaces", "skip 0", "skip 1", "pass 2 into 7",
                                "  draw 0:0+2"}));
}

TEST_F(FramePlannerTest, TextureCreatedAfterSealWaitsForTheNextFrame) {
  planner_.seal();
  planner_.created(texture, square);
  EXPECT_EQ(run(), Trace{"surfaces"});
  planner_.seal();
  EXPECT_EQ(run(), (Trace{"clear 7", "surfaces"}));
}

TEST_F(FramePlannerTest, PartialUploadAfterSealClearsBeforeItAndNotAfter) {
  planner_.created(texture, square);
  planner_.seal();
  const auto cleared = planner_.upload(
      texture, TextureRegion{.x = 0, .y = 0, .width = 2, .height = 2});
  ASSERT_TRUE(cleared.has_value());
  EXPECT_EQ(cleared->texture, texture);
  EXPECT_EQ(run(), Trace{"surfaces"});
}

TEST_F(FramePlannerTest, FullUploadAfterSealCancelsTheSealedClear) {
  planner_.created(texture, square);
  planner_.seal();
  EXPECT_FALSE(
      planner_
          .upload(texture,
                  TextureRegion{.x = 0, .y = 0, .width = 8, .height = 8})
          .has_value());
  EXPECT_EQ(run(), Trace{"surfaces"});
  EXPECT_EQ(planner_.unwritten(), 0U);
}

TEST_F(FramePlannerTest, OwedClearsIgnoreSelfClearingPasses) {
  planner_.created(texture, square);
  planner_.seal();
  EXPECT_EQ(TraceExecutor{planner_.owed()}.trace(), Trace{"clear 7"});
  EXPECT_EQ(run(), Trace{"surfaces"});
}

TEST_F(FramePlannerTest, OffscreenUnderlaysMapThroughThePassRegion) {
  const std::array holes{
      FramePlanner::Hole{1, SurfaceUnderlay{.surface = SurfaceId{3},
                                            .viewport = {},
                                            .scissor = rect(10, 10, 10, 10)}}};
  FramePlanner::RecordedPass scaled =
      pass(std::nullopt,
           PassRegion{.x = 10.0F, .y = 10.0F, .width = 16.0F, .height = 16.0F});
  scaled.batch = FramePlanner::Batch{.commands = 3, .holes = holes};
  const std::array passes{scaled};
  EXPECT_EQ(run(passes), (Trace{"surfaces", "pass 0 into 7", "  draw 0:0+1",
                                "  fill 3 0,0 5x5", "  draw 0:2+1"}));
}

TEST_F(FramePlannerTest, OffscreenUnderlayOutsideTheRegionIsDropped) {
  const std::array holes{
      FramePlanner::Hole{0, SurfaceUnderlay{.surface = SurfaceId{3},
                                            .viewport = {},
                                            .scissor = rect(40, 40, 4, 4)}}};
  FramePlanner::RecordedPass outside = pass();
  outside.batch = FramePlanner::Batch{.commands = 1, .holes = holes};
  const std::array passes{outside};
  EXPECT_EQ(run(passes), (Trace{"surfaces", "pass 0 into 7"}));
}

TEST_F(FramePlannerTest, ScreenUnderlaysAreClippedToTheDamage) {
  const std::array first_holes{
      FramePlanner::Hole{1, SurfaceUnderlay{.surface = SurfaceId{4},
                                            .viewport = {},
                                            .scissor = rect(0, 0, 30, 30)}}};
  const std::array second_holes{
      FramePlanner::Hole{0, SurfaceUnderlay{.surface = SurfaceId{5},
                                            .viewport = {},
                                            .scissor = rect(80, 40, 10, 10)}}};
  const std::array batches{
      FramePlanner::Batch{.commands = 2, .holes = first_holes},
      FramePlanner::Batch{.commands = 1, .holes = second_holes}};
  const FramePlanner::RecordedScreen screen{.image = 0,
                                            .images = 1,
                                            .size = screen_size,
                                            .damage = rect(10, 10, 40, 10),
                                            .whole = false,
                                            .batches = batches};
  static_cast<void>(planner_.plan(
      FramePlanner::RecordedFrame{.passes = {}, .screen = screen}));
  EXPECT_EQ(TraceExecutor{planner_.plan(FramePlanner::RecordedFrame{
                              .passes = {}, .screen = screen})}
                .trace(),
            (Trace{"surfaces", "screen partial 10,10 40x10 scissored",
                   "  draw 0:0+1", "  fill 4 10,10 20x10"}));
}

TEST_F(FramePlannerTest, UnpaintedScreenHasNoSegments) {
  present(0, 1, std::nullopt);
  EXPECT_EQ(present(0, 1, rect(0, 0, 0, 0)),
            "screen partial 0,0 0x0 scissored");
}

TEST_F(FramePlannerTest, DamageAccumulatesPerImageAcrossTwoImages) {
  EXPECT_EQ(present(0, 2, rect(0, 0, 10, 10)), "screen whole 0,0 100x50");
  EXPECT_EQ(present(1, 2, rect(20, 0, 10, 10)), "screen whole 0,0 100x50");
  EXPECT_EQ(present(0, 2, rect(40, 0, 10, 10)),
            "screen partial 20,0 30x10 scissored");
  EXPECT_EQ(present(1, 2, rect(40, 20, 10, 10)),
            "screen partial 40,0 10x30 scissored");
}

TEST_F(FramePlannerTest, DamageAccumulatesPerImageAcrossThreeImages) {
  EXPECT_EQ(present(0, 3, rect(0, 0, 10, 10)), "screen whole 0,0 100x50");
  EXPECT_EQ(present(1, 3, rect(10, 0, 10, 10)), "screen whole 0,0 100x50");
  EXPECT_EQ(present(2, 3, rect(20, 0, 10, 10)), "screen whole 0,0 100x50");
  EXPECT_EQ(present(0, 3, rect(30, 0, 10, 10)),
            "screen partial 10,0 30x10 scissored");
  EXPECT_EQ(present(1, 3, rect(0, 40, 5, 5)),
            "screen partial 0,0 40x45 scissored");
}

TEST_F(FramePlannerTest, DamageIsClampedToTheScreen) {
  present(0, 1, std::nullopt);
  EXPECT_EQ(present(0, 1, rect(90, 40, 50, 50)),
            "screen partial 90,40 10x10 scissored");
}

TEST_F(FramePlannerTest, UndamagedFrameRepaintsEveryImage) {
  present(0, 2, std::nullopt);
  present(1, 2, std::nullopt);
  present(0, 2, rect(0, 0, 1, 1));
  EXPECT_EQ(present(1, 2, std::nullopt), "screen whole 0,0 100x50");
  EXPECT_EQ(present(0, 2, rect(0, 0, 1, 1)), "screen whole 0,0 100x50");
}

TEST_F(FramePlannerTest, ForcedAndInvalidatedFramesRepaintWhole) {
  present(0, 1, std::nullopt);
  EXPECT_EQ(present(0, 1, rect(0, 0, 1, 1), true), "screen whole 0,0 100x50");
  planner_.invalidate();
  EXPECT_EQ(present(0, 1, rect(0, 0, 1, 1)), "screen whole 0,0 100x50");
  EXPECT_EQ(present(0, 1, rect(0, 0, 1, 1)),
            "screen partial 0,0 1x1 scissored");
}

TEST_F(FramePlannerTest, ChangingTheImageCountRepaintsWhole) {
  present(0, 2, std::nullopt);
  present(1, 2, std::nullopt);
  EXPECT_EQ(present(0, 3, rect(0, 0, 1, 1)), "screen whole 0,0 100x50");
}

}
