#include <arisa/async/manual_executor.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/image_cache.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/provider/assets/asset_path.hpp>
#include <arisa/provider/diagnostics/allocation_meter.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/runtime/application_options.hpp>
#include <arisa/runtime/frame_timings.hpp>
#include <arisa/runtime/root.hpp>

#include <covers_page.hpp>
#include <gallery_fixture.hpp>
#include <presentations_page.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <ranges>
#include <utility>
#include <vector>

namespace arisa::runtime {

class CoversTest : public GalleryFixture {
 protected:
  static constexpr std::uint64_t budget = 12ULL * 1024ULL * 1024ULL;

  class Host final : public composition::Component {
   public:
    static constexpr composition::StateKey<bool> shown{"covers-host", true};

    explicit Host(std::optional<components::ImageCache>* cache) noexcept
        : cache_(cache) {}

    auto operator==(const Host&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      *cache_ = context.environment(components::Environment::image_cache);
      return examples::CoversPage{context.state(shown)};
    }

   private:
    std::optional<components::ImageCache>* cache_;
  };

  static auto rasters(const composition::Node& node) -> int {
    int count =
        node.content_as<components::Image::Raster::Content>().has_value() ? 1
                                                                          : 0;
    for (const composition::Node& child : node.children()) {
      count += rasters(child);
    }
    return count;
  }

  static auto covers(const composition::Tree& tree) -> int {
    const auto scroll =
        tree.find(composition::ElementKey{examples::CoversPage::covers_view});
    if (!scroll.has_value()) {
      return 0;
    }
    const auto node = tree.node(*scroll);
    return node.has_value() ? rasters(node->get()) : 0;
  }

  static void banner(provider::headless::HeadlessProviderFactory& providers) {
    std::ifstream stream{std::filesystem::path{ARISA_SOURCE_ASSETS} /
                             examples::CoversPage::banner_path,
                         std::ios::binary};
    const std::vector<char> raw{std::istreambuf_iterator<char>{stream},
                                std::istreambuf_iterator<char>{}};
    std::vector<std::byte> bytes(raw.size());
    std::ranges::transform(raw, bytes.begin(),
                           [](char value) { return std::byte(value); });
    ASSERT_FALSE(bytes.empty());
    providers.assets().add(
        *provider::AssetPath::create(examples::CoversPage::banner_path),
        std::move(bytes));
  }

