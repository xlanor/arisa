#include <arisa/components/text.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/provider/diagnostics/allocation_meter.hpp>
#include <arisa/provider/headless/headless_factory.hpp>

#include <gallery_fixture.hpp>
#include <shapes_page.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace arisa::runtime {

class GalleryShapesTest : public GalleryFixture {
 protected:
  using Page = examples::ShapesPage;

  class Host final : public composition::Component {
   public:
    static constexpr composition::StateKey<bool> shown{"shapes-host", true};

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

  static auto find(const composition::Node& node, std::string_view wanted)
      -> std::optional<composition::Rect> {
    if (const auto text = node.content_as<components::Text::Content>();
        text.has_value() && text->get().text() == wanted) {
      return node.presented().has_value() ? node.presented()->bounds
                                          : node.frame();
    }
    for (const composition::Node& child : node.children()) {
      if (const std::optional<composition::Rect> found = find(child, wanted)) {
        return found;
      }
    }
    return std::nullopt;
  }
};

TEST_F(GalleryShapesTest, ThePageShowsEveryDemo) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  for (const char* title : {"Shapes", "Concentric", "Shadows", "Gradient",
                            "Tiers", "Gradients", "Transforms", "Styles"}) {
    EXPECT_TRUE(shows(application, title)) << title;
  }
}

TEST_F(GalleryShapesTest, GradientTextRunsAlongTheRamp) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = open(providers);
  const std::optional<composition::Rect> frame =
      find(application.root().tree().root(), "Gradient text");
  ASSERT_TRUE(frame.has_value());
  const auto image = providers.gpu().capture();
  ASSERT_TRUE(image.has_value());
  const float scale = static_cast<float>(image->width()) / 1280.0F;
  std::optional<std::array<int, 3>> first;
  std::array<int, 3> last{};
  const auto row_from = static_cast<std::uint32_t>(frame->y * scale);
  const auto row_to =
      static_cast<std::uint32_t>((frame->y + frame->height) * scale);
  const auto column_from = static_cast<std::uint32_t>(frame->x * scale);
  const auto column_to =
      static_cast<std::uint32_t>((frame->x + frame->width) * scale);
  for (std::uint32_t x = column_from; x < column_to; ++x) {
    for (std::uint32_t y = row_from; y < row_to; ++y) {
      const auto seen = image->pixel(x, y);
      if (!seen.has_value()) {
        continue;
      }
      const std::array<int, 3> colour{(*seen)[0], (*seen)[1], (*seen)[2]};
      if (colour[0] > 200 && (colour[1] < 140 || colour[2] < 90)) {
        if (!first.has_value()) {
          first = colour;
        }
        last = colour;
      }
    }
  }
  ASSERT_TRUE(first.has_value());
  EXPECT_GT((*first)[2], (*first)[1]);
  EXPECT_GT(last[1], last[2] + 60);
}

TEST_F(GalleryShapesTest, SettledShapesAllocateNothing) {
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

}
