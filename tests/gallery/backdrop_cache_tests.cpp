#include <arisa/provider/gpu/image.hpp>
#include <arisa/provider/headless/null_gpu.hpp>
#include <arisa/provider/surface/surface_size.hpp>
#include <arisa/theme/color_scheme.hpp>

#include <gallery_fixture.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <utility>

namespace arisa::runtime {

class GalleryBackdropCacheTest : public GalleryFixture {
 protected:
  struct Tally {
    std::uint64_t blurs{0};
    std::uint64_t damaged{0};
  };

  struct Rendering {
    std::optional<provider::Image> image;
    std::uint64_t shared{0};
    std::uint64_t blurs{0};
  };

  struct Difference {
    int largest{0};
    double mean{0.0};
    double spread{0.0};
  };

  static constexpr provider::SurfaceSize raster{
      .width = provider::SurfaceWidth{960},
      .height = provider::SurfaceHeight{540}};

  static auto render(examples::GalleryPage page, bool separate) -> Rendering {
    provider::headless::HeadlessProviderFactory providers;
    load_fonts(providers);
    providers.gpu().rasterize(raster);
    providers.surface().resize(raster);
    providers.surface().set_scale(provider::SurfaceScale::for_pixels(raster));
    AppearanceOptions appearance{
        .policy = examples::GalleryScreen::policy(theme::ColorScheme::light),
        .storage = std::nullopt};
    appearance.policy.defaults.theme = "arisa-theme-glass";
    appearance.policy.defaults.ui_scale = appearance::UiScales{
        .handheld = 1.25F, .docked = 1.25F, .desktop = 1.25F};
    auto created = Application::create(
        providers,
        examples::GalleryScreen{
            page, theme::Seed{.scheme = theme::ColorScheme::light}},
        ApplicationOptions{
            .executors = std::nullopt,
            .fonts = examples::GalleryFonts::load(providers.assets()),
            .atlas = {},
            .interaction = {},
            .appearance = std::move(appearance),
            .debug_separate_backdrops = separate});
    Rendering rendering;
    if (!created.has_value()) {
      ADD_FAILURE() << "the gallery did not start";
      return rendering;
    }
    Application application = std::move(*created);
    for (int index = 0; index < 16; ++index) {
      if (!providers.clock().advance(frame_step).has_value() ||
          !application.frame().has_value()) {
        ADD_FAILURE() << "frame " << index << " failed";
        return rendering;
      }
      rendering.shared += application.last_frame().shared_backdrops;
      rendering.blurs += application.last_frame().blurs;
    }
    auto captured = providers.gpu().capture();
    if (captured.has_value()) {
      rendering.image = std::move(*captured);
    }
    return rendering;
  }

  static auto compare(const provider::Image& first,
                      const provider::Image& second) -> Difference {
    Difference difference;
    const auto left = first.pixels();
    const auto right = second.pixels();
    const std::size_t count = std::min(left.size(), right.size());
    std::uint64_t total = 0;
    std::uint64_t moved = 0;
    for (std::size_t index = 0; index < count; ++index) {
      const int gap = std::abs(std::to_integer<int>(left[index]) -
                               std::to_integer<int>(right[index]));
      difference.largest = std::max(difference.largest, gap);
      total += static_cast<std::uint64_t>(gap);
      moved += gap > 4 ? 1U : 0U;
    }
    if (count > 0) {
      difference.mean = static_cast<double>(total) / static_cast<double>(count);
      difference.spread =
          static_cast<double>(moved) / static_cast<double>(count);
    }
    return difference;
  }

  static void expect_shared_like_separate(examples::GalleryPage page) {
    const Rendering grouped = render(page, false);
    const Rendering separate = render(page, true);
    ASSERT_TRUE(grouped.image.has_value());
    ASSERT_TRUE(separate.image.has_value());
    ASSERT_EQ(grouped.image->width(), separate.image->width());
    ASSERT_EQ(grouped.image->height(), separate.image->height());
    EXPECT_GT(grouped.shared, 0U);
    EXPECT_EQ(separate.shared, 0U);
    const Difference difference = compare(*grouped.image, *separate.image);
    EXPECT_LE(difference.largest, 32);
    EXPECT_LT(difference.mean, 0.4);
    EXPECT_LT(difference.spread, 0.02);
  }

