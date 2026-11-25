#include <arisa/components/button.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/aspect_ratio.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/text_attributes.hpp>
#include <arisa/provider/gpu/image.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/runtime/application_options.hpp>
#include <arisa/theme/colour.hpp>

#include <gallery_fixture.hpp>
#include <typography_page.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::runtime {

class TextFitTest : public GalleryFixture {
 protected:
  static constexpr provider::SurfaceSize size{
      .width = provider::SurfaceWidth{480},
      .height = provider::SurfaceHeight{360}};

  struct Ink {
    float left{-1.0F};
    float right{-1.0F};
    float top{-1.0F};
    float bottom{-1.0F};

    auto found() const noexcept -> bool { return left >= 0.0F; }
  };

  static auto launch(provider::headless::HeadlessProviderFactory& providers,
                     composition::Element content) -> Application {
    load_fonts(providers);
    providers.surface().resize(size);
    providers.surface().set_scale(provider::SurfaceScale::unit());
    providers.gpu().rasterize(size);
    auto application = Application::create(
        providers, std::move(content),
        ApplicationOptions{
            .executors = std::nullopt,
            .fonts = examples::GalleryFonts::load(providers.assets()),
            .atlas = {},
            .interaction = {},
            .appearance = std::nullopt});
    EXPECT_TRUE(application.has_value());
    Application started = std::move(*application);
    run(providers, started, 6);
    return started;
  }

  static auto frame(Application& application, std::string_view key)
      -> composition::Rect {
    const auto node = node_named(application, key);
    return node.has_value() ? node->get().frame() : composition::Rect{};
  }

  static auto capture(provider::headless::HeadlessProviderFactory& providers)
      -> provider::Image {
    auto image = providers.gpu().capture();
    EXPECT_TRUE(image.has_value());
    return std::move(*image);
  }

  static auto ink(const provider::Image& image, composition::Rect area,
                  int threshold = 60) -> Ink {
    const auto ground = image.pixel(1, 1).value();
    Ink seen;
    const auto x0 = static_cast<std::uint32_t>(std::max(area.x, 0.0F));
    const auto y0 = static_cast<std::uint32_t>(std::max(area.y, 0.0F));
    const auto x1 = std::min(static_cast<std::uint32_t>(area.x + area.width),
                             image.width());
    const auto y1 = std::min(static_cast<std::uint32_t>(area.y + area.height),
                             image.height());
    for (std::uint32_t y = y0; y < y1; ++y) {
      for (std::uint32_t x = x0; x < x1; ++x) {
        const auto pixel = image.pixel(x, y).value();
        int difference = 0;
        for (std::size_t channel = 0; channel < 3; ++channel) {
          difference =
              std::max(difference, std::abs(static_cast<int>(pixel[channel]) -
                                            static_cast<int>(ground[channel])));
        }
        if (difference < threshold) {
          continue;
        }
        const auto fx = static_cast<float>(x);
        const auto fy = static_cast<float>(y);
        seen.left = seen.found() ? std::min(seen.left, fx) : fx;
        seen.right = std::max(seen.right, fx + 1.0F);
        seen.top = seen.top < 0.0F ? fy : std::min(seen.top, fy);
        seen.bottom = std::max(seen.bottom, fy + 1.0F);
      }
    }
    return seen;
  }

  static auto picture(std::uint32_t width, std::uint32_t height)
      -> std::shared_ptr<const provider::Image> {
    std::vector<std::byte> pixels(static_cast<std::size_t>(width) * height *
                                  provider::Image::channels);
    for (std::size_t index = 0; index < pixels.size();
         index += provider::Image::channels) {
      pixels[index] = std::byte{0xE0};
      pixels[index + 1] = std::byte{0x10};
      pixels[index + 2] = std::byte{0x10};
      pixels[index + 3] = std::byte{0xFF};
    }
    return std::make_shared<const provider::Image>(
        provider::Image::create(width, height, std::move(pixels)).value());
  }
};

