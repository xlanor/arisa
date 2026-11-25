#include <arisa/components/text.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/layout/viewport.hpp>
#include <arisa/provider/gpu/image.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/provider.hpp>
#include <arisa/provider/input/touch_panel.hpp>
#include <arisa/provider/surface/surface_scale.hpp>
#include <arisa/provider/surface/surface_size.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/runtime/application_options.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/typography.hpp>

#include <gallery_fixture.hpp>
#include <gallery_fonts.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace arisa::runtime {

class ConsoleDisplayTest : public GalleryFixture {
 protected:
  struct Console {
    provider::SurfaceSize framebuffer;
    provider::TouchPanel panel;
  };

  struct Shown {
    composition::Rect root;
    composition::Rect label;
    std::uint32_t glyph_rows{0};
  };

  static constexpr provider::SurfaceSize switch_panel{
      .width = provider::SurfaceWidth{1280},
      .height = provider::SurfaceHeight{720}};
  static constexpr Console nx_handheld{
      .framebuffer = switch_panel, .panel = provider::TouchPanel{switch_panel}};
  static constexpr Console nx_docked{
      .framebuffer = {.width = provider::SurfaceWidth{1920},
                      .height = provider::SurfaceHeight{1080}},
      .panel = provider::TouchPanel{switch_panel}};
  static constexpr Console ounce_handheld{
      .framebuffer = {.width = provider::SurfaceWidth{1920},
                      .height = provider::SurfaceHeight{1080}},
      .panel = provider::TouchPanel{switch_panel}};
  static constexpr Console ounce_docked{
      .framebuffer = {.width = provider::SurfaceWidth{3840},
                      .height = provider::SurfaceHeight{2160}},
      .panel = provider::TouchPanel{switch_panel}};

  static constexpr std::string_view label_key = "label";

  static auto scene() -> composition::Element {
    return components::ZStack(
               components::Text("H")
                   .font(theme::Font::large_title)
                   .foreground_style(theme::Colour::hex(0x000000))
                   .id(std::string{label_key}))
        .frame({.width = 400.0F, .height = 200.0F})
        .background(theme::Colour::hex(0xFFFFFF));
  }

  static auto show(provider::SurfaceSize framebuffer) -> Shown {
    provider::headless::HeadlessProviderFactory providers;
    load_fonts(providers);
    providers.surface().resize(framebuffer);
    providers.surface().set_scale(
        provider::SurfaceScale::for_pixels(framebuffer));
    providers.gpu().rasterize(framebuffer);
    auto application = Application::create(
        providers, scene(),
        ApplicationOptions{
            .executors = std::nullopt,
            .fonts = examples::GalleryFonts::load(providers.assets()),
            .atlas = {},
            .interaction = {}});
    EXPECT_TRUE(application.has_value());
    if (!application.has_value()) {
      return {};
    }
    run(providers, *application, 4);
    const composition::Tree& tree = application->root().tree();
    Shown shown{.root = tree.root().frame(), .label = {}, .glyph_rows = 0};
    if (const auto found = tree.find(composition::ElementKey{label_key})) {
      if (const auto node = tree.node(*found)) {
        shown.label = node->get().frame();
      }
    }
    const auto image = providers.gpu().capture();
    EXPECT_TRUE(image.has_value());
    if (!image.has_value()) {
      return shown;
    }
    const float scale = provider::SurfaceScale::for_pixels(framebuffer).value();
    const auto left =
        static_cast<std::uint32_t>(std::floor(shown.label.x * scale));
    const auto top =
        static_cast<std::uint32_t>(std::floor(shown.label.y * scale));
    const auto right = static_cast<std::uint32_t>(
        std::ceil((shown.label.x + shown.label.width) * scale));
    const auto bottom = static_cast<std::uint32_t>(
        std::ceil((shown.label.y + shown.label.height) * scale));
    for (std::uint32_t y = top; y < bottom && y < image->height(); ++y) {
      for (std::uint32_t x = left; x < right && x < image->width(); ++x) {
        const auto pixel = image->pixel(x, y);
        if (pixel.has_value() && (*pixel)[0] < 128) {
          ++shown.glyph_rows;
          break;
        }
      }
    }
    return shown;
  }

