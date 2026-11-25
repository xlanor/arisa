#include <arisa/components/text.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/provider/diagnostics/allocation_meter.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>

#include <gallery_fixture.hpp>
#include <interaction_page.hpp>

#include <gtest/gtest.h>
#include <stb_image.h>
#include <stb_image_write.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::runtime {

using namespace std::chrono_literals;

class GalleryInteractionTest : public GalleryFixture {
 protected:
  using Page = examples::InteractionPage;

  class Host final : public composition::Component {
   public:
    static constexpr composition::StateKey<bool> shown{"interaction-host",
                                                       true};

    auto operator==(const Host&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      return Page{context.state(shown)};
    }
  };

  static auto open(provider::headless::HeadlessProviderFactory& providers,
                   bool rasterized = false, std::uint32_t height = 720)
      -> Application {
    load_fonts(providers);
    const provider::SurfaceSize size{.width = provider::SurfaceWidth{1280},
                                     .height = provider::SurfaceHeight{height}};
    providers.surface().resize(size);
    providers.surface().set_display_mode(provider::DisplayMode::handheld);
    if (rasterized) {
      providers.gpu().rasterize(size);
    }
    Application application = start(providers, Host{});
    run(providers, application, 30);
    return application;
  }

  static auto pixel(provider::headless::HeadlessProviderFactory& providers,
                    Application& application, std::string_view key)
      -> provider::PixelPoint {
    const provider::PixelPoint point = centre(application, key);
    const float points = application.root().tree().root().frame().width;
    const float factor =
        points > 0.0F
            ? static_cast<float>(providers.surface().size().width.value()) /
                  points
            : 1.0F;
    return provider::PixelPoint{.x = point.x * factor, .y = point.y * factor};
  }
};

TEST_F(GalleryInteractionTest, EveryTileShows) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  for (const std::string_view title :
       {"Input kinds", "Mapped gesture", "Modifier keys", "Type select",
        "Drag and drop", "Reorder", "Repeat", "Touch 0", "Pointer 0",
        "Controller 0", "Moved 0", "Option", "Released", "Red", "Green",
        "Drop here", "Step 0"}) {
    EXPECT_TRUE(shows(application, title)) << title;
  }
  EXPECT_EQ(providers.lifecycle().live_home_button_blocks(), 1U);
}

TEST_F(GalleryInteractionTest, InputKindsCountOnlyTheirOwnInput) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  tap(providers, application, pixel(providers, application, Page::touch_area));
  tap(providers, application,
      pixel(providers, application, Page::pointer_area));
  tap(providers, application,
      pixel(providers, application, Page::controller_area));
  run(providers, application, 4);
  EXPECT_TRUE(shows(application, "Touch 1"));
  EXPECT_TRUE(shows(application, "Pointer 0"));
  EXPECT_TRUE(shows(application, "Controller 0"));
}

TEST_F(GalleryInteractionTest, TheSelectionDragsIntoTheTray) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers, false, 1600);
  const provider::PixelPoint from =
      pixel(providers, application, Page::tile_key(1));
  const provider::PixelPoint to = pixel(providers, application, Page::tray);

  touch_down(providers, application, from);
  step(providers, application, 550ms);
  run(providers, application, 2);
  ASSERT_TRUE(application.interaction().drag().has_value());
  EXPECT_EQ(application.interaction().drag()->count, 2U);
  for (int index = 1; index <= 6; ++index) {
    const float progress = static_cast<float>(index) / 6.0F;
    touch_move(
        providers, application,
        provider::PixelPoint{.x = from.x + ((to.x - from.x) * progress),
                             .y = from.y + ((to.y - from.y) * progress)});
  }
  touch_up(providers, application, to);
  run(providers, application, 6);
  EXPECT_TRUE(shows(application, "Red Green"));
}

TEST_F(GalleryInteractionTest, ASettledPageAllocatesNothing) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  run(providers, application, 60);
  std::uint64_t used = 0;
  int presented = 0;
  for (int index = 0; index < 120; ++index) {
    EXPECT_TRUE(providers.clock().advance(frame_step).has_value());
    const std::uint64_t before = provider::AllocationMeter::count().value_or(0);
    EXPECT_TRUE(application.frame().has_value());
    used += provider::AllocationMeter::count().value_or(0) - before;
    presented += application.last_frame().presented ? 1 : 0;
  }
  EXPECT_EQ(presented, 0);
  if (provider::AllocationMeter::counting()) {
    EXPECT_EQ(used, 0U);
  }
}

TEST_F(GalleryInteractionTest, HandheldMatchesGolden) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers, true);
  run(providers, application, 120);
  matches_golden(providers, "interaction_handheld_1280x720");
}

}
