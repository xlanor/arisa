#include <arisa/components/zstack.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/provider/gpu/image.hpp>
#include <arisa/provider/gpu/shape_vertex.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/surface/surface_scale.hpp>
#include <arisa/provider/surface/surface_size.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/theme/colour.hpp>

#include <gallery_fixture.hpp>
#include <launch_page.hpp>
#include <launch_screen.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace arisa::runtime {

class GalleryM7Test : public GalleryFixture {
 protected:
  static auto reference(const std::string& key) -> double {
    std::ifstream stream{ARISA_SWIFTUI_REFERENCE};
    std::string line;
    while (std::getline(stream, line)) {
      std::istringstream fields{line};
      std::string name;
      std::string light;
      std::getline(fields, name, '\t');
      std::getline(fields, light, '\t');
      if (name == key) {
        return std::stod(light);
      }
    }
    ADD_FAILURE() << key;
    return 0.0;
  }

  static auto shade(const provider::Image& image, std::uint32_t x,
                    std::uint32_t y) -> std::array<float, 3> {
    const auto found = image.pixel(x, y);
    EXPECT_TRUE(found.has_value());
    if (!found.has_value()) {
      return {};
    }
    return {static_cast<float>((*found)[0]) / 255.0F,
            static_cast<float>((*found)[1]) / 255.0F,
            static_cast<float>((*found)[2]) / 255.0F};
  }

  static auto writing(provider::headless::HeadlessProviderFactory& providers,
                      examples::LaunchScreen::Ink ink) -> Application {
    load_fonts(providers);
    const provider::SurfaceSize size{.width = provider::SurfaceWidth{320},
                                     .height = provider::SurfaceHeight{180}};
    providers.surface().resize(size);
    providers.surface().set_scale(
        provider::SurfaceScale::create(0.25F).value_or(
            provider::SurfaceScale::unit()));
    providers.gpu().rasterize(size);
    auto application = Application::create(
        providers,
        examples::GalleryScreen{examples::GalleryPage::typography,
                                theme::Seed{}, std::nullopt, std::nullopt,
                                examples::LaunchScreen::Style{.ink = ink}},
        ApplicationOptions{
            .executors = std::nullopt,
            .fonts = examples::GalleryFonts::load(providers.assets()),
            .atlas = {},
            .interaction = {},
            .appearance = std::nullopt,
            .launch_colour =
                theme::Colour::hex(examples::LaunchScreen::wallpaper[5])});
    EXPECT_TRUE(application.has_value());
    Application started = std::move(*application);
    for (int step = 0; step < 32; ++step) {
      EXPECT_TRUE(providers.clock()
                      .advance(std::chrono::milliseconds{100})
                      .has_value());
      EXPECT_TRUE(started.frame().has_value());
    }
    return started;
  }

  static auto centres(provider::headless::HeadlessProviderFactory& providers)
      -> std::vector<std::array<float, 2>> {
    const std::vector<provider::ShapeVertex> vertices =
        providers.gpu().last_shapes().vertices;
    std::vector<std::array<float, 2>> found;
    for (std::size_t index = 0; index + 1 < vertices.size(); ++index) {
      const provider::ShapeVertex& a = vertices[index];
      const provider::ShapeVertex& b = vertices[index + 1];
      if (a.kind == provider::ShapeKind::glass_path && a.kind == b.kind &&
          std::abs(a.uv[0] - b.uv[0]) > 0.9F) {
        found.push_back({(a.position[0] + b.position[0]) * 0.5F,
                         (a.position[1] + b.position[1]) * 0.5F});
      }
    }
    return found;
  }
};

