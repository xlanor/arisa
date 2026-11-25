#include <arisa/components/button.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/provider/diagnostics/allocation_meter.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>

#include <gallery_fixture.hpp>
#include <input_page.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace arisa::runtime {

class GalleryInputTest : public GalleryFixture {
 protected:
  using Page = examples::InputPage;

  class Host final : public composition::Component {
   public:
    static constexpr composition::StateKey<bool> shown{"input-host", true};

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
    ASSERT_TRUE(application.root().activate(composition::ElementKey{
        Page::tile_key("input-demo", static_cast<std::size_t>(demo))}));
    run(providers, application, 40);
  }
};

TEST_F(GalleryInputTest, EveryTileOpensItsDemo) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  EXPECT_TRUE(shows(application, "Input"));
  for (const Page::Demo demo :
       {Page::Demo::pinch, Page::Demo::location, Page::Demo::sequence,
        Page::Demo::clipboard, Page::Demo::home, Page::Demo::focused,
        Page::Demo::reset, Page::Demo::keyboard}) {
    choose(providers, application, demo);
    EXPECT_TRUE(shows(application, std::string{Page::title_of(demo)}))
        << Page::title_of(demo);
    press(providers, application, provider::Button::b);
    run(providers, application, 60);
  }
}

TEST_F(GalleryInputTest, TapLocationReadsOut) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  choose(providers, application, Page::Demo::location);
  const composition::Rect area =
      presented_of(application, Page::location_target);
  tap(providers, application,
      provider::PixelPoint{.x = area.x + 40.0F, .y = area.y + 30.0F});
  run(providers, application, 4);
  EXPECT_TRUE(shows(application, "40, 30"));
}

TEST_F(GalleryInputTest, CopyAndPasteMovesATileBetweenLists) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  choose(providers, application, Page::Demo::clipboard);
  press(providers, application, provider::Button::plus);
  run(providers, application, 20);
  press(providers, application, provider::Button::a);
  run(providers, application, 20);
  press(providers, application, provider::Button::dpad_right);
  press(providers, application, provider::Button::plus);
  run(providers, application, 20);
  press(providers, application, provider::Button::a);
  run(providers, application, 20);
  EXPECT_TRUE(
      node_of(application, Page::tile_key("input-pasted", 0)).has_value());
}

TEST_F(GalleryInputTest, AControllerDropsADockTileOnTheGrid) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  choose(providers, application, Page::Demo::home);
  for (int index = 0; index < 6; ++index) {
    if (focused_key(application) == Page::tile_key("input-dock", 0)) {
      break;
    }
    press(providers, application, provider::Button::dpad_down);
  }
  ASSERT_EQ(focused_key(application), Page::tile_key("input-dock", 0));
  press(providers, application, provider::Button::x);
  press(providers, application, provider::Button::dpad_up);
  press(providers, application, provider::Button::x);
  run(providers, application, 10);
  EXPECT_TRUE(shows(application, std::string{Page::dock[0]}));
  EXPECT_FALSE(application.interaction().drag().has_value());
}

TEST_F(GalleryInputTest, AKeyboardShortcutPressesNext) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  choose(providers, application, Page::Demo::keyboard);
  provider::InputFrame down;
  down.keys.push_back(
      provider::KeyEvent{.key = provider::Key::left_super, .down = true});
  down.keys.push_back(
      provider::KeyEvent{.key = provider::Key::n, .down = true});
  providers.input().push(down);
  provider::InputFrame up;
  up.keys.push_back(provider::KeyEvent{.key = provider::Key::n, .down = false});
  up.keys.push_back(
      provider::KeyEvent{.key = provider::Key::left_super, .down = false});
  providers.input().push(up);
  run(providers, application, 6);
  const composition::Tree& tree = application.root().tree();
  const auto next =
      tree.find(composition::ElementKey{std::string{Page::next_button}});
  ASSERT_TRUE(next.has_value());
  const auto label =
      tree.node(*next)->get().content_as<components::Button::Content>();
  ASSERT_TRUE(label.has_value());
  EXPECT_EQ(label->get().label(), "Next 1");
}

TEST_F(GalleryInputTest, SettledDemosAllocateNothing) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  for (const Page::Demo demo : {Page::Demo::pinch, Page::Demo::home,
                                Page::Demo::focused, Page::Demo::keyboard}) {
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
