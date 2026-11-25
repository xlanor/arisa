#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/provider/diagnostics/allocation_meter.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/provider.hpp>

#include <gallery_fixture.hpp>
#include <symbols_page.hpp>

#include <gtest/gtest.h>

#include <cstdint>

namespace arisa::runtime {

class GallerySymbolsTest : public GalleryFixture {
 protected:
  using Page = examples::SymbolsPage;

  class Host final : public composition::Component {
   public:
    static constexpr composition::StateKey<bool> shown{"symbols-host", true};

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

  static auto centre(Application& application, std::string_view key)
      -> provider::PixelPoint {
    const composition::Rect frame = presented_of(application, key);
    return provider::PixelPoint{.x = frame.x + (frame.width * 0.5F),
                                .y = frame.y + (frame.height * 0.5F)};
  }
};

TEST_F(GallerySymbolsTest, EverySectionShows) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  for (const std::string_view title :
       {"Buttons", "Menus", "Mixed state", "Close", "Filter", "Card",
        "Primary 0", "Select all"}) {
    EXPECT_TRUE(shows(application, title)) << title;
  }
  EXPECT_TRUE(node_of(application, Page::split_menu).has_value());
  EXPECT_TRUE(node_of(application, Page::select_all).has_value());
}

TEST_F(GallerySymbolsTest, TappingTheSplitMenuRunsItsPrimaryAction) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  tap(providers, application, centre(application, Page::split_menu));
  run(providers, application, 20);
  EXPECT_TRUE(shows(application, "Primary 1"));
}

TEST_F(GallerySymbolsTest, ASettledPageAllocatesAndPresentsNothing) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  run(providers, application, 60);
  std::uint64_t used = 0;
  int presented = 0;
  for (int index = 0; index < 60; ++index) {
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

}
