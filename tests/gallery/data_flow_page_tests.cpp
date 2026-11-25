#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/provider/diagnostics/allocation_meter.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>

#include <data_flow_page.hpp>
#include <gallery_fixture.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <string>

namespace arisa::runtime {

class GalleryDataFlowTest : public GalleryFixture {
 protected:
  using Page = examples::DataFlowPage;

  class Host final : public composition::Component {
   public:
    static constexpr composition::StateKey<bool> shown{"data-flow-host", true};

    auto operator==(const Host&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      return Page{context.state(shown)};
    }
  };

  static auto open(provider::headless::HeadlessProviderFactory& providers)
      -> Application {
    load_fonts(providers);
    providers.surface().resize(
        provider::SurfaceSize{.width = provider::SurfaceWidth{1280},
                              .height = provider::SurfaceHeight{720}});
    Application application = start(providers, Host{});
    run(providers, application, 20);
    return application;
  }

  static auto focus_tile(provider::headless::HeadlessProviderFactory& providers,
                         Application& application) -> bool {
    for (int hop = 0; hop < 12; ++hop) {
      if (focused_key(application).starts_with("data-flow-tile-")) {
        return true;
      }
      press(providers, application, provider::Button::dpad_down);
      run(providers, application, 2);
    }
    return false;
  }
};

TEST_F(GalleryDataFlowTest, TheInboxTitlesItsStackFromInsideItsBody) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  EXPECT_TRUE(shows(application, "Inbox"));
  EXPECT_TRUE(shows(application, "Unread 3"));
  ASSERT_TRUE(application.root().activate(
      composition::ElementKey{Page::Inbox::unread_button}));
  run(providers, application, 4);
  EXPECT_TRUE(shows(application, "Unread 2"));
}

TEST_F(GalleryDataFlowTest, TheMarkerFollowsTheFocusedTile) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  EXPECT_FALSE(node_of(application, Page::marker_view).has_value());
  ASSERT_TRUE(focus_tile(providers, application));
  run(providers, application, 4);
  ASSERT_TRUE(node_of(application, Page::marker_view).has_value());
  const std::string first = focused_key(application);
  const composition::Rect marker = frame_of(application, Page::marker_view);
  const composition::Rect tile = frame_of(application, first);
  EXPECT_NEAR(marker.x + (marker.width * 0.5F), tile.x + (tile.width * 0.5F),
              1.0F);
  press(providers, application, provider::Button::dpad_right);
  run(providers, application, 4);
  const std::string second = focused_key(application);
  ASSERT_NE(second, first);
  const composition::Rect moved = frame_of(application, Page::marker_view);
  const composition::Rect next = frame_of(application, second);
  EXPECT_NEAR(moved.x + (moved.width * 0.5F), next.x + (next.width * 0.5F),
              1.0F);
}

TEST_F(GalleryDataFlowTest, TheEnvironmentTableShowsTheDisplayTraits) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  EXPECT_TRUE(shows(application, "Display scale"));
  EXPECT_TRUE(shows(application, "gregorian"));
  EXPECT_TRUE(shows(application, "regular"));
}

TEST_F(GalleryDataFlowTest, SettledFramesLayOutAndAllocateNothing) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  run(providers, application, 60);
  std::uint64_t used = 0;
  int laid = 0;
  for (int index = 0; index < 120; ++index) {
    ASSERT_TRUE(providers.clock().advance(frame_step).has_value());
    const std::uint64_t before = provider::AllocationMeter::count().value_or(0);
    ASSERT_TRUE(application.frame().has_value());
    used += provider::AllocationMeter::count().value_or(0) - before;
    laid += application.last_frame().laid_out ? 1 : 0;
  }
  EXPECT_EQ(laid, 0);
  if (provider::AllocationMeter::counting()) {
    EXPECT_EQ(used, 0U);
  }
}

}
