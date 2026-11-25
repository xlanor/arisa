#include <arisa/components/text.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/provider/diagnostics/allocation_meter.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/provider.hpp>

#include <gallery_fixture.hpp>
#include <text_formats_page.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <string>

namespace arisa::runtime {

using namespace std::chrono_literals;

class GalleryTextFormatsTest : public GalleryFixture {
 protected:
  using Page = examples::TextFormatsPage;

  class Host final : public composition::Component {
   public:
    static constexpr composition::StateKey<bool> shown{"formats-host", true};

    auto operator==(const Host&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      return Page{context.state(shown)};
    }
  };

  static constexpr std::chrono::seconds wall{1778307210};

  static auto open(provider::headless::HeadlessProviderFactory& providers)
      -> Application {
    providers.clock().set_wall(Page::Date{wall});
    load_fonts(providers);
    providers.surface().resize(
        provider::SurfaceSize{.width = provider::SurfaceWidth{1280},
                              .height = provider::SurfaceHeight{720}});
    Application application = start(providers, Host{});
    run(providers, application, 20);
    return application;
  }

  static auto text_of(Application& application, std::string_view key)
      -> std::string {
    const auto id = node_of(application, key);
    if (!id.has_value()) {
      return {};
    }
    const auto node = application.root().tree().node(*id);
    if (!node.has_value()) {
      return {};
    }
    const auto text = node->get().content_as<components::Text::Content>();
    return text.has_value() ? std::string{text->get().text()} : std::string{};
  }

  static auto centre(Application& application, std::string_view key)
      -> provider::PixelPoint {
    const composition::Rect frame = presented_of(application, key);
    return provider::PixelPoint{.x = frame.x + (frame.width * 0.5F),
                                .y = frame.y + (frame.height * 0.5F)};
  }
};

TEST_F(GalleryTextFormatsTest, EverySectionShows) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  for (const std::string_view title :
       {"Live", "Formats", "Runs", "Lines", "1.5 MB", "21 kilometers",
        "3 items for $7.50", "Small Caps"}) {
    EXPECT_TRUE(shows(application, title)) << title;
  }
  EXPECT_FALSE(text_of(application, Page::timer_text).empty());
  EXPECT_FALSE(text_of(application, Page::relative_text).empty());
  EXPECT_FALSE(text_of(application, Page::stopwatch_text).empty());
}

TEST_F(GalleryTextFormatsTest, TappingAddReformatsTheInterpolation) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  tap(providers, application, centre(application, Page::count_button));
  run(providers, application, 20);
  EXPECT_TRUE(shows(application, "4 items for $10.00"));
}

TEST_F(GalleryTextFormatsTest, TheTimerTicksWithTheWallClock) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  const std::string before = text_of(application, Page::timer_text);
  for (int index = 0; index < 70; ++index) {
    EXPECT_TRUE(providers.clock().advance(frame_step).has_value());
    EXPECT_TRUE(application.frame().has_value());
  }
  EXPECT_NE(text_of(application, Page::timer_text), before);
}

TEST_F(GalleryTextFormatsTest, ASettledPageOnlyPresentsWhenLiveTextChanges) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  run(providers, application, 60);
  int presented = 0;
  std::uint64_t idle = 0;
  for (int index = 0; index < 120; ++index) {
    EXPECT_TRUE(providers.clock().advance(frame_step).has_value());
    const std::uint64_t before = provider::AllocationMeter::count().value_or(0);
    EXPECT_TRUE(application.frame().has_value());
    const std::uint64_t used =
        provider::AllocationMeter::count().value_or(0) - before;
    if (application.last_frame().presented) {
      ++presented;
    } else {
      idle += used;
    }
  }
  EXPECT_GE(presented, 1);
  EXPECT_LE(presented, 6);
  if (provider::AllocationMeter::counting()) {
    EXPECT_EQ(idle, 0U);
  }
}

}
