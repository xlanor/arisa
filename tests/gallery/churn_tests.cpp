#include <arisa/provider/diagnostics/allocation_meter.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/runtime/frame_timings.hpp>

#include <gallery_fixture.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

namespace arisa::runtime {

class GalleryChurnTest
    : public GalleryFixture,
      public ::testing::WithParamInterface<examples::GalleryPage> {
 protected:
  struct Tally {
    int frames{0};
    int presented{0};
    std::uint64_t allocations{0};
    std::uint64_t uploaded{0};
    std::uint64_t created{0};
    std::uint64_t renders{0};
  };

  static constexpr int idle_frames = 120;
  static constexpr int held_frames = 60;
  static constexpr int quiet_frames = 15;
  static constexpr int settle_limit = 900;
  static constexpr std::uint64_t animated_targets_per_frame = 1;

  static auto animates_forever(examples::GalleryPage page) noexcept -> bool {
    return page == examples::GalleryPage::paths ||
           page == examples::GalleryPage::materials;
  }

  static auto start(provider::headless::HeadlessProviderFactory& providers,
                    examples::GalleryPage page) -> Application {
    return start_gallery(
        providers, page, theme::ColorScheme::light,
        provider::SurfaceSize{.width = provider::SurfaceWidth{1920},
                              .height = provider::SurfaceHeight{1080}},
        std::nullopt, 1.5F);
  }

  static auto tilted() -> provider::InputFrame {
    provider::InputFrame frame = pad({});
    frame.pads[0].right = provider::Stick{.x = 0.0F, .y = 1.0F};
    return frame;
  }

  static auto measure(provider::headless::HeadlessProviderFactory& providers,
                      Application& application, int count, bool held) -> Tally {
    Tally tally;
    for (int frame = 0; frame < count; ++frame) {
      if (held) {
        providers.input().push(tilted());
      }
      EXPECT_TRUE(providers.clock().advance(frame_step).has_value());
      const std::uint64_t before =
          provider::AllocationMeter::count().value_or(0);
      EXPECT_TRUE(application.frame().has_value());
      tally.allocations +=
          provider::AllocationMeter::count().value_or(0) - before;
      const FrameTimings& timings = application.last_frame();
      ++tally.frames;
      tally.presented += timings.presented ? 1 : 0;
      tally.uploaded += timings.uploaded_bytes;
      tally.created += timings.targets_created;
      tally.renders += timings.layer_renders;
    }
    return tally;
  }

  static void hold_until_quiet(
      provider::headless::HeadlessProviderFactory& providers,
      Application& application) {
    int quiet = 0;
    for (int frame = 0; frame < settle_limit && quiet < quiet_frames; ++frame) {
      providers.input().push(tilted());
      run(providers, application, 1);
      quiet = application.last_frame().presented ? 0 : quiet + 1;
    }
  }
};

TEST_P(GalleryChurnTest, IdleFramesPresentAndAllocateNothing) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, GetParam());
  run(providers, application, 240);
  const Tally idle = measure(providers, application, idle_frames, false);
  if (animates_forever(GetParam())) {
    EXPECT_LE(idle.created, animated_targets_per_frame * idle_frames)
        << idle.created << " targets in " << idle.frames << " frames";
    return;
  }
  EXPECT_EQ(idle.presented, 0)
      << idle.presented << " of " << idle.frames << " frames presented";
  EXPECT_EQ(idle.uploaded, 0U);
  EXPECT_EQ(idle.created, 0U);
  if (provider::AllocationMeter::counting()) {
    EXPECT_EQ(idle.allocations, 0U);
  }
}

TEST_P(GalleryChurnTest,
       AHeldStickAtTheLimitPresentsNothingAndCreatesNoTargets) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, GetParam());
  run(providers, application, 60);
  hold_until_quiet(providers, application);
  const Tally held = measure(providers, application, held_frames, true);
  EXPECT_EQ(held.created, 0U)
      << held.created << " targets in " << held.frames << " frames";
  if (animates_forever(GetParam())) {
    return;
  }
  EXPECT_EQ(held.presented, 0)
      << held.presented << " of " << held.frames << " frames presented, "
      << held.uploaded << " bytes uploaded";
  EXPECT_EQ(held.uploaded, 0U);
}

INSTANTIATE_TEST_SUITE_P(
    Pages, GalleryChurnTest,
    ::testing::ValuesIn(examples::GalleryPages::all.begin(),
                        examples::GalleryPages::all.end()),
    [](const ::testing::TestParamInfo<examples::GalleryPage>& shown) {
      return std::string{examples::GalleryPages::name(shown.param)};
    });

}