TEST_F(TextFitTest, AlignmentAndLineLimitInheritFromTheStack) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = launch(
      providers,
      components::VStack(
          components::Text("A much wider first line\nShort").id("aligned"),
          components::VStack(
              components::Text("One two three four five six seven eight nine "
                               "ten eleven twelve thirteen fourteen")
                  .frame({.width = 200.0F,
                          .alignment = composition::Alignment::leading})
                  .id("limited"))
              .line_limit(1))
          .multiline_text_alignment(composition::TextAlignment::center));
  const provider::Image image = capture(providers);
  const composition::Rect aligned = frame(application, "aligned");
  ASSERT_GT(aligned.height, 0.0F);
  const float half = aligned.height * 0.5F;
  const Ink first = ink(image, {aligned.x, aligned.y, aligned.width, half});
  const Ink second =
      ink(image, {aligned.x, aligned.y + half, aligned.width, half});
  ASSERT_TRUE(first.found());
  ASSERT_TRUE(second.found());
  EXPECT_NEAR((second.left + second.right) * 0.5F,
              (first.left + first.right) * 0.5F, 2.0F);
  EXPECT_GT(second.left - first.left, 20.0F);
  const composition::Rect limited = frame(application, "limited");
  EXPECT_LT(limited.height, aligned.height * 0.75F);
  const Ink cut = ink(image, limited);
  ASSERT_TRUE(cut.found());
  EXPECT_LE(cut.right, limited.x + limited.width + 1.0F);
}

TEST_F(TextFitTest, ReservedLinesHoldTheirHeight) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      launch(providers,
             components::VStack(
                 components::Text("One").id("plain"),
                 components::Text("One").line_limit(3, true).id("reserved")));
  const float line = frame(application, "plain").height;
  EXPECT_NEAR(frame(application, "reserved").height, line * 3.0F, 1.0F);
  const provider::Image image = capture(providers);
  const composition::Rect reserved = frame(application, "reserved");
  const Ink drawn = ink(image, reserved);
  ASSERT_TRUE(drawn.found());
  EXPECT_LT(drawn.bottom, reserved.y + line + 1.0F);
}

TEST_F(TextFitTest, MinimumScaleShrinksTheTitleInsteadOfCuttingIt) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = launch(
      providers, components::VStack(
                     components::Text("A long title that shrinks")
                         .font(theme::Font::title)
                         .id("natural"),
                     components::Text("A long title that shrinks")
                         .font(theme::Font::title)
                         .frame({.width = 120.0F,
                                 .alignment = composition::Alignment::leading})
                         .line_limit(1)
                         .minimum_scale_factor(0.3F)
                         .id("shrunk")));
  const provider::Image image = capture(providers);
  const Ink full = ink(image, frame(application, "natural"));
  const composition::Rect box = frame(application, "shrunk");
  const Ink small = ink(image, box);
  ASSERT_TRUE(full.found());
  ASSERT_TRUE(small.found());
  EXPECT_LE(small.right, box.x + box.width + 1.0F);
  EXPECT_GT(small.right - small.left, 100.0F);
  const float wanted = 120.0F / (full.right - full.left);
  ASSERT_LT(wanted, 0.75F);
  EXPECT_LT(small.bottom - small.top, (full.bottom - full.top) * 0.85F);
  EXPECT_NEAR((small.right - small.left) / (full.right - full.left), wanted,
              0.06F);
}

TEST_F(TextFitTest, MarqueeScrollsOnlyWhileItsTileIsFocused) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = launch(
      providers,
      components::VStack(
          components::Button("First", [] {}).id("first"),
          components::Button(
              [] {}, components::Text("A focused tile title that is far too "
                                      "long to fit in its tile")
                         .marquee())
              .frame({.width = 160.0F})
              .id("tile")));
  ASSERT_EQ(focused_key(application), "first");
  const composition::Rect tile = frame(application, "tile");
  run(providers, application, 120);
  const Ink resting = ink(capture(providers), tile);
  run(providers, application, 30);
  const Ink still = ink(capture(providers), tile);
  ASSERT_TRUE(resting.found());
  EXPECT_FLOAT_EQ(resting.left, still.left);
  EXPECT_FLOAT_EQ(resting.right, still.right);
  press(providers, application, provider::Button::dpad_down);
  ASSERT_EQ(focused_key(application), "tile");
  run(providers, application, 30);
  const provider::Image waiting = capture(providers);
  run(providers, application, 90);
  const provider::Image moved = capture(providers);
  const Ink before = ink(waiting, tile);
  const Ink after = ink(moved, tile);
  ASSERT_TRUE(before.found());
  ASSERT_TRUE(after.found());
  EXPECT_GE(after.left, tile.x - 1.0F);
  EXPECT_LE(after.right, tile.x + tile.width + 1.0F);
  std::size_t differing = 0;
  for (auto y = static_cast<std::uint32_t>(tile.y);
       y < static_cast<std::uint32_t>(tile.y + tile.height); ++y) {
    for (auto x = static_cast<std::uint32_t>(tile.x);
         x < static_cast<std::uint32_t>(tile.x + tile.width); ++x) {
      if (waiting.pixel(x, y).value()[0] != moved.pixel(x, y).value()[0]) {
        ++differing;
      }
    }
  }
  EXPECT_GT(differing, 50U);
  EXPECT_TRUE(application.last_frame().animating);
  ASSERT_TRUE(application.last_frame().frame_rate.has_value());
  EXPECT_GE(*application.last_frame().frame_rate, 59.0);
}

