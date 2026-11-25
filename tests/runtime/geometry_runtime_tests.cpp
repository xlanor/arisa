#include <arisa/components/button.hpp>
#include <arisa/components/geometry_reader.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/coordinate_space.hpp>
#include <arisa/composition/drag_gesture.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/geometry_proxy.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/structure/list.hpp>
#include <arisa/structure/section.hpp>
#include <arisa/theme/shape.hpp>

#include <scroll_fixture.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace arisa::runtime {

using composition::DragGesture;
using composition::Element;

class GeometryRuntimeTest : public ScrollFixture {
 protected:
  static auto swatch(std::string key, theme::ColourRole role, float size)
      -> Element {
    return components::VStack()
        .frame({.width = size, .height = size})
        .background(role, theme::Shape::rectangle())
        .id(std::move(key));
  }

  static auto red(const provider::Image& image, float x, float y) -> bool {
    const auto seen = image.pixel(static_cast<std::uint32_t>(x),
                                  static_cast<std::uint32_t>(y));
    return seen.has_value() && (*seen)[0] > 180 && (*seen)[1] < 120 &&
           (*seen)[2] < 120;
  }

  class Halves final : public composition::Component {
   public:
    static constexpr composition::StateKey<int> wide{"halves-wide", 300};

    auto operator==(const Halves&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto width = context.state(wide);
      return components::VStack(
          components::GeometryReader(
              [](const composition::GeometryProxy& proxy) -> Element {
                return components::VStack()
                    .frame(
                        {.width = proxy.size().width / 2.0F, .height = 10.0F})
                    .id("half");
              })
              .frame(
                  {.width = static_cast<float>(width.get()), .height = 100.0F})
              .id("reader"),
          components::Button("Shrink", [width] {
            width.set(200);
          }).id("shrink"));
    }
  };
};

TEST_F(GeometryRuntimeTest, ZIndexPaintsTheRaisedViewOnTop) {
  provider::headless::HeadlessProviderFactory providers;
  const provider::SurfaceSize size{.width = provider::SurfaceWidth{200},
                                   .height = provider::SurfaceHeight{200}};
  providers.surface().resize(size);
  providers.gpu().rasterize(size);
  Application application = start(
      providers,
      components::ZStack(
          swatch("raised", theme::ColourRole::destructive, 40.0F).z_index(1.0),
          swatch("plain", theme::ColourRole::accent, 40.0F)));
  frames(application, 2);
  const auto image = providers.gpu().capture();
  ASSERT_TRUE(image.has_value());
  const provider::PixelPoint middle = centre(application, "plain");
  EXPECT_TRUE(red(*image, middle.x, middle.y));
}

TEST_F(GeometryRuntimeTest, WithoutZIndexTheLaterViewPaintsOnTop) {
  provider::headless::HeadlessProviderFactory providers;
  const provider::SurfaceSize size{.width = provider::SurfaceWidth{200},
                                   .height = provider::SurfaceHeight{200}};
  providers.surface().resize(size);
  providers.gpu().rasterize(size);
  Application application = start(
      providers,
      components::ZStack(swatch("first", theme::ColourRole::destructive, 40.0F),
                         swatch("plain", theme::ColourRole::accent, 40.0F)));
  frames(application, 2);
  const auto image = providers.gpu().capture();
  ASSERT_TRUE(image.has_value());
  const provider::PixelPoint middle = centre(application, "plain");
  EXPECT_FALSE(red(*image, middle.x, middle.y));
}

TEST_F(GeometryRuntimeTest, ZIndexDecidesWhichOverlappingViewIsTapped) {
  provider::headless::HeadlessProviderFactory providers;
  auto taps = std::make_shared<std::vector<std::string>>();
  Application application =
      start(providers,
            components::ZStack(
                swatch("raised", theme::ColourRole::destructive, 80.0F)
                    .z_index(2.0)
                    .on_tap_gesture([taps] { taps->emplace_back("raised"); }),
                swatch("plain", theme::ColourRole::accent, 80.0F)
                    .on_tap_gesture([taps] { taps->emplace_back("plain"); })));
  tap(providers, application, centre(application, "plain"));
  EXPECT_EQ(*taps, (std::vector<std::string>{"raised"}));
}

TEST_F(GeometryRuntimeTest, GeometryReaderIsLaidOutFromItsSizeInOneFrame) {
  provider::headless::HeadlessProviderFactory providers;
  auto application = Application::create(providers, Halves{});
  ASSERT_TRUE(application.has_value());
  ASSERT_TRUE(application->frame().has_value());
  EXPECT_NEAR(frame_of(*application, "half").width, 150.0F, 0.01F);
  tap(providers, *application, centre(*application, "shrink"));
  EXPECT_NEAR(frame_of(*application, "half").width, 100.0F, 0.01F);
}

TEST_F(GeometryRuntimeTest, GeometryReaderPagesSettleWithoutLayout) {
  provider::headless::HeadlessProviderFactory providers;
  auto application = Application::create(providers, Halves{});
  ASSERT_TRUE(application.has_value());
  frames(*application, 3);
  EXPECT_EQ(layouts_during(providers, *application, 20), 0);
  EXPECT_EQ(application->last_frame().layout_computations, 0U);
}

TEST_F(GeometryRuntimeTest, DragReportsItsLocationInANamedSpace) {
  provider::headless::HeadlessProviderFactory providers;
  auto seen = std::make_shared<std::vector<DragGesture::Value>>();
  Application application = start(
      providers,
      components::VStack(
          components::VStackOptions{.spacing = 0.0F},
          swatch("spacer", theme::ColourRole::accent, 40.0F),
          swatch("handle", theme::ColourRole::destructive, 80.0F)
              .gesture(DragGesture(0.0F,
                                   composition::CoordinateSpace::named("panel"))
                           .on_changed([seen](const DragGesture::Value& value) {
                             seen->push_back(value);
                           })))
          .coordinate_space("panel")
          .id("panel"));
  const composition::Rect panel = frame_of(application, "panel");
  const provider::PixelPoint from = centre(application, "handle");
  drag(providers, application, from,
       provider::PixelPoint{.x = from.x + 10.0F, .y = from.y}, 4);
  ASSERT_FALSE(seen->empty());
  EXPECT_NEAR(seen->back().location.x, from.x + 10.0F - panel.x, 0.5F);
  EXPECT_NEAR(seen->back().location.y, from.y - panel.y, 0.5F);
}

TEST_F(GeometryRuntimeTest, ListRowsWithANaturalWidthStartAtTheLeadingInset) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers,
      structure::List(structure::Section(
          components::Text("Text row").id("text"),
          components::HStack(swatch("a", theme::ColourRole::accent, 20.0F),
                             swatch("b", theme::ColourRole::accent, 20.0F))
              .id("pair"))));
  frames(application, 2);
  const composition::Rect text = frame_of(application, "text");
  const composition::Rect pair = frame_of(application, "a");
  EXPECT_NEAR(pair.x, text.x, 0.5F);
  EXPECT_LT(pair.x, 64.0F);
}

}
