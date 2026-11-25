#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/provider/diagnostics/allocation_meter.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/runtime/image_renderer.hpp>

#include <gallery_fixture.hpp>
#include <graphics_page.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

namespace arisa::runtime {

class GalleryGraphicsTest : public GalleryFixture {
 protected:
  using Page = examples::GraphicsPage;

  class Host final : public composition::Component {
   public:
    static constexpr composition::StateKey<bool> shown{"graphics-host", true};

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
    providers.gpu().rasterize(size);
    Application application = start(providers, Host{});
    run(providers, application, 30);
    return application;
  }
};

TEST_F(GalleryGraphicsTest, ThePageShowsEveryDemo) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  for (const char* title :
       {"Visual effects", "Unavailable games", "Colour adjustments", "Blur",
        "Masks", "Blend modes", "3D flip"}) {
    EXPECT_TRUE(shows(application, title)) << title;
  }
}

TEST_F(GalleryGraphicsTest, FlipAnimatesAndSettles) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  ASSERT_TRUE(application.root().activate(
      composition::ElementKey{std::string{Page::flip_button}}));
  run(providers, application, 90);
  EXPECT_TRUE(shows(application, "Card"));
}

TEST_F(GalleryGraphicsTest, SettledEffectsAllocateNothing) {
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

TEST_F(GalleryGraphicsTest, TheBadgeRendersToAnImage) {
  ImageRenderer renderer{Page::badge()};
  renderer.set_scale(2.0F);
  const auto made = renderer.image();
  ASSERT_TRUE(made.has_value());
  EXPECT_GT(made->width(), 0U);
  EXPECT_GT(made->height(), 0U);
}

}
