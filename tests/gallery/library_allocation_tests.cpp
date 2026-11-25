#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/provider/diagnostics/allocation_meter.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>

#include <gallery_fixture.hpp>
#include <library_page.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

namespace arisa::runtime {

class LibraryAllocationTest : public GalleryFixture {
 protected:
  class Host final : public composition::Component {
   public:
    static constexpr composition::StateKey<bool> shown{"library-host", true};

    auto operator==(const Host&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      return examples::LibraryPage{context.state(shown)};
    }
  };

  struct Tally {
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
    const std::uint64_t before = provider::AllocationMeter::count().value_or(0);
    if (!application.frame().has_value()) {
      return false;
    }
    const std::uint64_t used =
        provider::AllocationMeter::count().value_or(0) - before;
    const FrameTimings& timings = application.last_frame();
    tally.allocations += used;
    if (timings.presented && !timings.laid_out && timings.evaluations == 0 &&
        !began) {
      ++tally.repaints;
      tally.repaint_allocations += used;
    }
    return true;
  }

  static void section(provider::headless::HeadlessProviderFactory& providers,
                      Application& application) {
    const auto id = application.root().tree().find(composition::ElementKey{
        std::string{examples::LibraryPage::sections_button}});
    ASSERT_TRUE(id.has_value());
    const composition::Node& node = application.root().tree().node(*id)->get();
    const composition::Rect frame =
        node.presented().has_value() ? node.presented()->bounds : node.frame();
    const provider::PixelPoint point{.x = frame.x + (frame.width * 0.5F),
                                     .y = frame.y + (frame.height * 0.5F)};
    providers.input().push(touching(point, provider::TouchPhase::began));
    providers.input().push(touching(point, provider::TouchPhase::ended));
    run(providers, application, 120);
  }

  static void expect_quiet(bool sectioned) {
    provider::headless::HeadlessProviderFactory providers;
    Application application = start(providers, Host{});
    run(providers, application, 240);
    if (sectioned) {
      section(providers, application);
      EXPECT_TRUE(application.root()
                      .tree()
                      .find(composition::ElementKey{
                          std::string{"arisa.grid.section.0"}})
                      .has_value());
    }
    Tally settled;
    for (int index = 0; index < 120; ++index) {
      ASSERT_TRUE(measured(providers, application, settled));
    }
    EXPECT_EQ(settled.allocations, 0U);
    const provider::PixelPoint from{.x = 640.0F, .y = 560.0F};
    const auto stroke = [&](float distance, Tally& tally) {
      constexpr int steps = 24;
      ASSERT_TRUE(measured(providers, application, tally,
                           touching(from, provider::TouchPhase::began)));
      for (int index = 1; index <= steps; ++index) {
        ASSERT_TRUE(measured(
            providers, application, tally,
            touching(
                provider::PixelPoint{
                    .x = from.x,
                    .y = from.y - (distance * static_cast<float>(index) /
                                   static_cast<float>(steps))},
                provider::TouchPhase::moved)));
      }
      ASSERT_TRUE(measured(
          providers, application, tally,
          touching(provider::PixelPoint{.x = from.x, .y = from.y - distance},
                   provider::TouchPhase::ended)));
      for (int index = 0; index < 90; ++index) {
        ASSERT_TRUE(measured(providers, application, tally));
      }
    };
    Tally warming;
    for (int round = 0; round < 2; ++round) {
      stroke(240.0F, warming);
      stroke(-240.0F, warming);
    }
    Tally scrolled;
    stroke(240.0F, scrolled);
    stroke(-240.0F, scrolled);
    EXPECT_GT(scrolled.repaints, 0);
    EXPECT_EQ(scrolled.repaint_allocations, 0U)
        << scrolled.repaints << " repaint frames";
  }
};

TEST_F(LibraryAllocationTest,
       FiveThousandCoversScrollAndSettleWithoutAllocating) {
  if (!provider::AllocationMeter::counting()) {
    GTEST_SKIP() << "allocations are not counted in this build";
  }
  expect_quiet(false);
}

TEST_F(LibraryAllocationTest,
       PinnedSectionHeadersRecycleCellsWithoutAllocating) {
  if (!provider::AllocationMeter::counting()) {
    GTEST_SKIP() << "allocations are not counted in this build";
  }
  expect_quiet(true);
}

}