  static void push(provider::headless::HeadlessProviderFactory& providers,
                   Application& application, provider::Button button) {
    provider::InputFrame down;
    down.pads[0] = provider::PadState{.connected = true,
                                      .buttons = provider::Buttons{button},
                                      .left = {},
                                      .right = {}};
    provider::InputFrame up;
    up.pads[0] = provider::PadState{
        .connected = true, .buttons = {}, .left = {}, .right = {}};
    providers.input().push(down);
    providers.input().push(up);
    run(providers, application, 3);
  }
};

TEST_F(CoversTest, FiveHundredCoversStreamThroughABoundedCacheOffTheUiThread) {
  provider::headless::HeadlessProviderFactory providers;
  load_fonts(providers);
  async::ManualExecutor background;
  std::optional<components::ImageCache> cache;
  ApplicationOptions options;
  options.executors =
      async::Executors{.background = background, .blocking = background};
  options.fonts = examples::GalleryFonts::load(providers.assets());
  options.images = ImageOptions{
      .decoded = components::ImageCache::Budget{budget}, .textures = budget};
  auto created =
      Application::create(providers, Host{&cache}, std::move(options));
  ASSERT_TRUE(created.has_value());
  Application application = std::move(*created);
  run(providers, application, 6);
  ASSERT_TRUE(cache.has_value());
  EXPECT_GT(background.pending(), 0U);
  EXPECT_EQ(cache->statistics().insertions, 0U);
  push(providers, application, provider::Button::dpad_down);
  std::uint32_t most_pictures = 0;
  std::uint64_t most_bytes = 0;
  float reached = -1.0F;
  for (int page = 0; page < 80; ++page) {
    const std::uint64_t before = cache->statistics().insertions;
    run(providers, application, 2);
    EXPECT_EQ(cache->statistics().insertions, before);
    background.drain();
    run(providers, application, 4);
    const components::ImageCache::Statistics seen = cache->statistics();
    most_bytes = std::max(most_bytes, seen.bytes);
    const FrameTimings& timings = application.last_frame();
    if (timings.presented) {
      most_pictures = std::max(most_pictures, timings.pictures);
    }
    const float now = offset(application, examples::CoversPage::covers_view).y;
    if (now <= reached + 0.5F && seen.insertions >= 500U) {
      break;
    }
    reached = now;
    push(providers, application, provider::Button::zr);
    settle(providers, application, 60);
  }
  const components::ImageCache::Statistics streamed = cache->statistics();
  EXPECT_GE(streamed.insertions, 500U);
  EXPECT_GT(streamed.evictions, 0U);
  EXPECT_LE(most_bytes, budget);
  EXPECT_LE(streamed.bytes, budget);
  const std::uint64_t cover = std::uint64_t{examples::CoversPage::cover_width} *
                              examples::CoversPage::cover_height * 4U;
  EXPECT_GT(most_pictures, 0U);
  EXPECT_LE(std::uint64_t{most_pictures} * cover, 2U * budget);
  const std::uint64_t hits = streamed.hits;
  push(providers, application, provider::Button::zl);
  settle(providers, application, 60);
  background.drain();
  run(providers, application, 4);
  EXPECT_GT(cache->statistics().hits, hits);
  EXPECT_EQ(cache->statistics().insertions, streamed.insertions);
  if (provider::AllocationMeter::counting()) {
    run(providers, application, 30);
    const std::uint64_t before = provider::AllocationMeter::count().value_or(0);
    run(providers, application, 60);
    EXPECT_EQ(provider::AllocationMeter::count().value_or(0) - before, 0U);
  }
}

TEST_F(CoversTest,
       PresentedCoversPaintFromTheRealBackgroundPoolWhileTheLoopIdles) {
  provider::headless::HeadlessProviderFactory providers;
  banner(providers);
  Application application = start_gallery(
      providers, examples::GalleryPage::presentations,
      theme::ColorScheme::light,
      provider::SurfaceSize{.width = provider::SurfaceWidth{1920},
                            .height = provider::SurfaceHeight{1080}},
      std::nullopt, 1.5F);
  ASSERT_TRUE(application.root().activate(
      composition::ElementKey{examples::PresentationsPage::covers_button}));
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds{20};
  int shown = 0;
  int idled = 0;
  std::uint32_t pictures = 0;
  ASSERT_TRUE(application
                  .run_until([&](Root& root) {
                    shown = covers(root.tree());
                    if (application.last_frame().presented) {
                      pictures =
                          std::max(pictures, application.last_frame().pictures);
                    }
                    if (application.last_frame().kind == FrameKind::idle) {
                      ++idled;
                    }
                    static_cast<void>(providers.clock().advance(frame_step));
                    return (shown >= 5 && idled > 0) ||
                           std::chrono::steady_clock::now() > deadline;
                  })
                  .has_value());
  EXPECT_GE(shown, 5);
  EXPECT_GT(idled, 0);
  run(providers, application, 4);
  pictures = std::max(pictures, application.last_frame().pictures);
  EXPECT_GE(pictures, 5U);
}

TEST_F(CoversTest, FlingingThroughCoversReusesEachTileLayerTarget) {
  provider::headless::HeadlessProviderFactory providers;
  banner(providers);
  Application application = start_gallery(
      providers, examples::GalleryPage::presentations,
      theme::ColorScheme::light,
      provider::SurfaceSize{.width = provider::SurfaceWidth{1920},
                            .height = provider::SurfaceHeight{1080}},
      std::nullopt, 1.5F);
  ASSERT_TRUE(application.root().activate(
      composition::ElementKey{examples::PresentationsPage::covers_button}));
  run(providers, application, 90);
  provider::InputFrame tilted;
  tilted.pads[0] = provider::PadState{
      .connected = true, .buttons = {}, .left = {}, .right = {}};
  tilted.pads[0].right = provider::Stick{.x = 0.0F, .y = -1.0F};
  std::uint64_t created = 0;
  std::uint64_t renders = 0;
  for (int frame = 0; frame < 240; ++frame) {
    providers.input().push(tilted);
    run(providers, application, 1);
    created += application.last_frame().targets_created;
    renders += application.last_frame().layer_renders;
  }
  EXPECT_GT(renders, 240U);
  EXPECT_LT(created, 240U) << created << " targets for " << renders
                           << " layer renders";
}

TEST_F(CoversTest, FlingingThroughCoversRedrawsOnlyTilesThatChange) {
  provider::headless::HeadlessProviderFactory providers;
  banner(providers);
  Application application = start_gallery(
      providers, examples::GalleryPage::presentations,
      theme::ColorScheme::light,
      provider::SurfaceSize{.width = provider::SurfaceWidth{1920},
                            .height = provider::SurfaceHeight{1080}},
      std::nullopt, 1.5F);
  ASSERT_TRUE(application.root().activate(
      composition::ElementKey{examples::PresentationsPage::covers_button}));
  run(providers, application, 90);
  provider::InputFrame tilted;
  tilted.pads[0] = provider::PadState{
      .connected = true, .buttons = {}, .left = {}, .right = {}};
  tilted.pads[0].right = provider::Stick{.x = 0.0F, .y = -1.0F};
  std::uint64_t renders = 0;
  std::uint64_t hits = 0;
  constexpr int frames = 240;
  for (int frame = 0; frame < frames; ++frame) {
    providers.input().push(tilted);
    run(providers, application, 1);
    renders += application.last_frame().layer_renders;
    hits += application.last_frame().layer_hits;
  }
  EXPECT_LT(renders, std::uint64_t{frames} * 4U)
      << renders << " layer renders, " << hits << " hits";
  EXPECT_GT(hits, renders * 10U) << renders << " layer renders";
}

TEST_F(CoversTest, AFlingReleasesNoRenderTargetsAndRendersOnlyEnteringTiles) {
  provider::headless::HeadlessProviderFactory providers;
  banner(providers);
  Application application = start_gallery(
      providers, examples::GalleryPage::presentations,
      theme::ColorScheme::light,
      provider::SurfaceSize{.width = provider::SurfaceWidth{1920},
                            .height = provider::SurfaceHeight{1080}},
      std::nullopt, 1.5F);
  ASSERT_TRUE(application.root().activate(
      composition::ElementKey{examples::PresentationsPage::covers_button}));
  run(providers, application, 90);
  provider::InputFrame tilted;
  tilted.pads[0] = provider::PadState{
      .connected = true, .buttons = {}, .left = {}, .right = {}};
  tilted.pads[0].right = provider::Stick{.x = 0.0F, .y = -1.0F};
  constexpr int frames = 240;
  std::uint64_t created = 0;
  std::uint64_t released = 0;
  std::uint64_t renders = 0;
  for (int frame = 0; frame < frames; ++frame) {
    providers.input().push(tilted);
    run(providers, application, 1);
    created += application.last_frame().targets_created;
    released += application.last_frame().targets_released;
    renders += application.last_frame().layer_renders;
  }
  EXPECT_LT(released, std::uint64_t{frames / 20})
      << released << " targets released in " << frames << " frames";
  EXPECT_LT(created, std::uint64_t{frames / 8})
      << created << " targets created in " << frames << " frames";
  EXPECT_LT(renders, std::uint64_t{frames} * 4U) << renders << " layer renders";
}

TEST_F(CoversTest, ABannerDecodedAfterTheFirstFramesReachesTheScreen) {
  const auto banner_pixels = [](bool decoded) {
    provider::headless::HeadlessProviderFactory providers;
    load_fonts(providers);
    banner(providers);
    const provider::SurfaceSize size{.width = provider::SurfaceWidth{640},
                                     .height = provider::SurfaceHeight{360}};
    providers.surface().resize(size);
    providers.gpu().rasterize(size);
    providers.gpu().swapchain(2, true);
    async::ManualExecutor background;
    std::optional<components::ImageCache> cache;
    ApplicationOptions options;
    options.executors =
        async::Executors{.background = background, .blocking = background};
    options.fonts = examples::GalleryFonts::load(providers.assets());
    auto created =
        Application::create(providers, Host{&cache}, std::move(options));
    EXPECT_TRUE(created.has_value());
    Application application = std::move(*created);
    run(providers, application, 12);
    if (decoded) {
      background.drain();
    }
    run(providers, application, 12);
    const auto image = providers.gpu().capture();
    EXPECT_TRUE(image.has_value());
    std::vector<std::array<std::uint8_t, 4>> pixels;
    for (std::uint32_t y = 16; y < 88; ++y) {
      for (std::uint32_t x = 496; x < 624; ++x) {
        pixels.push_back(
            image->pixel(x, y).value_or(std::array<std::uint8_t, 4>{}));
      }
    }
    return pixels;
  };
  const auto placeholder = banner_pixels(false);
  const auto shown = banner_pixels(true);
  const auto differing = std::ranges::count_if(
      std::views::iota(std::size_t{0}, placeholder.size()),
      [&](std::size_t at) { return placeholder[at] != shown[at]; });
  EXPECT_GT(differing, static_cast<std::ptrdiff_t>(placeholder.size() / 2));
}

TEST_F(CoversTest, ReevaluatingThePageKeepsItsCoverCells) {
  provider::headless::HeadlessProviderFactory providers;
  load_fonts(providers);
  async::ManualExecutor background;
  std::optional<components::ImageCache> cache;
  ApplicationOptions options;
  options.executors =
      async::Executors{.background = background, .blocking = background};
  options.fonts = examples::GalleryFonts::load(providers.assets());
  auto created =
      Application::create(providers, Host{&cache}, std::move(options));
  ASSERT_TRUE(created.has_value());
  Application application = std::move(*created);
  run(providers, application, 6);
  background.drain();
  run(providers, application, 6);
  ASSERT_GE(covers(application.root().tree()), 4);
  const std::uint64_t before = application.root().tree().evaluations();
  ASSERT_TRUE(application.root().activate(
      composition::ElementKey{examples::CoversPage::skeleton_button}));
  run(providers, application, 2);
  const std::uint64_t evaluated =
      application.root().tree().evaluations() - before;
  EXPECT_LE(evaluated, 4U);
}

}
