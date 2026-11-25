#include <arisa/components/text.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/provider/diagnostics/allocation_meter.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/structure/tab.hpp>
#include <arisa/structure/tab_view.hpp>
#include <arisa/structure/tab_view_style.hpp>

#include <gallery_fixture.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace arisa::runtime {

class GalleryAllocationTest
    : public GalleryFixture,
      public ::testing::WithParamInterface<examples::GalleryPage> {
 protected:
  struct Tally {
    int frames{0};
    int presented{0};
    int repaints{0};
    std::uint64_t allocations{0};
    std::uint64_t repaint_allocations{0};
  };

  static auto measured(provider::headless::HeadlessProviderFactory& providers,
                       Application& application, Tally& tally,
                       std::optional<provider::InputFrame> input = std::nullopt)
      -> bool {
    const bool began =
        input.has_value() && !input->touches.empty() &&
        input->touches.front().phase == provider::TouchPhase::began;
    if (input.has_value()) {
      providers.input().push(std::move(*input));
    }
    if (!providers.clock().advance(frame_step).has_value()) {
      return false;
    }
    const std::size_t reported = providers.diagnostics().snapshot().size();
    const std::uint64_t before = provider::AllocationMeter::count().value_or(0);
    if (!application.frame().has_value()) {
      return false;
    }

    const std::uint64_t used =
        provider::AllocationMeter::count().value_or(0) - before;
    const bool quiet = providers.diagnostics().snapshot().size() == reported;
    const FrameTimings& timings = application.last_frame();
    ++tally.frames;
    tally.allocations += used;
    tally.presented += timings.presented ? 1 : 0;
    if (timings.presented && !timings.laid_out && timings.evaluations == 0 &&
        !began && quiet) {
      ++tally.repaints;
      tally.repaint_allocations += used;
    }
    return true;
  }

  static auto stroke(provider::headless::HeadlessProviderFactory& providers,
                     Application& application, provider::PixelPoint from,
                     float distance) -> Tally {
    Tally tally;
    constexpr int steps = 24;
    EXPECT_TRUE(measured(providers, application, tally,
                         touching(from, provider::TouchPhase::began)));
    for (int index = 1; index <= steps; ++index) {
      EXPECT_TRUE(
          measured(providers, application, tally,
                   touching(
                       provider::PixelPoint{
                           .x = from.x,
                           .y = from.y - (distance * static_cast<float>(index) /
                                          static_cast<float>(steps))},
                       provider::TouchPhase::moved)));
    }
    EXPECT_TRUE(measured(
        providers, application, tally,
        touching(provider::PixelPoint{.x = from.x, .y = from.y - distance},
                 provider::TouchPhase::ended)));
    for (int index = 0; index < 90; ++index) {
      EXPECT_TRUE(measured(providers, application, tally));
    }
    return tally;
  }

  static constexpr std::uint64_t visibility_notification = 8;

  static auto origin() -> provider::PixelPoint {
    return provider::PixelPoint{
        .x = GetParam() == examples::GalleryPage::lists ? 950.0F : 760.0F,
        .y = 560.0F};
  }
};

TEST_P(GalleryAllocationTest, SettledFramesAllocateNothing) {
  if (!provider::AllocationMeter::counting()) {
    GTEST_SKIP() << "allocations are not counted in this build";
  }
  provider::headless::HeadlessProviderFactory providers;
  Application application = start_gallery(providers, GetParam());
  run(providers, application, 240);
  Tally settled;
  for (int index = 0; index < 120; ++index) {
    ASSERT_TRUE(measured(providers, application, settled));
  }
  EXPECT_EQ(settled.allocations, 0U)
      << settled.presented << " of " << settled.frames << " frames presented";
}

TEST_P(GalleryAllocationTest, ScrollRepaintsAllocateNothing) {
  if (!provider::AllocationMeter::counting()) {
    GTEST_SKIP() << "allocations are not counted in this build";
  }
  provider::headless::HeadlessProviderFactory providers;
  Application application = start_gallery(providers, GetParam());
  run(providers, application, 60);
  for (int round = 0; round < 2; ++round) {
    static_cast<void>(stroke(providers, application, origin(), 240.0F));
    static_cast<void>(stroke(providers, application, origin(), -240.0F));
  }
  const Tally down = stroke(providers, application, origin(), 240.0F);
  const Tally up = stroke(providers, application, origin(), -240.0F);
  const std::uint64_t allocations =
      down.repaint_allocations + up.repaint_allocations;
  const int repaints = down.repaints + up.repaints;
  if (GetParam() == examples::GalleryPage::feed) {
    EXPECT_LE(allocations,
              static_cast<std::uint64_t>(repaints) * visibility_notification);
    return;
  }
  EXPECT_EQ(allocations, 0U) << repaints << " repaint frames";
}

class TabStepAllocationTest : public GalleryFixture {
 protected:
  class Tabs final : public composition::Component {
   public:
    explicit Tabs(std::size_t count) noexcept : count_(count) {}

    auto operator==(const Tabs&) const -> bool = default;

    auto body(composition::Context&) const -> composition::Element override {
      std::vector<composition::Element> tabs;
      for (std::size_t index = 0; index < count_; ++index) {
        tabs.push_back(structure::Tab(std::format("Tab {}", index), "star", [] {
          return composition::Element{components::Text("page")};
        }));
      }
      return composition::Element{
          structure::TabView(std::nullopt, std::move(tabs))}
          .tab_view_style(structure::SidebarAdaptableTabViewStyle{});
    }

   private:
    std::size_t count_;
  };

  static constexpr int settle_frames = 60;

  static auto step_allocations(std::size_t count) -> std::uint64_t {
    provider::headless::HeadlessProviderFactory providers;
    auto created = Application::create(providers, Tabs{count});
    EXPECT_TRUE(created.has_value());
    Application application = std::move(*created);
    run(providers, application, settle_frames);
    const std::uint64_t before = provider::AllocationMeter::count().value_or(0);
    providers.input().push(pad({provider::Button::r}));
    providers.input().push(pad({}));
    run(providers, application, settle_frames);
    return provider::AllocationMeter::count().value_or(0) - before;
  }
};

TEST_F(TabStepAllocationTest, UnchangedTabItemsAreNotRebuiltOnAStep) {
  if (!provider::AllocationMeter::counting()) {
    GTEST_SKIP() << "allocations are not counted in this build";
  }
  constexpr std::size_t few = 3;
  constexpr std::size_t many = 24;
  constexpr std::uint64_t per_tab = 2;
  const std::uint64_t small = step_allocations(few);
  const std::uint64_t large = step_allocations(many);
  EXPECT_LE(large, small + (per_tab * (many - few)))
      << small << " allocations with " << few << " tabs";
}

INSTANTIATE_TEST_SUITE_P(
    Pages, GalleryAllocationTest,
    ::testing::ValuesIn(examples::GalleryPages::all.begin(),
                        examples::GalleryPages::all.end()),
    [](const ::testing::TestParamInfo<examples::GalleryPage>& shown) {
      return std::string{examples::GalleryPages::name(shown.param)};
    });

}