  static auto start(provider::headless::HeadlessProviderFactory& providers,
                    examples::GalleryPage page) -> Application {
    AppearanceOptions options{
        .policy = examples::GalleryScreen::policy(theme::ColorScheme::light),
        .storage = std::nullopt};
    options.policy.defaults.theme = "arisa-theme-glass";
    options.policy.defaults.ui_scale = appearance::UiScales{
        .handheld = 1.25F, .docked = 1.25F, .desktop = 1.25F};
    return start_gallery(providers, page, theme::ColorScheme::light,
                         provider::headless::ScriptedSurface::default_size,
                         std::move(options));
  }

  static auto scroll_at(provider::headless::HeadlessProviderFactory& providers,
                        Application& application, float column) -> Tally {
    Tally tally;
    const auto count = [&] {
      tally.blurs += application.last_frame().blurs;
      tally.damaged += application.last_frame().damaged_pixels;
    };
    for (const bool down : {true, false, true}) {
      const provider::PixelPoint from{.x = column, .y = down ? 540.0F : 240.0F};
      touch_down(providers, application, from);
      count();
      for (int index = 1; index <= 8; ++index) {
        const float progress = static_cast<float>(index) / 8.0F;
        touch_move(providers, application,
                   provider::PixelPoint{
                       .x = column,
                       .y = from.y + ((down ? -300.0F : 300.0F) * progress)});
        count();
      }
      touch_up(providers, application,
               provider::PixelPoint{.x = column,
                                    .y = from.y + (down ? -300.0F : 300.0F)});
      count();
      for (int index = 0; index < 30; ++index) {
        ScrollFixture::step(providers, application);
        count();
      }
    }
    return tally;
  }
};

TEST_F(GalleryBackdropCacheTest, SidebarRowsDoNotReblurTheChrome) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, examples::GalleryPage::typography);
  run(providers, application, 60);
  const Tally tally = scroll_at(providers, application, 115.0F);
  EXPECT_GT(tally.damaged, 0U);
  EXPECT_EQ(tally.blurs, 0U);
}

TEST_F(GalleryBackdropCacheTest, ContentBehindTheHeaderStillReblursIt) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, examples::GalleryPage::settings);
  run(providers, application, 60);
  const Tally tally = scroll_at(providers, application, 768.0F);
  EXPECT_GT(tally.blurs, 0U);
}

TEST_F(GalleryBackdropCacheTest,
       DraggingMaterialRowsPastTheEdgeReusesTheirBackdropTargets) {
  provider::headless::HeadlessProviderFactory providers;
  AppearanceOptions options{
      .policy = examples::GalleryScreen::policy(theme::ColorScheme::light),
      .storage = std::nullopt};
  options.policy.defaults.theme = "arisa-theme-glass";
  Application application = start_gallery(
      providers, examples::GalleryPage::materials, theme::ColorScheme::light,
      provider::SurfaceSize{.width = provider::SurfaceWidth{1920},
                            .height = provider::SurfaceHeight{1080}},
      std::move(options), 1.5F);
  run(providers, application, 60);
  constexpr float column = 1150.0F;
  constexpr int steps = 90;
  std::uint64_t created = 0;
  std::uint64_t blurs = 0;
  touch_down(providers, application,
             provider::PixelPoint{.x = column, .y = 1000.0F});
  for (const float direction : {-10.0F, 10.0F}) {
    const float start = direction < 0.0F ? 1000.0F : 100.0F;
    for (int step = 1; step <= steps; ++step) {
      touch_move(providers, application,
                 provider::PixelPoint{
                     .x = column,
                     .y = start + (direction * static_cast<float>(step))});
      created += application.last_frame().targets_created;
      blurs += application.last_frame().blurs;
    }
  }
  touch_up(providers, application,
           provider::PixelPoint{.x = column, .y = 1000.0F});
  EXPECT_GT(blurs, 2U * steps);
  EXPECT_LT(created, 60U) << created << " targets for " << blurs << " blurs";
}

TEST_F(GalleryBackdropCacheTest, SharedMaterialCapturesMatchSeparateOnes) {
  expect_shared_like_separate(examples::GalleryPage::materials);
}

TEST_F(GalleryBackdropCacheTest, SharedGlassCapturesMatchSeparateOnes) {
  expect_shared_like_separate(examples::GalleryPage::glass);
}
}