TEST_F(TextFitTest, AspectRatioFitsAndFillsLikeSwiftUI) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = launch(
      providers,
      components::VStack(
          components::VStackOptions{.spacing = 0.0F},
          components::VStack(
              components::VStack()
                  .background(theme::Colour::hex(0x2040E0))
                  .aspect_ratio(2.0F, composition::ContentMode::fit)
                  .id("fit"))
              .frame({.width = 300.0F, .height = 100.0F}),
          components::VStack(
              components::VStack()
                  .background(theme::Colour::hex(0x2040E0))
                  .aspect_ratio(2.0F, composition::ContentMode::fill)
                  .id("fill"))
              .frame({.width = 100.0F, .height = 60.0F}),
          components::VStack(components::VStack()
                                 .background(theme::Colour::hex(0x2040E0))
                                 .aspect_ratio(2.0F)
                                 .id("open"))
              .frame({.width = 160.0F})));
  const composition::Rect fit = frame(application, "fit");
  EXPECT_NEAR(fit.width, 200.0F, 0.5F);
  EXPECT_NEAR(fit.height, 100.0F, 0.5F);
  const composition::Rect fill = frame(application, "fill");
  EXPECT_NEAR(fill.width, 120.0F, 0.5F);
  EXPECT_NEAR(fill.height, 60.0F, 0.5F);
  const composition::Rect open = frame(application, "open");
  EXPECT_NEAR(open.width, 160.0F, 0.5F);
  EXPECT_NEAR(open.height, 80.0F, 0.5F);
  const provider::Image image = capture(providers);
  const Ink fitted = ink(image, {0.0F, fit.y, 480.0F, fit.height});
  ASSERT_TRUE(fitted.found());
  EXPECT_NEAR(fitted.left, fit.x, 1.0F);
  EXPECT_NEAR(fitted.right - fitted.left, 200.0F, 1.0F);
}

TEST_F(TextFitTest, CoverArtFitsAndFillsInAFixedTile) {
  provider::headless::HeadlessProviderFactory providers;
  const auto tall = picture(40, 80);
  Application application = launch(
      providers,
      components::VStack(
          components::VStackOptions{.spacing = 8.0F},
          components::Image::Raster(tall, components::Image::ContentMode::fit)
              .frame({.width = 100.0F, .height = 100.0F})
              .id("fit"),
          components::Image::Raster(tall, components::Image::ContentMode::fill)
              .frame({.width = 100.0F, .height = 100.0F})
              .id("fill")));
  const provider::Image image = capture(providers);
  const composition::Rect fit = frame(application, "fit");
  const Ink letterboxed = ink(image, fit);
  ASSERT_TRUE(letterboxed.found());
  EXPECT_NEAR(letterboxed.right - letterboxed.left, 50.0F, 1.5F);
  EXPECT_NEAR(letterboxed.bottom - letterboxed.top, 100.0F, 1.5F);
  EXPECT_NEAR(letterboxed.left - fit.x, 25.0F, 1.5F);
  const composition::Rect fill = frame(application, "fill");
  const Ink filled = ink(image, fill);
  ASSERT_TRUE(filled.found());
  EXPECT_NEAR(filled.right - filled.left, 100.0F, 1.5F);
  EXPECT_NEAR(filled.bottom - filled.top, 100.0F, 1.5F);
}

TEST_F(TextFitTest, TypographyPageShowsTheTextAndImageFitTiles) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start_gallery(providers, examples::GalleryPage::typography);
  run(providers, application, 8);
  for (const std::string_view key : {examples::TypographyPage::alignment_id,
                                     examples::TypographyPage::reserved_id,
                                     examples::TypographyPage::shrinking_id,
                                     examples::TypographyPage::counter_id,
                                     examples::TypographyPage::marquee_id,
                                     examples::TypographyPage::covers_id}) {
    const auto id = node_of(application, key);
    ASSERT_TRUE(id.has_value()) << key;
    const composition::Rect placed =
        application.root().tree().node(*id)->get().frame();
    EXPECT_GT(placed.width, 0.0F) << key;
    EXPECT_GT(placed.height, 0.0F) << key;
  }
}

}
