#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/provider/diagnostics/allocation_meter.hpp>
#include <arisa/provider/headless/headless_factory.hpp>

#include <gallery_fixture.hpp>
#include <geometry_page.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <format>
#include <string>

namespace arisa::runtime {

class GalleryGeometryTest : public GalleryFixture {
 protected:
  using Page = examples::GeometryPage;

  class Host final : public composition::Component {
   public:
    static constexpr composition::StateKey<bool> shown{"geometry-host", true};

    auto operator==(const Host&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      return Page{context.state(shown)};
    }
  };

  static auto open(provider::headless::HeadlessProviderFactory& providers,
                   provider::SurfaceSize size, float scale) -> Application {
    load_fonts(providers);
    providers.surface().resize(size);
    providers.surface().set_scale(
        provider::SurfaceScale::create(scale).value_or(
            provider::SurfaceScale::unit()));
    Application application = start(providers, Host{});
    run(providers, application, 20);
    return application;
  }

  static void expect_thirds(Application& application) {
    const composition::Rect view = frame_of(application, Page::carousel_view);
    const composition::Rect first = frame_of(application, Page::first_card);
    const composition::Rect second =
        frame_of(application, std::format("geometry-card-{}", 1));
    const float gap = second.x - (first.x + first.width);
    const float margin = first.x - view.x;
    EXPECT_GT(first.width, 0.0F);
    EXPECT_NEAR((first.width * 3.0F) + (gap * 2.0F) + (margin * 2.0F),
                view.width, 0.5F);
  }
};

TEST_F(GalleryGeometryTest, CarouselShowsThreeCardsHandheld) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      open(providers,
           provider::SurfaceSize{.width = provider::SurfaceWidth{1280},
                                 .height = provider::SurfaceHeight{720}},
           1.0F);
  expect_thirds(application);
}

TEST_F(GalleryGeometryTest, CarouselShowsThreeCardsDocked) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      open(providers,
           provider::SurfaceSize{.width = provider::SurfaceWidth{1920},
                                 .height = provider::SurfaceHeight{1080}},
           1.5F);
  expect_thirds(application);
}

TEST_F(GalleryGeometryTest, PriorityKeepsThePriceWhole) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      open(providers,
           provider::SurfaceSize{.width = provider::SurfaceWidth{1280},
                                 .height = provider::SurfaceHeight{720}},
           1.0F);
  EXPECT_GT(frame_of(application, Page::kept_price).width,
            frame_of(application, Page::plain_price).width + 1.0F);
}

TEST_F(GalleryGeometryTest, ABadgeIsRaisedWhenChosen) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      open(providers,
           provider::SurfaceSize{.width = provider::SurfaceWidth{1280},
                                 .height = provider::SurfaceHeight{720}},
           1.0F);
  ASSERT_TRUE(
      application.root().activate(composition::ElementKey{Page::badge_key(2)}));
  run(providers, application, 4);
  const auto node = node_named(application, Page::badge_key(2));
  ASSERT_TRUE(node.has_value());
  EXPECT_EQ(node->get().modifiers().z_index, 1.0);
}

TEST_F(GalleryGeometryTest, DraggingTheKnobReportsTheNamedSpace) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      open(providers,
           provider::SurfaceSize{.width = provider::SurfaceWidth{1280},
                                 .height = provider::SurfaceHeight{720}},
           1.0F);
  const composition::Rect pad = frame_of(application, Page::pad_view);
  const provider::PixelPoint from = centre(application, Page::knob);
  drag(providers, application, from,
       provider::PixelPoint{.x = pad.x + 40.0F, .y = pad.y + 30.0F}, 6);
  run(providers, application, 4);
  EXPECT_TRUE(shows(application, "x 40  y 30"));
  const composition::Rect knob = frame_of(application, Page::knob);
  EXPECT_NEAR(knob.x + (knob.width * 0.5F), pad.x + 40.0F, 0.5F);
}

TEST_F(GalleryGeometryTest, TheReaderShowsItsWidth) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      open(providers,
           provider::SurfaceSize{.width = provider::SurfaceWidth{1280},
                                 .height = provider::SurfaceHeight{720}},
           1.0F);
  const composition::Rect reader = frame_of(application, Page::reader_view);
  EXPECT_GT(reader.width, 0.0F);
  EXPECT_TRUE(shows(application, std::format("{:.0f} pt", reader.width)));
}

TEST_F(GalleryGeometryTest, SettledFramesLayOutAndAllocateNothing) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      open(providers,
           provider::SurfaceSize{.width = provider::SurfaceWidth{1280},
                                 .height = provider::SurfaceHeight{720}},
           1.0F);
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