  static auto pixel_scale(const Console& console) -> float {
    return provider::SurfaceScale::for_pixels(console.framebuffer).value();
  }
};

TEST_F(ConsoleDisplayTest, OunceHandheldLaysOutLikeNxHandheldAtOneAndAHalf) {
  EXPECT_FLOAT_EQ(pixel_scale(nx_handheld), 1.0F);
  EXPECT_FLOAT_EQ(pixel_scale(ounce_handheld), 1.5F);
  EXPECT_FLOAT_EQ(pixel_scale(ounce_docked), 3.0F);
  for (const float content : {1.0F, 1.25F}) {
    const auto points = [content](const Console& console) {
      const layout::Viewport viewport{
          .pixels = console.framebuffer,
          .scale =
              provider::SurfaceScale::create(pixel_scale(console) * content)
                  .value()};
      return std::array{viewport.width(), viewport.height()};
    };
    EXPECT_EQ(points(ounce_handheld), points(nx_handheld));
    EXPECT_EQ(points(ounce_docked), points(nx_handheld));
  }
  const layout::Viewport gallery{
      .pixels = ounce_handheld.framebuffer,
      .scale =
          provider::SurfaceScale::create(pixel_scale(ounce_handheld) * 1.25F)
              .value()};
  EXPECT_FLOAT_EQ(gallery.width(), 1024.0F);
  EXPECT_FLOAT_EQ(gallery.height(), 576.0F);
}

TEST_F(ConsoleDisplayTest, TheSameTreeLaysOutInTheSamePointsAndTextIsSharper) {
  const Shown nx = show(nx_handheld.framebuffer);
  const Shown ounce = show(ounce_handheld.framebuffer);
  const Shown docked = show(ounce_docked.framebuffer);
  EXPECT_EQ(nx.root, ounce.root);
  EXPECT_EQ(nx.root, docked.root);
  EXPECT_FLOAT_EQ(nx.root.width, 1280.0F);
  EXPECT_FLOAT_EQ(nx.root.height, 720.0F);
  EXPECT_EQ(nx.label, ounce.label);
  EXPECT_EQ(nx.label, docked.label);
  ASSERT_GT(nx.glyph_rows, 0U);
  EXPECT_NEAR(
      static_cast<float>(ounce.glyph_rows) / static_cast<float>(nx.glyph_rows),
      1.5F, 0.1F);
  EXPECT_NEAR(
      static_cast<float>(docked.glyph_rows) / static_cast<float>(nx.glyph_rows),
      3.0F, 0.15F);
}

TEST_F(ConsoleDisplayTest, TouchRoundTripsPointsOnBothConsoles) {
  constexpr std::array<provider::PixelPoint, 5> points{
      {{.x = 0.0F, .y = 0.0F},
       {.x = 100.0F, .y = 50.0F},
       {.x = 640.0F, .y = 360.0F},
       {.x = 1001.5F, .y = 333.25F},
       {.x = 1280.0F, .y = 720.0F}}};
  for (const Console& console :
       {nx_handheld, nx_docked, ounce_handheld, ounce_docked}) {
    const float scale = pixel_scale(console);
    for (const provider::PixelPoint point : points) {
      const provider::PixelPoint pixels{.x = point.x * scale,
                                        .y = point.y * scale};
      const provider::PixelPoint reported =
          console.panel.to_panel(pixels, console.framebuffer);
      const provider::PixelPoint back =
          console.panel.to_pixels(reported, console.framebuffer);
      EXPECT_NEAR(back.x / scale, point.x, 0.01F);
      EXPECT_NEAR(back.y / scale, point.y, 0.01F);
    }
    const provider::PixelPoint corner = console.panel.to_pixels(
        {.x = 1280.0F, .y = 720.0F}, console.framebuffer);
    EXPECT_FLOAT_EQ(corner.x,
                    static_cast<float>(console.framebuffer.width.value()));
    EXPECT_FLOAT_EQ(corner.y,
                    static_cast<float>(console.framebuffer.height.value()));
    const provider::PixelPoint beyond = console.panel.to_pixels(
        {.x = 5000.0F, .y = -20.0F}, console.framebuffer);
    EXPECT_FLOAT_EQ(beyond.x, corner.x);
    EXPECT_FLOAT_EQ(beyond.y, 0.0F);
  }
}

}