TEST_F(GalleryM7Test, GlassLaunchRefractsTheWallpaperInsideTheStroke) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      writing(providers, examples::LaunchScreen::Ink::glass);
  const std::vector<std::array<float, 2>> points = centres(providers);
  ASSERT_GT(points.size(), 10U);
  EXPECT_GT(application.last_frame().backdrops, 0U);
  EXPECT_GT(providers.gpu().passes(), 0U);
  const auto image = providers.gpu().capture();
  ASSERT_TRUE(image.has_value());
  std::size_t coloured = 0;
  std::size_t white = 0;
  for (const std::array<float, 2>& point : points) {
    const auto x =
        static_cast<std::uint32_t>(std::clamp(point[0], 0.0F, 319.0F));
    const auto y =
        static_cast<std::uint32_t>(std::clamp(point[1], 0.0F, 179.0F));
    const std::array<float, 3> seen = shade(*image, x, y);
    const auto [low, high] = std::ranges::minmax(seen);
    if (low > 0.92F) {
      ++white;
    } else if (high - low > 0.08F) {
      ++coloured;
    }
  }
  EXPECT_GT(coloured, points.size() / 2);
  EXPECT_LT(white, points.size() / 10);
}

TEST_F(GalleryM7Test, GlassReplayFromTheLaunchPageDrawsTheStrokeMidWrite) {
  provider::headless::HeadlessProviderFactory providers;
  load_fonts(providers);
  const provider::SurfaceSize size{.width = provider::SurfaceWidth{320},
                                   .height = provider::SurfaceHeight{180}};
  providers.surface().resize(size);
  providers.surface().set_scale(provider::SurfaceScale::create(0.25F).value_or(
      provider::SurfaceScale::unit()));
  providers.gpu().rasterize(size);
  auto created = Application::create(
      providers,
      examples::GalleryScreen{
          examples::GalleryPage::launch, theme::Seed{}, std::nullopt,
          std::nullopt, examples::LaunchScreen::Style{},
          examples::GalleryFonts::lettering(providers.assets())},
      ApplicationOptions{
          .executors = std::nullopt,
          .fonts = examples::GalleryFonts::load(providers.assets()),
          .atlas = {},
          .interaction = {},
          .appearance = std::nullopt,
          .launch_colour = std::nullopt});
  ASSERT_TRUE(created.has_value());
  Application application = std::move(*created);
  for (int step = 0; step < 600; ++step) {
    ASSERT_TRUE(
        providers.clock().advance(std::chrono::milliseconds{16}).has_value());
    ASSERT_TRUE(application.frame().has_value());
  }
  ASSERT_TRUE(application.root().activate(
      composition::ElementKey{examples::LaunchPage::glass_button}));
  for (int step = 0; step < 100; ++step) {
    ASSERT_TRUE(
        providers.clock().advance(std::chrono::milliseconds{16}).has_value());
    ASSERT_TRUE(application.frame().has_value());
  }
  const std::vector<std::array<float, 2>> points = centres(providers);
  ASSERT_GT(points.size(), 4U);
  const auto image = providers.gpu().capture();
  ASSERT_TRUE(image.has_value());
  std::size_t coloured = 0;
  for (const std::array<float, 2>& point : points) {
    const auto x =
        static_cast<std::uint32_t>(std::clamp(point[0], 0.0F, 319.0F));
    const auto y =
        static_cast<std::uint32_t>(std::clamp(point[1], 0.0F, 179.0F));
    const std::array<float, 3> seen = shade(*image, x, y);
    const auto [low, high] = std::ranges::minmax(seen);
    if (high - low > 0.08F && low < 0.92F) {
      ++coloured;
    }
  }
  EXPECT_GT(coloured, points.size() / 2);
}

