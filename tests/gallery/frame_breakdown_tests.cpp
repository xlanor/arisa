#include <arisa/composition/element_key.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/presentation.hpp>
#include <arisa/motion/spring.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/runtime/frame_breakdown.hpp>
#include <arisa/structure/tab_view.hpp>

#include <gallery_fixture.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <format>
#include <optional>
#include <string>

namespace arisa::runtime {

class GalleryFrameBreakdownTest : public GalleryFixture {
 protected:
  static constexpr int entry_window = 120;
  static constexpr int idle_window = 30;
  static constexpr int settle_slack = 3;
  static constexpr double longest_travel = 100.0;

  struct Entry {
    FrameTimings busiest;
    std::uint64_t shaping_runs{0};
    std::uint64_t rasterised{0};
    std::uint64_t atlas_bytes{0};
    std::uint64_t nodes_created{0};
  };

  static auto step_sidebar(
      provider::headless::HeadlessProviderFactory& providers,
      Application& application, provider::Button direction) -> Entry {
    run(providers, application, idle_window);
    providers.input().push(pad({direction}));
    providers.input().push(pad({}));
    Entry entry;
    for (int index = 0; index < entry_window; ++index) {
      run(providers, application, 1);
      const FrameTimings& timings = application.last_frame();
      if (timings.nodes_created > entry.busiest.nodes_created) {
        entry.busiest = timings;
      }
      entry.shaping_runs += timings.shaping_runs;
      entry.rasterised += timings.rasterised;
      entry.atlas_bytes += timings.atlas_bytes;
      entry.nodes_created += timings.nodes_created;
    }
    return entry;
  }
};

TEST_F(GalleryFrameBreakdownTest, BuildingAPageCountsTheWorkThatBuildsIt) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start_gallery(providers, examples::GalleryPage::controls);
  const FrameTimings built =
      step_sidebar(providers, application, provider::Button::dpad_down).busiest;
  EXPECT_GT(built.nodes_created, 0U);
  EXPECT_GT(built.evaluations, 0U);
  EXPECT_GT(built.layout_computations, 0U);
  EXPECT_GT(built.shaping_runs, 0U);
  EXPECT_GE(built.text_layouts, built.shaping_runs);
  EXPECT_GT(built.rasterised, 0U);
  EXPECT_GT(built.atlas_bytes, 0U);
  EXPECT_EQ(built.image_reads, built.image_decodes);
  const std::string described = FrameBreakdown::describe(built, 42U);
  EXPECT_NE(described.find("unaccounted"), std::string::npos);
  EXPECT_NE(described.find("allocations 42"), std::string::npos);
  EXPECT_NE(described.find(std::format("nodes +{}", built.nodes_created)),
            std::string::npos);
}

TEST_F(GalleryFrameBreakdownTest, AnIdleFrameCountsNoWork) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start_gallery(providers, examples::GalleryPage::lists);
  static_cast<void>(settle(providers, application));
  run(providers, application, idle_window);
  const FrameTimings& idle = application.last_frame();
  EXPECT_EQ(idle.nodes_created, 0U);
  EXPECT_EQ(idle.evaluations, 0U);
  EXPECT_EQ(idle.layout_computations, 0U);
  EXPECT_EQ(idle.shaping_runs, 0U);
  EXPECT_EQ(idle.text_layouts, 0U);
  EXPECT_EQ(idle.rasterised, 0U);
  EXPECT_EQ(idle.atlas_bytes, 0U);
  EXPECT_EQ(idle.image_reads, 0U);
  EXPECT_EQ(idle.textures_created, 0U);
  EXPECT_EQ(idle.memory_blocks_created, 0U);
  EXPECT_EQ(idle.unaccounted(), std::chrono::nanoseconds{0});
}

TEST_F(GalleryFrameBreakdownTest, RevisitingAPageShapesNothingNew) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start_gallery(providers, examples::GalleryPage::controls);
  const Entry first =
      step_sidebar(providers, application, provider::Button::dpad_down);
  ASSERT_GT(first.shaping_runs, 0U);
  ASSERT_GT(first.rasterised, 0U);
  static_cast<void>(
      step_sidebar(providers, application, provider::Button::dpad_up));
  const Entry again =
      step_sidebar(providers, application, provider::Button::dpad_down);
  EXPECT_LT(again.nodes_created, first.nodes_created);
  EXPECT_EQ(again.shaping_runs, 0U);
  EXPECT_EQ(again.rasterised, 0U);
  EXPECT_EQ(again.atlas_bytes, 0U);
}

TEST_F(GalleryFrameBreakdownTest, TheSidebarCapsuleStopsWhenItsSpringSettles) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start_gallery(providers, examples::GalleryPage::controls);
  run(providers, application, idle_window);
  const auto capsule = application.root().tree().find(
      composition::ElementKey{structure::TabView::capsule_id});
  ASSERT_TRUE(capsule.has_value());
  providers.input().push(pad({provider::Button::dpad_down}));
  providers.input().push(pad({}));
  const std::optional<motion::Spring> spring =
      motion::Animation::bouncy(0.45, 0.15).spring_parameters();
  ASSERT_TRUE(spring.has_value());
  const double settling = spring->settling_duration(
      motion::Presentation::precision(motion::Presentation::Property::offset) /
      longest_travel);
  const int budget =
      static_cast<int>(std::ceil(
          settling / std::chrono::duration<double>(frame_step).count())) +
      settle_slack;
  int moving = 0;
  std::optional<int> last;
  for (int index = 0; index < budget + idle_window; ++index) {
    run(providers, application, 1);
    const auto& unhinted = application.last_frame().unhinted;
    if (unhinted.has_value() && unhinted->node == capsule->value()) {
      ++moving;
      last = index;
    }
  }
  EXPECT_GT(moving, 0);
  ASSERT_TRUE(last.has_value());
  EXPECT_LE(*last, budget);
}

}
