#include <arisa/provider/headless/headless_factory.hpp>

#include <gallery_fixture.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <print>
#include <string>

namespace arisa::runtime {

class GalleryScrollCostTest
    : public GalleryFixture,
      public ::testing::WithParamInterface<examples::GalleryPage> {
 protected:
  struct Cost {
    int frames{0};
    int presented{0};
    int laid_out{0};
    std::uint64_t evaluations{0};
    std::size_t shaped{0};
    std::uint64_t visited{0};
    std::uint64_t culled{0};
    std::uint64_t shadow_misses{0};
    std::uint64_t quads{0};
    std::chrono::nanoseconds wall{0};
    int spins{0};

    void add(const FrameTimings& timings) {
      ++frames;
      presented += timings.presented ? 1 : 0;
      laid_out += timings.laid_out ? 1 : 0;
      evaluations += timings.evaluations;
      shaped += timings.shaped;
      visited += timings.visited;
      culled += timings.culled;
      shadow_misses += timings.shadow_misses;
      quads += timings.quads;
    }
  };

  static constexpr int blocking_waits = 3;

  static auto stroke(provider::headless::HeadlessProviderFactory& providers,
                     Application& application, provider::PixelPoint from,
                     float distance) -> Cost {
    Cost cost;
    int waits = 0;
    const auto frame = [&](std::chrono::milliseconds step) {
      ASSERT_TRUE(providers.clock().advance(step).has_value());
      const auto started = std::chrono::steady_clock::now();
      ASSERT_TRUE(application.frame().has_value());
      cost.wall += std::chrono::steady_clock::now() - started;
      cost.add(application.last_frame());
      if (application.waiting()) {
        if (++waits > blocking_waits) {
          return;
        }
        const auto paused = std::chrono::steady_clock::now();
        application.wait();
        if (std::chrono::steady_clock::now() - paused <
            std::chrono::milliseconds{1}) {
          ++cost.spins;
        }
      } else if (!application.last_frame().presented) {
        ++cost.spins;
      }
    };
    providers.input().push(touching(from, provider::TouchPhase::began));
    frame(frame_step);
    constexpr int steps = 24;
    for (int index = 1; index <= steps; ++index) {
      providers.input().push(touching(
          provider::PixelPoint{
              .x = from.x,
              .y = from.y - (distance * static_cast<float>(index) /
                             static_cast<float>(steps))},
          provider::TouchPhase::moved));
      frame(frame_step);
    }
    providers.input().push(
        touching(provider::PixelPoint{.x = from.x, .y = from.y - distance},
                 provider::TouchPhase::ended));
    frame(frame_step);
    for (int index = 0; index < 60; ++index) {
      frame(frame_step);
    }
    return cost;
  }
};

TEST_F(GalleryScrollCostTest, LongListRealisesRowsInBatches) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start_gallery(providers, examples::GalleryPage::lists);
  run(providers, application, 60);
  const provider::PixelPoint from{.x = 950.0F, .y = 600.0F};
  for (int round = 0; round < 3; ++round) {
    static_cast<void>(stroke(providers, application, from, 500.0F));
  }
  const Cost cost = stroke(providers, application, from, 500.0F);
  EXPECT_LE(cost.laid_out * 6, cost.frames);
  EXPECT_LE(cost.evaluations, static_cast<std::uint64_t>(cost.laid_out) * 16);
  EXPECT_LE(cost.shaped, cost.evaluations * 2);
}

TEST_P(GalleryScrollCostTest, ScrollingRepaintsWithoutRebuildingThePage) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start_gallery(providers, GetParam());
  run(providers, application, 60);
  const provider::PixelPoint from{
      .x = GetParam() == examples::GalleryPage::lists      ? 950.0F
           : GetParam() == examples::GalleryPage::controls ? 712.0F
                                                           : 760.0F,
      .y = 560.0F};
  static_cast<void>(stroke(providers, application, from, 240.0F));
  static_cast<void>(stroke(providers, application, from, -240.0F));
  run(providers, application, 30);
  const Cost cost = stroke(providers, application, from, 240.0F);
  std::println(
      "scroll {}: {} frames, {} presented, {} layouts, {} evaluations, "
      "{} shaped, {} visited, {} culled, {} shadow misses, {} quads, "
      "wall {:.3f} ms/frame, {} spins",
      examples::GalleryPages::name(GetParam()), cost.frames, cost.presented,
      cost.laid_out, cost.evaluations, cost.shaped,
      cost.visited / static_cast<std::uint64_t>(cost.frames),
      cost.culled / static_cast<std::uint64_t>(cost.frames), cost.shadow_misses,
      cost.quads / static_cast<std::uint64_t>(cost.frames),
      std::chrono::duration<double, std::milli>(cost.wall).count() /
          cost.frames,
      cost.spins);
  EXPECT_EQ(cost.spins, 0);
  EXPECT_GT(cost.presented, 10);
  EXPECT_LE(cost.laid_out * 6, cost.frames);
  EXPECT_LE(cost.evaluations, static_cast<std::uint64_t>(cost.laid_out) * 16);
  EXPECT_LE(cost.shaped, cost.evaluations * 2);
}

TEST_P(GalleryScrollCostTest, IdleIterationsWalkNothingAndAnimationRepaints) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start_gallery(providers, GetParam());
  run(providers, application, 240);
  const std::uint64_t collections =
      application.interaction().engine().collections();
  const std::uint64_t synchronisations =
      application.interaction().scrolls().synchronisations();
  Cost cost;
  for (int index = 0; index < 60; ++index) {
    ASSERT_TRUE(providers.clock().advance(frame_step).has_value());
    ASSERT_TRUE(application.frame().has_value());
    cost.add(application.last_frame());
  }
  if (cost.presented > 0) {
    EXPECT_EQ(cost.laid_out, 0);
    EXPECT_EQ(cost.shaped, 0U);
    EXPECT_LE(cost.evaluations, static_cast<std::uint64_t>(cost.frames));
    return;
  }
  EXPECT_EQ(application.interaction().engine().collections(), collections);
  EXPECT_EQ(application.interaction().scrolls().synchronisations(),
            synchronisations);
}

INSTANTIATE_TEST_SUITE_P(
    Pages, GalleryScrollCostTest,
    ::testing::ValuesIn(examples::GalleryPages::all.begin(),
                        examples::GalleryPages::all.end()),
    [](const ::testing::TestParamInfo<examples::GalleryPage>& shown) {
      return std::string{examples::GalleryPages::name(shown.param)};
    });

}
