#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/provider/diagnostics/allocation_meter.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>

#include <gallery_fixture.hpp>
#include <groupings_page.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

namespace arisa::runtime {

class GalleryGroupingsTest : public GalleryFixture {
 protected:
  using Page = examples::GroupingsPage;

  class Host final : public composition::Component {
   public:
    static constexpr composition::StateKey<bool> shown{"groupings-host", true};

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

  static void choose(provider::headless::HeadlessProviderFactory& providers,
                     Application& application, Page::Demo demo) {
    ASSERT_TRUE(application.root().activate(
        composition::ElementKey{Page::tile_key(demo)}));
    run(providers, application, 40);
  }

  static constexpr std::array all{Page::Demo::dates,     Page::Demo::colour,
                                  Page::Demo::gauges,    Page::Demo::sizes,
                                  Page::Demo::group_box, Page::Demo::editing,
                                  Page::Demo::menus,     Page::Demo::wheel};
};

TEST_F(GalleryGroupingsTest, EveryTileOpensItsDemo) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  EXPECT_TRUE(shows(application, "Controls and groupings"));
  for (const Page::Demo demo : all) {
    choose(providers, application, demo);
    EXPECT_TRUE(shows(application, std::string{Page::title_of(demo)}))
        << Page::title_of(demo);
    press(providers, application, provider::Button::b);
    run(providers, application, 60);
  }
}

TEST_F(GalleryGroupingsTest, TheEditButtonPutsTheListInEditMode) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  choose(providers, application, Page::Demo::editing);
  EXPECT_TRUE(shows(application, "Edit"));
  press(providers, application, provider::Button::a);
  run(providers, application, 20);
  EXPECT_TRUE(shows(application, "Done"));
}

TEST_F(GalleryGroupingsTest, TheWheelEngagesWithAAndTurnsWithTheDPad) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  choose(providers, application, Page::Demo::wheel);
  EXPECT_TRUE(shows(application, "Chocolate"));
  const auto wheel = application.interaction().focused();
  press(providers, application, provider::Button::a);
  press(providers, application, provider::Button::dpad_down);
  run(providers, application, 20);
  EXPECT_EQ(application.interaction().focused(), wheel);
  EXPECT_TRUE(shows(application, "Mint"));
  press(providers, application, provider::Button::a);
  press(providers, application, provider::Button::dpad_down);
  run(providers, application, 20);
  EXPECT_NE(application.interaction().focused(), wheel);
}

TEST_F(GalleryGroupingsTest, SettledDemosAllocateNothing) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  for (const Page::Demo demo : all) {
    choose(providers, application, demo);
    run(providers, application, 60);
    std::uint64_t used = 0;
    for (int index = 0; index < 60; ++index) {
      EXPECT_TRUE(providers.clock().advance(frame_step).has_value());
      const std::uint64_t before =
          provider::AllocationMeter::count().value_or(0);
      EXPECT_TRUE(application.frame().has_value());
      used += provider::AllocationMeter::count().value_or(0) - before;
    }
    if (provider::AllocationMeter::counting()) {
      EXPECT_EQ(used, 0U) << Page::title_of(demo);
    }
    press(providers, application, provider::Button::b);
    run(providers, application, 60);
  }
}

}