TEST_F(GalleryM7Test, GlassReplayRefractsAtConsoleSizeUnderTheTargetLimit) {
  constexpr std::size_t image_block_bytes = 0x1000000;
  provider::headless::HeadlessProviderFactory providers;
  load_fonts(providers);
  const provider::SurfaceSize size{.width = provider::SurfaceWidth{1280},
                                   .height = provider::SurfaceHeight{720}};
  providers.surface().resize(size);
  providers.surface().set_scale(provider::SurfaceScale::create(1.25F).value_or(
      provider::SurfaceScale::unit()));
  providers.gpu().rasterize(size);
  providers.gpu().limit_render_targets(
      provider::headless::NullGpu::RenderTargetLimit{
          .extent = provider::Texture::maximum_extent,
          .bytes = image_block_bytes});
  auto created = Application::create(
      providers,
      examples::GalleryScreen{examples::GalleryPage::launch, theme::Seed{},
                              std::nullopt, std::nullopt, std::nullopt},
      ApplicationOptions{
          .executors = std::nullopt,
          .fonts = examples::GalleryFonts::load(providers.assets()),
          .atlas = {},
          .interaction = {},
          .appearance = std::nullopt,
          .launch_colour = std::nullopt});
  ASSERT_TRUE(created.has_value());
  Application application = std::move(*created);
  for (int step = 0; step < 2; ++step) {
    ASSERT_TRUE(
        providers.clock().advance(std::chrono::milliseconds{16}).has_value());
    ASSERT_TRUE(application.frame().has_value());
  }
  ASSERT_TRUE(application.root().activate(
      composition::ElementKey{examples::LaunchPage::glass_button}));
  for (int step = 0; step < 8; ++step) {
    ASSERT_TRUE(
        providers.clock().advance(std::chrono::milliseconds{160}).has_value());
    ASSERT_TRUE(application.frame().has_value());
  }
  EXPECT_GT(application.last_frame().backdrops, 0U);
  const std::vector<std::array<float, 2>> points = centres(providers);
  ASSERT_GT(points.size(), 10U);
  const auto image = providers.gpu().capture();
  ASSERT_TRUE(image.has_value());
  std::size_t coloured = 0;
  for (const std::array<float, 2>& point : points) {
    const auto x =
        static_cast<std::uint32_t>(std::clamp(point[0], 0.0F, 1279.0F));
    const auto y =
        static_cast<std::uint32_t>(std::clamp(point[1], 0.0F, 719.0F));
    const std::array<float, 3> seen = shade(*image, x, y);
    const auto [low, high] = std::ranges::minmax(seen);
    if (high - low > 0.08F && low < 0.92F) {
      ++coloured;
    }
  }
  EXPECT_GT(coloured, points.size() / 2);
  const auto reports = providers.diagnostics().snapshot();
  EXPECT_EQ(std::ranges::count_if(reports,
                                  [](const provider::Diagnostic& each) {
                                    return each.category == "glass";
                                  }),
            0);
}

TEST_F(GalleryM7Test, DropShadowMatchesTheSwiftUIReference) {
  provider::headless::HeadlessProviderFactory providers;
  const provider::SurfaceSize size{.width = provider::SurfaceWidth{200},
                                   .height = provider::SurfaceHeight{120}};
  providers.surface().resize(size);
  providers.surface().set_scale(provider::SurfaceScale::unit());
  providers.gpu().rasterize(size);
  auto application = Application::create(
      providers,
      components::ZStack(
          components::ZStack()
              .frame({.width = 200.0F, .height = 120.0F})
              .background(theme::Colour::hex(0xFFFFFF)),
          components::ZStack()
              .frame({.width = 40.0F, .height = 40.0F})
              .background(theme::Colour::hex(0xFFFFFF))
              .shadow(theme::Colour{0.0F, 0.0F, 0.0F, 0.33F}, 8.0F)),
      ApplicationOptions{});
  ASSERT_TRUE(application.has_value());
  ASSERT_TRUE(application->frame().has_value());
  ASSERT_TRUE(application->frame().has_value());
  const auto image = providers.gpu().capture();
  ASSERT_TRUE(image.has_value());
  const auto alpha = [&](std::uint32_t x) {
    return 1.0F - shade(*image, x, 60)[0];
  };
  float extent = 0.0F;
  for (std::uint32_t step = 0; step < 40; ++step) {
    if (alpha(120 + step) > 1.5F / 255.0F) {
      extent = static_cast<float>(step) + 0.5F;
    }
  }
  EXPECT_NEAR(alpha(120), reference("shadow.default.alpha_at_edge"), 0.03);
  EXPECT_NEAR(extent, reference("shadow.default.radius8_extent"), 1.5);
}

}
