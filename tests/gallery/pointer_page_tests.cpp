#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/provider/diagnostics/allocation_meter.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/provider/input/provider.hpp>

#include <gallery_fixture.hpp>
#include <pointer_page.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <string>

namespace arisa::runtime {

class GalleryPointerTest : public GalleryFixture {
 protected:
  using Page = examples::PointerPage;

  class Host final : public composition::Component {
   public:
    static constexpr composition::StateKey<bool> shown{"pointer-host", true};

    auto operator==(const Host&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      return Page{context.state(shown)};
    }
  };

  static auto open(provider::headless::HeadlessProviderFactory& providers,
                   std::uint32_t height = 720) -> Application {
    load_fonts(providers);
    providers.surface().resize(
        provider::SurfaceSize{.width = provider::SurfaceWidth{1280},
                              .height = provider::SurfaceHeight{height}});
    providers.input().set_pointer_host(provider::PointerHost::application);
    Application application = start(providers, Host{});
    run(providers, application, 20);
    return application;
  }

  static auto at(provider::PixelPoint position) -> provider::InputFrame {
    provider::InputFrame frame;
    frame.pointer = provider::PointerState{.position = position};
    return frame;
  }

  static void point(provider::headless::HeadlessProviderFactory& providers,
                    Application& application, provider::PixelPoint position) {
    providers.input().push(
        at(provider::PixelPoint{.x = position.x + 1.0F, .y = position.y}));
    providers.input().push(at(position));
    run(providers, application, 4);
  }

  static auto pixel_centre(Application& application, std::string_view key)
      -> provider::PixelPoint {
    const composition::Rect frame = presented_of(application, key);
    return provider::PixelPoint{.x = frame.x + (frame.width * 0.5F),
                                .y = frame.y + (frame.height * 0.5F)};
  }
};

TEST_F(GalleryPointerTest, ThePageShowsEverySection) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  for (const std::string_view title :
       {"Hover effects", "Groups", "Pointer styles", "Hover", "Wheel"}) {
    EXPECT_TRUE(shows(application, title)) << title;
  }
  for (std::size_t index = 0; index < Page::style_names.size(); ++index) {
    EXPECT_TRUE(node_of(application, Page::tile_key("pointer-style", index))
                    .has_value())
        << Page::style_names[index];
  }
  for (std::size_t index = 0; index < Page::effect_names.size(); ++index) {
    EXPECT_TRUE(node_of(application, Page::tile_key("pointer-effect", index))
                    .has_value());
  }
}

TEST_F(GalleryPointerTest, EveryStyleTileSetsItsPointer) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  for (std::size_t index = 0; index < 6; ++index) {
    point(providers, application,
          pixel_centre(application, Page::tile_key("pointer-style", index)));
    const auto pointer = application.interaction().pointer();
    ASSERT_TRUE(pointer.has_value());
    EXPECT_EQ(pointer->style, Page::style_of(index))
        << Page::style_names[index];
    EXPECT_TRUE(pointer->visible);
  }
}

TEST_F(GalleryPointerTest, ACursorThatStaysStillAllocatesNothing) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  point(providers, application,
        pixel_centre(application, Page::tile_key("pointer-style", 0)));
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

TEST_F(GalleryPointerTest, MovingTheCursorRepaintsOnlyTheCursor) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  const provider::PixelPoint start{.x = 1240.0F, .y = 700.0F};
  point(providers, application, start);
  run(providers, application, 30);
  providers.input().push(
      at(provider::PixelPoint{.x = start.x - 6.0F, .y = start.y}));
  EXPECT_TRUE(application.frame().has_value());
  const FrameTimings& moved = application.last_frame();
  EXPECT_TRUE(moved.presented);
  EXPECT_LT(moved.damaged_pixels, 64U * 64U);
}

TEST_F(GalleryPointerTest, HoverReadsOutAndTheWheelScrolls) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers, 2000);
  const composition::Rect area = presented_of(application, Page::hover_area);
  point(providers, application,
        provider::PixelPoint{.x = area.x + 12.0F, .y = area.y + 24.0F});
  EXPECT_TRUE(shows(application, "Inside"));
  EXPECT_TRUE(shows(application, "12, 24"));
  const provider::PixelPoint list = pixel_centre(application, Page::wheel_list);
  point(providers, application, list);
  const float before = offset(application, Page::wheel_list).y;
  provider::InputFrame wheel = at(list);
  wheel.pointer->wheel_y = -4.0F;
  providers.input().push(wheel);
  run(providers, application, 40);
  EXPECT_GT(offset(application, Page::wheel_list).y, before + 10.0F);
}

}
