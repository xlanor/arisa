#include <arisa/components/text.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/provider/diagnostics/allocation_meter.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>

#include <animators_page.hpp>
#include <gallery_fixture.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace arisa::runtime {

class GalleryAnimatorsTest : public GalleryFixture {
 protected:
  using Page = examples::AnimatorsPage;

  class Host final : public composition::Component {
   public:
    static constexpr composition::StateKey<bool> shown{"animators-host", true};

    auto operator==(const Host&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      return Page{context.state(shown)};
    }
  };

  static auto open(provider::headless::HeadlessProviderFactory& providers)
      -> Application {
    load_fonts(providers);
    const provider::SurfaceSize size{.width = provider::SurfaceWidth{1280},
                                     .height = provider::SurfaceHeight{720}};
    providers.surface().resize(size);
    Application application = start(providers, Host{});
    run(providers, application, 30);
    return application;
  }

  static void activate(provider::headless::HeadlessProviderFactory& providers,
                       Application& application, std::string_view key) {
    ASSERT_TRUE(
        application.root().activate(composition::ElementKey{std::string{key}}));
    run(providers, application, 40);
  }

  static auto clock_text(Application& application)
      -> std::optional<std::string> {
    std::optional<std::string> found;
    walk(application.root().tree().root(),
         [&found](const composition::Node& node) {
           if (const auto text = node.content_as<components::Text::Content>();
               text.has_value() && text->get().text().size() == 8 &&
               text->get().text()[2] == ':' && text->get().text()[5] == ':') {
             found = std::string{text->get().text()};
           }
         });
    return found;
  }
};

TEST_F(GalleryAnimatorsTest, ThePageShowsEveryDemo) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  for (const char* title :
       {"Live", "Prompt", "Clock", "Zoom", "Glass morph", "Materialize",
        "Favourite", "Transitions", "Styles"}) {
    EXPECT_TRUE(shows(application, title)) << title;
  }
}

TEST_F(GalleryAnimatorsTest, SettledAnimatorsAllocateNothing) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  run(providers, application, 60);
  std::uint64_t used = 0;
  for (int index = 0; index < 60; ++index) {
    EXPECT_TRUE(providers.clock().advance(frame_step).has_value());
    const std::uint64_t before = provider::AllocationMeter::count().value_or(0);
    EXPECT_TRUE(application.frame().has_value());
    used += provider::AllocationMeter::count().value_or(0) - before;
  }
  if (provider::AllocationMeter::counting()) {
    EXPECT_EQ(used, 0U);
  }
}

TEST_F(GalleryAnimatorsTest, LiveDemosTickTheClock) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  const std::optional<std::string> still = clock_text(application);
  ASSERT_TRUE(still.has_value());
  run(providers, application, 70);
  EXPECT_EQ(clock_text(application), still);
  activate(providers, application, Page::live_toggle);
  const std::optional<std::string> first = clock_text(application);
  run(providers, application, 70);
  EXPECT_NE(clock_text(application), first);
}

TEST_F(GalleryAnimatorsTest, TheZoomCardOpensItsDetailAndReturns) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  activate(providers, application, Page::zoom_card);
  run(providers, application, 60);
  EXPECT_TRUE(shows(application, "‹ Back"));
  press(providers, application, provider::Button::b);
  run(providers, application, 60);
  EXPECT_FALSE(shows(application, "‹ Back"));
}

TEST_F(GalleryAnimatorsTest, TheGlassButtonMorphsIntoAToolbar) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  const auto widest = [&application] {
    float found = 0.0F;
    walk(application.root().tree().root(),
         [&found](const composition::Node& node) {
           const auto& glass = node.modifiers().glass;
           if (glass.has_value() && glass->id.has_value()) {
             found = std::max(found, node.frame().width);
           }
         });
    return found;
  };
  EXPECT_FLOAT_EQ(widest(), 72.0F);
  activate(providers, application, Page::morph_button);
  EXPECT_FLOAT_EQ(widest(), 280.0F);
  activate(providers, application, Page::morph_button);
  EXPECT_FLOAT_EQ(widest(), 72.0F);
}

TEST_F(GalleryAnimatorsTest, TheMorePageSurvivesRenderTargetPressure) {
  for (const std::size_t limit :
       {std::size_t{0}, std::size_t{3}, std::size_t{8}, std::size_t{16}}) {
    provider::headless::HeadlessProviderFactory providers;
    providers.gpu().limit_render_targets(
        provider::headless::NullGpu::RenderTargetLimit{.count = limit});
    Application application = open(providers);
    activate(providers, application, Page::live_toggle);
    for (const std::string_view key :
         {Page::morph_button, Page::materialize_button, Page::styles_button,
          Page::morph_button, Page::materialize_button, Page::styles_button}) {
      activate(providers, application, key);
      run(providers, application, 90);
    }
    EXPECT_TRUE(shows(application, "Styles")) << limit;
  }
}

}
