#include <arisa/components/canvas.hpp>
#include <arisa/components/graphics_context.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/shape_view.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/controls/progress_view.hpp>
#include <arisa/graphics/any_shape.hpp>
#include <arisa/graphics/blend_mode.hpp>
#include <arisa/graphics/colour_matrix.hpp>
#include <arisa/graphics/gradient.hpp>
#include <arisa/graphics/shader.hpp>
#include <arisa/graphics/shadow_style.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/motion/angle.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/provider/assets/asset_path.hpp>
#include <arisa/provider/default_factory.hpp>
#include <arisa/provider/gpu/gpu_timings.hpp>
#include <arisa/provider/gpu/image.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/headless/null_gpu.hpp>
#include <arisa/provider/headless/scripted_surface.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/provider/input/provider.hpp>
#include <arisa/provider/providers.hpp>
#include <arisa/provider/surface/display_mode.hpp>
#include <arisa/provider/surface/surface_scale.hpp>
#include <arisa/provider/surface/surface_size.hpp>
#include <arisa/provider/threading/thread_role.hpp>
#include <arisa/provider/threading/thread_spec.hpp>
#include <arisa/provider/vulkan/memory_allocator.hpp>
#include <arisa/provider/vulkan/vulkan_device.hpp>
#include <arisa/provider/vulkan/vulkan_gpu.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/runtime/application_options.hpp>
#include <arisa/theme/color_scheme.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/seed.hpp>
#include <arisa/theme/shape.hpp>

#include <gallery_fonts.hpp>
#include <gallery_page.hpp>
#include <gallery_screen.hpp>
#include <gallery_shaders.hpp>
#include <launch_screen.hpp>
#include <presentations_page.hpp>

#include "vulkan_test.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <print>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace arisa::provider::vulkan {

class VulkanSceneTest : public VulkanTest {
 protected:
  static constexpr std::chrono::milliseconds frame_step{16};
  static constexpr TouchId finger{7};

  using Build = std::function<composition::Element()>;

  struct Look {
    bool glass{false};
    bool render_thread{false};
    runtime::ImageOptions images{};
  };

  class Run {
   public:
    Run(SurfaceSize extent, std::optional<VulkanGpu> gpu,
        bool render_targets = false)
        : gpu_(std::move(gpu)) {
      headless_.surface().resize(extent);
      headless_.surface().set_scale(SurfaceScale::for_pixels(extent));
      if (gpu_.has_value()) {
        factory_.emplace(Providers{.threading = headless_.threading(),
                                   .clock = headless_.clock(),
                                   .diagnostics = headless_.diagnostics(),
                                   .assets = headless_.assets(),
                                   .lifecycle = headless_.lifecycle(),
                                   .system_info = headless_.system_info(),
                                   .haptics = headless_.haptics(),
                                   .audio = headless_.audio(),
                                   .clipboard = headless_.clipboard(),
                                   .browser = headless_.browser(),
                                   .fonts = headless_.fonts(),
                                   .surface = headless_.surface(),
                                   .input = headless_.input(),
                                   .gpu = *gpu_,
                                   .text_input = headless_.text_input()});
      } else {
        if (!render_targets) {
          headless_.gpu().limit_render_targets(
              headless::NullGpu::RenderTargetLimit{.extent = 0, .bytes = 0});
        }
        headless_.gpu().rasterize(extent);
      }
    }

    void add(std::string_view name, std::span<const std::byte> bytes) {
      auto path = AssetPath::create(name);
      ASSERT_TRUE(path.has_value());
      headless_.assets().add(std::move(*path), bytes);
    }

    void start(composition::Element content, bool gallery_fonts) {
      start(std::move(content), gallery_fonts,
            Look{.glass = false, .render_thread = false});
    }

    void start(composition::Element content, bool gallery_fonts, Look look) {
      if (gallery_fonts) {
        for (const auto& [file, bytes] : asset_bytes()) {
          auto path = AssetPath::create(file);
          ASSERT_TRUE(path.has_value());
          headless_.assets().add(std::move(*path), bytes);
        }
      }
      std::optional<runtime::AppearanceOptions> appearance;
      if (look.glass) {
        appearance.emplace(runtime::AppearanceOptions{
            .policy =
                examples::GalleryScreen::policy(theme::ColorScheme::light),
            .storage = std::nullopt});
        appearance->policy.defaults.theme = "arisa-theme-glass";
      }
      auto application = runtime::Application::create(
          factory(), std::move(content),
          runtime::ApplicationOptions{
              .executors = std::nullopt,
              .fonts = gallery_fonts
                           ? examples::GalleryFonts::load(headless_.assets())
                           : std::vector<text::FontEntry>{},
              .atlas = {},
              .interaction = {},
              .appearance = std::move(appearance),
              .launch_colour = std::nullopt,
              .debug_glass_coverage = false,
              .debug_full_walk = false,
              .debug_separate_backdrops = false,
              .render_thread = look.render_thread,
              .pacing = true,
              .images = look.images});
      ASSERT_TRUE(application.has_value());
      application_.emplace(std::move(*application));
    }

    void step(std::optional<InputFrame> input = std::nullopt) {
      if (input.has_value()) {
        headless_.input().push(std::move(*input));
      }
      ASSERT_TRUE(headless_.clock().advance(frame_step).has_value());
      ASSERT_TRUE(application_.has_value());
      ASSERT_TRUE(application_->frame().has_value());
    }

    void advance() {
      ASSERT_TRUE(headless_.clock().advance(frame_step).has_value());
    }

    void step_by(std::chrono::milliseconds duration) {
      ASSERT_TRUE(headless_.clock().advance(duration).has_value());
      ASSERT_TRUE(application_.has_value());
      ASSERT_TRUE(application_->frame().has_value());
    }

    auto activate(std::string_view key) -> bool {
      return application_.has_value() &&
             application_->root().activate(composition::ElementKey{key});
    }

    auto application() -> runtime::Application& { return *application_; }

    auto threading() -> ThreadingProvider& { return headless_.threading(); }

    auto capture() -> std::expected<Image, ProviderError> {
      return factory().gpu().capture();
    }

    auto gpu() -> std::optional<VulkanGpu>& { return gpu_; }

    auto surface() -> headless::ScriptedSurface& { return headless_.surface(); }

   private:
    auto factory() -> ProviderFactory& {
      if (factory_.has_value()) {
        return *factory_;
      }
      return headless_;
    }

    headless::HeadlessProviderFactory headless_;
    std::optional<VulkanGpu> gpu_;
    std::optional<DefaultProviderFactory> factory_;
    std::optional<runtime::Application> application_;
  };

  using AssetBytes =
      std::vector<std::pair<std::string_view, std::vector<std::byte>>>;

  static auto asset_bytes() -> const AssetBytes& {
    static const AssetBytes loaded = [] {
      AssetBytes read;
      for (const std::string_view file :
           {std::string_view{"arisa/fonts/InterVariable.ttf"},
            std::string_view{"arisa/fonts/MaterialSymbolsRounded.ttf"},
            std::string_view{"arisa/fonts/MaterialSymbolsRoundedFilled.ttf"},
            std::string_view{"arisa/themes/arisa-theme-glass.toml"}}) {
        std::ifstream stream{std::filesystem::path{ARISA_SOURCE_ASSETS} / file,
                             std::ios::binary};
        const std::vector<char> raw{std::istreambuf_iterator<char>{stream},
                                    std::istreambuf_iterator<char>{}};
        std::vector<std::byte> bytes(raw.size());
        std::ranges::transform(raw, bytes.begin(),
                               [](char value) { return std::byte(value); });
        read.emplace_back(file, std::move(bytes));
      }
      return read;
    }();
    return loaded;
  }

  static auto picture() -> const std::vector<std::byte>& {
    static const std::vector<std::byte> loaded = [] {
      std::ifstream stream{std::filesystem::path{ARISA_TEST_FIXTURES} / ".." /
                               "swiftui_reference" / "list_automatic_light.png",
                           std::ios::binary};
      const std::vector<char> raw{std::istreambuf_iterator<char>{stream},
                                  std::istreambuf_iterator<char>{}};
      std::vector<std::byte> bytes(raw.size());
      std::ranges::transform(raw, bytes.begin(),
                             [](char value) { return std::byte(value); });
      return bytes;
    }();
    return loaded;
  }

  static auto mixed() -> composition::Element {
    return components::ZStack(
        components::ZStack()
            .frame({.width = 640.0F, .height = 360.0F})
            .background(theme::Colour::hex(0xF2F2F7)),
        components::VStack(components::Text("Vulkan through the loader"),
                           components::Image("pictures/list.png")
                               .resizable()
                               .frame({.width = 300.0F, .height = 200.0F}),
                           components::ZStack()
                               .frame({.width = 180.0F, .height = 40.0F})
                               .background(theme::Colour::hex(0x3478F6))
                               .clip_shape(theme::Shape::capsule()),
                           controls::ProgressView()));
  }

  static auto pressing(Button button) -> InputFrame {
    InputFrame frame;
    frame.pads[0] = PadState{
        .connected = true, .buttons = Buttons{button}, .left = {}, .right = {}};
    return frame;
  }

  static auto idle_pad() -> InputFrame {
    InputFrame frame;
    frame.pads[0] = PadState{
        .connected = true, .buttons = Buttons{}, .left = {}, .right = {}};
    return frame;
  }

  static auto touching(PixelPoint point, TouchPhase phase) -> InputFrame {
    InputFrame frame;
    frame.touches.push_back(
        Touch{.id = finger, .position = point, .phase = phase});
    return frame;
  }

  static auto stripes() -> composition::Element {
    std::vector<composition::Element> rows;
    constexpr std::array<std::uint32_t, 4> colours{0xD03030, 0x30A040, 0x3050D0,
                                                   0xE0C020};
    for (std::size_t index = 0; index < 48; ++index) {
      rows.push_back(
          components::ZStack()
              .frame({.width = 320.0F, .height = 8.0F})
              .background(theme::Colour::hex(colours[index % colours.size()])));
    }
    return components::ScrollView(components::VStack(std::move(rows)))
        .frame({.width = 320.0F, .height = 180.0F})
        .id("stripes");
  }

  static constexpr std::uint32_t glide_rgb = 0x2050C0;
  static constexpr float travel = 160.0F;

  class Glide final : public composition::Component {
   public:
    explicit Glide(const composition::Observable<bool>& moved)
        : moved_(moved) {}

    auto operator==(const Glide& other) const -> bool {
      return &moved_.get() == &other.moved_.get();
    }

    auto body(composition::Context& context) const
        -> composition::Element override {
      const bool moved = context.read(moved_.get());
      return components::ZStack(components::ZStack()
                                    .frame({.width = 1280.0F, .height = 720.0F})
                                    .background(theme::Colour::hex(0xFFFFFF)),
                                components::VStack()
                                    .frame({.width = 40.0F, .height = 40.0F})
                                    .background(theme::Colour::hex(glide_rgb))
                                    .offset(moved ? travel : 0.0F, 0.0F));
    }

   private:
    std::reference_wrapper<const composition::Observable<bool>> moved_;
  };

  static auto leftmost(const Image& image, std::uint32_t row)
      -> std::optional<std::uint32_t> {
    for (std::uint32_t column = 0; column < image.width(); ++column) {
      const auto seen = image.pixel(column, row);
      if (seen.has_value() && (*seen)[2] > 150 && (*seen)[0] < 80) {
        return column;
      }
    }
    return std::nullopt;
  }

  struct Effect {
    std::string_view name;
    examples::GalleryPage page{examples::GalleryPage::glass};
    bool glass{false};
    std::optional<examples::LaunchScreen::Style> launch{};
    std::string_view activate{};
    int frames{4};
    bool passes{true};
    std::chrono::milliseconds step{frame_step};
  };

  static auto effects() -> std::vector<Effect> {
    return {
        Effect{.name = "glass",
               .page = examples::GalleryPage::glass,
               .glass = true},
        Effect{.name = "materials", .page = examples::GalleryPage::materials},
        Effect{.name = "materials_glass",
               .page = examples::GalleryPage::materials,
               .glass = true},
        Effect{.name = "effects",
               .page = examples::GalleryPage::effects,
               .passes = false},
        Effect{.name = "presentations_glass_sheet",
               .page = examples::GalleryPage::presentations,
               .glass = true,
               .activate = examples::PresentationsPage::sheet_button,
               .frames = 10,
               .step = std::chrono::milliseconds{80}},
        Effect{.name = "launch_glass_stroke",
               .page = examples::GalleryPage::controls,
               .launch =
                   examples::LaunchScreen::Style{
                       .ending = examples::LaunchScreen::Ending::fade,
                       .ink = examples::LaunchScreen::Ink::glass},
               .frames = 13,
               .step = std::chrono::milliseconds{50}},
        Effect{.name = "paths_clips", .page = examples::GalleryPage::paths}};
  }

  static auto gallery(const Effect& effect) -> composition::Element {
    return examples::GalleryScreen{
        effect.page, theme::Seed{.scheme = theme::ColorScheme::light},
        std::nullopt, std::nullopt, effect.launch};
  }

  class Shelf final : public composition::Component {
   public:
    Shelf(const std::vector<std::shared_ptr<const Image>>& library,
          const composition::Observable<int>& page)
        : library_(library), page_(page) {}

    auto operator==(const Shelf& other) const -> bool {
      return &library_.get() == &other.library_.get() &&
             &page_.get() == &other.page_.get();
    }

    auto body(composition::Context& context) const
        -> composition::Element override {
      const int shown = context.read(page_.get());
      const auto& library = library_.get();
      std::vector<composition::Element> tiles;
      for (std::size_t index = 0; index < 4; ++index) {
        tiles.push_back(
            components::Image(
                library[((static_cast<std::size_t>(shown) * 4) + index) %
                        library.size()])
                .interpolation(components::Image::Interpolation::low)
                .frame({.width = 32.0F, .height = 32.0F}));
      }
      return components::HStack(std::move(tiles));
    }

   private:
    std::reference_wrapper<const std::vector<std::shared_ptr<const Image>>>
        library_;
    std::reference_wrapper<const composition::Observable<int>> page_;
  };

  static auto scene() -> composition::Element {
    return components::ZStack(
        stripes(),
        components::ZStack()
            .frame({.width = 200.0F, .height = 60.0F})
            .background(theme::Colour::hex(0x202020))
            .id("panel"),
        controls::ProgressView().offset(120.0F, 60.0F).id("spinner"));
  }
};

TEST_F(VulkanSceneTest, PartialRepaintOverAnImageChainMatchesAWholeRepaint) {
  constexpr SurfaceSize extent{.width = SurfaceWidth{320},
                               .height = SurfaceHeight{180}};
  for (const std::uint32_t images : {2U, 3U}) {
    auto partial_gpu =
        offscreen({.size = extent, .images = images, .preserved = true});
    if (!partial_gpu.has_value()) {
      GTEST_SKIP() << skip_reason(partial_gpu.error());
    }
    auto whole_gpu =
        offscreen({.size = extent, .images = 1, .preserved = false});
    ASSERT_TRUE(whole_gpu.has_value());
    Run partial{extent, std::move(*partial_gpu)};
    Run whole{extent, std::move(*whole_gpu)};
    partial.start(scene(), false);
    whole.start(scene(), false);
    int mismatched = 0;
    int frames = 0;
    const auto step = [&](std::optional<InputFrame> input) {
      partial.step(input);
      whole.step(std::move(input));
      const auto seen = partial.capture();
      const auto expected = whole.capture();
      ASSERT_TRUE(seen.has_value());
      ASSERT_TRUE(expected.has_value());
      ++frames;
      if (!std::ranges::equal(seen->pixels(), expected->pixels())) {
        ++mismatched;
      }
    };
    for (int index = 0; index < 6; ++index) {
      step(std::nullopt);
    }
    constexpr int moves = 8;
    constexpr float distance = 60.0F;
    step(touching(PixelPoint{.x = 40.0F, .y = 150.0F}, TouchPhase::began));
    for (int index = 1; index <= moves; ++index) {
      step(touching(
          PixelPoint{.x = 40.0F,
                     .y = 150.0F - (distance * static_cast<float>(index) /
                                    static_cast<float>(moves))},
          TouchPhase::moved));
    }
    step(touching(PixelPoint{.x = 40.0F, .y = 150.0F - distance},
                  TouchPhase::ended));
    for (int index = 0; index < 20; ++index) {
      step(std::nullopt);
    }
    const VulkanGpu::Statistics partial_statistics =
        partial.gpu()->statistics();
    std::println("{} images: {} frames compared, {} presented, {} mismatched",
                 images, frames, partial_statistics.frames_presented,
                 mismatched);
    EXPECT_EQ(mismatched, 0) << images << " images";
    EXPECT_GT(partial_statistics.frames_presented, 10U);
  }
}

TEST_F(VulkanSceneTest, ShapesTextAndImagesMatchTheSoftwareRaster) {
  constexpr SurfaceSize extent{.width = SurfaceWidth{640},
                               .height = SurfaceHeight{360}};
  auto gpu = offscreen({.size = extent, .images = 3});
  if (!gpu.has_value()) {
    GTEST_SKIP() << skip_reason(gpu.error());
  }
  ASSERT_FALSE(picture().empty());
  Run vulkan{extent, std::move(*gpu)};
  Run reference{extent, std::nullopt};
  vulkan.add("pictures/list.png", picture());
  reference.add("pictures/list.png", picture());
  vulkan.start(mixed(), true);
  reference.start(mixed(), true);
  for (int index = 0; index < 4; ++index) {
    vulkan.step();
    reference.step();
  }
  const auto seen = vulkan.capture();
  const auto expected = reference.capture();
  ASSERT_TRUE(seen.has_value());
  ASSERT_TRUE(expected.has_value());
  const Difference difference = compare(*seen, *expected);
  std::println("shapes, text and images: {}", difference.describe());
  EXPECT_TRUE(difference.within_tolerance(
      static_cast<std::size_t>(extent.width.value()) * extent.height.value()))
      << difference.describe();
  EXPECT_GE(vulkan.gpu()->statistics().texture_bytes_uploaded,
            150U * 130U * 4U);
}

class VulkanGalleryTest
    : public VulkanSceneTest,
      public ::testing::WithParamInterface<examples::GalleryPage> {};

TEST_P(VulkanGalleryTest, PageMatchesTheSoftwareRaster) {
  constexpr SurfaceSize extent{.width = SurfaceWidth{1280},
                               .height = SurfaceHeight{720}};
  auto gpu = offscreen({.size = extent, .images = 2});
  if (!gpu.has_value()) {
    GTEST_SKIP() << skip_reason(gpu.error());
  }
  const examples::GalleryPage page = GetParam();
  Run vulkan{extent, std::move(*gpu)};
  Run reference{extent, std::nullopt};
  vulkan.start(
      examples::GalleryScreen{page,
                              theme::Seed{.scheme = theme::ColorScheme::light}},
      true);
  reference.start(
      examples::GalleryScreen{page,
                              theme::Seed{.scheme = theme::ColorScheme::light}},
      true);
  for (int index = 0; index < 4; ++index) {
    vulkan.step();
    reference.step();
  }
  const auto seen = vulkan.capture();
  const auto expected = reference.capture();
  ASSERT_TRUE(seen.has_value());
  ASSERT_TRUE(expected.has_value());
  const Difference difference = compare(*seen, *expected);
  std::println("gallery page {}: {}", static_cast<int>(page),
               difference.describe());
  EXPECT_TRUE(difference.within_tolerance(
      static_cast<std::size_t>(extent.width.value()) * extent.height.value()))
      << difference.describe();
}

INSTANTIATE_TEST_SUITE_P(Pages, VulkanGalleryTest,
                         ::testing::Values(examples::GalleryPage::controls,
                                           examples::GalleryPage::lists,
                                           examples::GalleryPage::cards,
                                           examples::GalleryPage::typography));

class VulkanEffectTest : public VulkanSceneTest,
                         public ::testing::WithParamInterface<std::size_t> {};

TEST_P(VulkanEffectTest, PageMatchesTheSoftwareRasterWithRenderTargets) {
  constexpr SurfaceSize extent{.width = SurfaceWidth{1280},
                               .height = SurfaceHeight{720}};
  {
    const Effect effect = effects().at(GetParam());
    auto gpu = offscreen({.size = extent, .images = 2});
    if (!gpu.has_value()) {
      GTEST_SKIP() << skip_reason(gpu.error());
    }
    Run vulkan{extent, std::move(*gpu)};
    Run reference{extent, std::nullopt, true};
    vulkan.start(gallery(effect), true, Look{.glass = effect.glass});
    reference.start(gallery(effect), true, Look{.glass = effect.glass});
    for (int index = 0; index < effect.frames; ++index) {
      if (index == 2 && !effect.activate.empty()) {
        ASSERT_TRUE(vulkan.activate(effect.activate)) << effect.name;
        ASSERT_TRUE(reference.activate(effect.activate)) << effect.name;
      }
      vulkan.step_by(effect.step);
      reference.step_by(effect.step);
    }
    const auto seen = vulkan.capture();
    const auto expected = reference.capture();
    ASSERT_TRUE(seen.has_value());
    ASSERT_TRUE(expected.has_value());
    const Difference difference = compare(*seen, *expected);
    const VulkanGpu::Statistics statistics = vulkan.gpu()->statistics();
    std::println("{}: {}; {} offscreen passes over {} frames", effect.name,
                 difference.describe(), statistics.offscreen_passes,
                 statistics.frames_presented);
    EXPECT_TRUE(difference.within_tolerance(
        static_cast<std::size_t>(extent.width.value()) * extent.height.value()))
        << effect.name << ": " << difference.describe();
    if (effect.passes) {
      EXPECT_GT(statistics.offscreen_passes, 0U) << effect.name;
    }
  }
}

INSTANTIATE_TEST_SUITE_P(Effects, VulkanEffectTest,
                         ::testing::Range<std::size_t>(0, 7));

TEST_F(VulkanSceneTest, PartialRepaintWithGlassOverADriftingBackground) {
  constexpr SurfaceSize extent{.width = SurfaceWidth{640},
                               .height = SurfaceHeight{360}};
  for (const std::uint32_t images : {2U, 3U}) {
    auto partial_gpu =
        offscreen({.size = extent, .images = images, .preserved = true});
    if (!partial_gpu.has_value()) {
      GTEST_SKIP() << skip_reason(partial_gpu.error());
    }
    auto whole_gpu =
        offscreen({.size = extent, .images = 1, .preserved = false});
    ASSERT_TRUE(whole_gpu.has_value());
    Run partial{extent, std::move(*partial_gpu)};
    Run whole{extent, std::move(*whole_gpu)};
    const Effect drifting{.name = "materials_glass",
                          .page = examples::GalleryPage::materials,
                          .glass = true};
    partial.start(gallery(drifting), true, Look{.glass = true});
    whole.start(gallery(drifting), true, Look{.glass = true});
    int mismatched = 0;
    int frames = 0;
    std::uint64_t changed = 0;
    std::optional<Image> previous;
    for (int index = 0; index < 48; ++index) {
      partial.step();
      whole.step();
      const auto seen = partial.capture();
      const auto expected = whole.capture();
      ASSERT_TRUE(seen.has_value());
      ASSERT_TRUE(expected.has_value());
      ++frames;
      if (!std::ranges::equal(seen->pixels(), expected->pixels())) {
        ++mismatched;
      }
      if (previous.has_value() &&
          !std::ranges::equal(previous->pixels(), seen->pixels())) {
        ++changed;
      }
      previous.emplace(*seen);
    }
    const VulkanGpu::Statistics statistics = partial.gpu()->statistics();
    std::println(
        "glass over drift, {} images: {} frames compared, {} presented, {} "
        "changed, {} offscreen passes, {} mismatched",
        images, frames, statistics.frames_presented, changed,
        statistics.offscreen_passes, mismatched);
    EXPECT_EQ(mismatched, 0) << images << " images";
    EXPECT_GT(changed, 4U);
    EXPECT_GT(statistics.offscreen_passes, 0U);
  }
}

TEST_F(VulkanSceneTest, GlassFramesReportGpuEffectsTime) {
  constexpr SurfaceSize extent{.width = SurfaceWidth{1280},
                               .height = SurfaceHeight{720}};
  auto gpu = offscreen({.size = extent, .images = 2});
  if (!gpu.has_value()) {
    GTEST_SKIP() << skip_reason(gpu.error());
  }
  if (gpu->device().capabilities().timestamp_bits == 0) {
    GTEST_SKIP() << "the device has no timestamps on its graphics queue";
  }
  Run vulkan{extent, std::move(*gpu)};
  const Effect glass{
      .name = "glass", .page = examples::GalleryPage::glass, .glass = true};
  vulkan.start(gallery(glass), true, Look{.glass = true});
  std::chrono::nanoseconds most{0};
  for (int index = 0; index < 8; ++index) {
    vulkan.step();
    const GpuTimings timings = vulkan.gpu()->timings();
    if (timings.effects.has_value()) {
      most = std::max(most, *timings.effects);
    }
  }
  const VulkanGpu::Statistics statistics = vulkan.gpu()->statistics();
  std::println(
      "glass page: longest effects time {} us, {} timestamp reads, "
      "{} stalls, {} fence waits",
      most.count() / 1000, statistics.timestamp_reads,
      statistics.timestamp_stalls, statistics.fence_waits);
  EXPECT_GT(most.count(), 0);
  EXPECT_GT(statistics.timestamp_reads, 0U);
  EXPECT_EQ(statistics.timestamp_stalls, 0U);
}

TEST_F(VulkanSceneTest, RenderThreadReplaysOnTheVulkanQueue) {
  constexpr SurfaceSize extent{.width = SurfaceWidth{1280},
                               .height = SurfaceHeight{720}};
  auto gpu = offscreen({.size = extent, .images = 3});
  if (!gpu.has_value()) {
    GTEST_SKIP() << skip_reason(gpu.error());
  }
  composition::Observable<bool> moved{false};
  Run run{extent, std::move(*gpu)};
  run.start(composition::Element{Glide{moved}}, false,
            Look{.glass = false, .render_thread = true});
  run.step();
  run.step();
  motion::with_animation(motion::Animation::linear(1.0),
                         [&moved] { moved.set(true); });
  ASSERT_TRUE(run.application().frame().has_value());
  run.step();
  constexpr std::uint32_t row = 360;
  const auto painted = run.capture();
  ASSERT_TRUE(painted.has_value());
  const std::optional<std::uint32_t> start = leftmost(*painted, row);
  ASSERT_TRUE(start.has_value());
  const std::uint64_t presented = run.gpu()->frames_presented();
  const std::uint64_t replayed = run.application().replayed_frames();
  std::uint32_t previous = *start;
  for (int index = 0; index < 6; ++index) {
    run.advance();
    run.application().step_animations();
    const auto image = run.capture();
    ASSERT_TRUE(image.has_value());
    const std::optional<std::uint32_t> now = leftmost(*image, row);
    ASSERT_TRUE(now.has_value());
    EXPECT_GE(*now, previous) << index;
    previous = *now;
  }
  const std::uint64_t replays = run.application().replayed_frames() - replayed;
  EXPECT_GE(replays, 5U);
  EXPECT_EQ(run.gpu()->frames_presented() - presented, replays);
  EXPECT_GT(previous, *start + 10);
  bool finished = false;
  {
    auto ticker = run.threading().start(
        ThreadSpec{.name = "test-ticker", .role = ThreadRole::app},
        [&run, &finished](const std::stop_token&) {
          for (int index = 0; index < 12; ++index) {
            run.advance();
            run.application().step_animations();
          }
          finished = true;
        });
    ASSERT_TRUE(ticker.has_value());
    for (int index = 0; index < 64; ++index) {
      ASSERT_TRUE(run.capture().has_value());
    }
  }
  EXPECT_TRUE(finished);
  ASSERT_TRUE(run.application().frame().has_value());
  const auto resumed = run.capture();
  ASSERT_TRUE(resumed.has_value());
  const std::optional<std::uint32_t> after = leftmost(*resumed, row);
  ASSERT_TRUE(after.has_value());
  const VulkanGpu::Statistics statistics = run.gpu()->statistics();
  std::println(
      "render thread: {} replays in the first six steps, {} in "
      "all, glide {} -> {} px, {} overlapping queue uses",
      replays, run.application().replayed_frames() - replayed, *start, *after,
      statistics.overlapping_queue_use);
  EXPECT_GE(*after, previous);
  EXPECT_EQ(statistics.overlapping_queue_use, 0U);
}

TEST_F(VulkanSceneTest, AGalleryRunAndSixHundredTexturesStayFarBelowTheLimit) {
  constexpr SurfaceSize extent{.width = SurfaceWidth{1280},
                               .height = SurfaceHeight{720}};
  auto gpu = offscreen({.size = extent, .images = 3});
  if (!gpu.has_value()) {
    GTEST_SKIP() << skip_reason(gpu.error());
  }
  Run run{extent, std::move(*gpu)};
  const Effect glass{
      .name = "glass", .page = examples::GalleryPage::glass, .glass = true};
  run.start(gallery(glass), true, Look{.glass = true});
  for (int index = 0; index < 4; ++index) {
    run.step();
  }
  for (int page = 0; page < 24; ++page) {
    run.step(pressing(Button::zl));
    run.step(idle_pad());
    for (int index = 0; index < 4; ++index) {
      run.step();
    }
  }
  std::vector<Texture> textures;
  for (std::uint32_t index = 0; index < 600; ++index) {
    auto texture = run.gpu()->create_texture(
        {.width = 8 + (index % 64), .height = 8 + (index % 32)});
    ASSERT_TRUE(texture.has_value()) << index;
    textures.push_back(std::move(*texture));
  }
  run.step();
  const VulkanDevice& device = run.gpu()->device();
  const MemoryAllocator::Statistics memory = device.memory_statistics();
  const std::uint32_t limit =
      device.properties().limits.maxMemoryAllocationCount;
  std::println(
      "gallery run plus 600 textures: {} vkAllocateMemory calls ({} live) "
      "for {} buffers and images ({} live) the provider would otherwise have "
      "allocated one by one; {} offscreen passes; device limit {}",
      memory.blocks_created, memory.blocks_live, memory.allocations_made,
      memory.allocations_live, run.gpu()->statistics().offscreen_passes, limit);
  EXPECT_GT(memory.allocations_made, 600U);
  EXPECT_LE(memory.blocks_live, 64U);
  EXPECT_LE(memory.blocks_created, 128U);
}

TEST_F(VulkanSceneTest, SweepingTheSidebarAfterRichTextKeepsEveryFrame) {
  constexpr SurfaceSize extent{.width = SurfaceWidth{1920},
                               .height = SurfaceHeight{1080}};
  auto gpu = offscreen({.size = extent, .images = 3});
  if (!gpu.has_value()) {
    GTEST_SKIP() << skip_reason(gpu.error());
  }
  Run run{extent, std::move(*gpu)};
  const Effect typography{.name = "typography",
                          .page = examples::GalleryPage::typography,
                          .glass = true};
  run.start(gallery(typography), true,
            Look{.glass = true, .render_thread = true});
  for (int index = 0; index < 20; ++index) {
    run.step();
  }
  ASSERT_TRUE(run.activate("typography-rich-text-button"));
  for (int index = 0; index < 40; ++index) {
    run.step();
  }
  ASSERT_TRUE(run.activate("rich-text-close"));
  for (int index = 0; index < 40; ++index) {
    run.step();
  }
  for (int press = 0; press < 3; ++press) {
    run.step(pressing(Button::dpad_left));
    run.step(idle_pad());
    run.step();
  }
  for (int press = 0; press < 30; ++press) {
    run.step(pressing(Button::dpad_down));
    run.step(idle_pad());
    run.step();
    run.step();
  }
  for (int press = 0; press < 30; ++press) {
    run.step(pressing(Button::dpad_up));
    run.step(idle_pad());
  }
  for (int index = 0; index < 30; ++index) {
    run.step();
  }
  EXPECT_TRUE(run.capture().has_value());
}

TEST_F(VulkanSceneTest, ReachingFidelityFromTheSidebarInHandheldKeepsDrawing) {
  constexpr SurfaceSize extent{.width = SurfaceWidth{1920},
                               .height = SurfaceHeight{1080}};
  auto gpu = offscreen({.size = extent, .images = 3});
  if (!gpu.has_value()) {
    GTEST_SKIP() << skip_reason(gpu.error());
  }
  Run run{extent, std::move(*gpu)};
  run.surface().set_scale(
      SurfaceScale::create(1.5F).value_or(SurfaceScale::unit()));
  run.surface().set_display_mode(DisplayMode::handheld);
  const std::filesystem::path folder =
      std::filesystem::path{ARISA_TEST_FIXTURES} / ".." / "swiftui_reference";
  for (const auto& entry : std::filesystem::directory_iterator{folder}) {
    if (entry.path().extension() != ".png") {
      continue;
    }
    std::ifstream stream{entry.path(), std::ios::binary};
    const std::vector<char> raw{std::istreambuf_iterator<char>{stream},
                                std::istreambuf_iterator<char>{}};
    std::vector<std::byte> bytes(raw.size());
    std::ranges::transform(raw, bytes.begin(),
                           [](char value) { return std::byte(value); });
    run.add("arisa/swiftui_reference/" + entry.path().filename().string(),
            bytes);
  }
  const Effect typography{.name = "typography",
                          .page = examples::GalleryPage::typography,
                          .glass = true};
  run.start(gallery(typography), true,
            Look{.glass = true, .render_thread = true});
  for (int index = 0; index < 30; ++index) {
    run.step();
  }
  for (int press = 0; press < 3; ++press) {
    run.step(pressing(Button::dpad_left));
    run.step(idle_pad());
  }
  for (int press = 0; press < 16; ++press) {
    run.step(pressing(Button::dpad_down));
    run.step(idle_pad());
    run.step();
  }
  for (int index = 0; index < 400; ++index) {
    run.step();
    if (index % 25 == 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds{15});
    }
  }
  EXPECT_TRUE(run.capture().has_value());
  std::println("fidelity handheld: {} bytes uploaded",
               run.gpu()->statistics().texture_bytes_uploaded);
}

TEST_F(VulkanSceneTest, SwitchingTabsUnderTheCoversCoverKeepsDrawing) {
  constexpr SurfaceSize extent{.width = SurfaceWidth{1920},
                               .height = SurfaceHeight{1080}};
  auto gpu = offscreen({.size = extent, .images = 3});
  if (!gpu.has_value()) {
    GTEST_SKIP() << skip_reason(gpu.error());
  }
  Run run{extent, std::move(*gpu)};
  run.surface().set_scale(
      SurfaceScale::create(1.5F).value_or(SurfaceScale::unit()));
  run.surface().set_display_mode(DisplayMode::handheld);
  const Effect presentations{.name = "presentations",
                             .page = examples::GalleryPage::presentations,
                             .glass = true};
  run.start(gallery(presentations), true,
            Look{.glass = true, .render_thread = true});
  for (int index = 0; index < 30; ++index) {
    run.step();
  }
  ASSERT_TRUE(run.activate(examples::PresentationsPage::covers_button));
  for (int index = 0; index < 60; ++index) {
    run.step();
    if (index % 10 == 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds{5});
    }
  }
  run.step(pressing(Button::r));
  run.step(idle_pad());
  for (int index = 0; index < 200; ++index) {
    run.step();
    if (index % 20 == 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds{10});
    }
  }
  EXPECT_TRUE(run.capture().has_value());
}

TEST_F(VulkanSceneTest,
       ThePictureBudgetReleasesVulkanTexturesOfImagesOffScreen) {
  static constexpr SurfaceSize extent{.width = SurfaceWidth{320},
                                      .height = SurfaceHeight{180}};
  constexpr std::uint32_t side = 32;
  constexpr std::uint64_t one = std::uint64_t{side} * side * 4;
  std::vector<std::shared_ptr<const Image>> library;
  for (std::uint32_t index = 0; index < 40; ++index) {
    std::vector<std::byte> rgba(
        std::size_t{side} * side * 4,
        std::byte{static_cast<std::uint8_t>(index * 6)});
    auto created = Image::create(side, side, std::move(rgba));
    ASSERT_TRUE(created.has_value());
    library.push_back(std::make_shared<const Image>(std::move(*created)));
  }
  const auto measure = [&library](std::uint64_t budget)
      -> std::optional<std::pair<std::uint64_t, std::size_t>> {
    auto gpu = offscreen({.size = extent, .images = 2});
    if (!gpu.has_value()) {
      return std::nullopt;
    }
    composition::Observable<int> page{0};
    Run run{extent, std::move(*gpu)};
    run.start(Shelf{library, page}, false,
              Look{.glass = false,
                   .render_thread = false,
                   .images = runtime::ImageOptions{.textures = budget}});
    for (int step = 0; step < 10; ++step) {
      page.set(step);
      run.step();
      run.step();
    }
    return std::pair{run.gpu()->inventory().textures.sampled_bytes,
                     run.gpu()->live_textures()};
  };
  const auto bounded = measure(4 * one);
  if (!bounded.has_value()) {
    GTEST_SKIP() << "no Vulkan device";
  }
  const auto roomy = measure(400 * one);
  ASSERT_TRUE(roomy.has_value());
  EXPECT_GE(roomy->second, bounded->second + 12U);
  EXPECT_GE(roomy->first, bounded->first + (12U * one));
}

TEST_F(VulkanSceneTest, VisualEffectsMatchTheSoftwareRaster) {
  constexpr SurfaceSize extent{.width = SurfaceWidth{640},
                               .height = SurfaceHeight{360}};
  auto gpu = offscreen({.size = extent, .images = 3});
  if (!gpu.has_value()) {
    GTEST_SKIP() << skip_reason(gpu.error());
  }
  const auto swatch = [](std::uint32_t rgb) {
    return components::VStack()
        .frame({.width = 80.0F, .height = 80.0F})
        .background(theme::Colour::hex(rgb), theme::Shape::rectangle());
  };
  const auto scene = [&swatch] {
    return components::ZStack(
        components::ZStack()
            .frame({.width = 640.0F, .height = 360.0F})
            .background(theme::Colour::hex(0xF2F2F7)),
        components::VStack(
            components::HStack(
                swatch(0xCC6633).grayscale(1.0),
                swatch(0xCC6633).hue_rotation(motion::Angle::degrees(90.0)),
                swatch(0x000000).blur(6.0F),
                swatch(0x3478F6).mask(swatch(0x000000).opacity(0.5F)),
                components::ZStack(swatch(0x3399E6),
                                   swatch(0xCC6633).blend_mode(
                                       graphics::BlendMode::multiply))),
            components::HStack(
                swatch(0x34C759).rotation_3d_effect(
                    motion::Angle::degrees(40.0),
                    {.x = 0.0F, .y = 1.0F, .z = 0.0F}),
                components::ZStack(swatch(0x000000), swatch(0x000000))
                    .compositing_group()
                    .opacity(0.5F),
                swatch(0xFF2D55).color_invert().contrast(1.4))));
  };
  Run vulkan{extent, std::move(*gpu)};
  Run reference{extent, std::nullopt, true};
  vulkan.start(scene(), false);
  reference.start(scene(), false);
  for (int index = 0; index < 4; ++index) {
    vulkan.step();
    reference.step();
  }
  const auto seen = vulkan.capture();
  const auto expected = reference.capture();
  ASSERT_TRUE(seen.has_value());
  ASSERT_TRUE(expected.has_value());
  const Difference difference = compare(*seen, *expected);
  std::println("visual effects: {}", difference.describe());
  EXPECT_TRUE(difference.within_tolerance(
      static_cast<std::size_t>(extent.width.value()) * extent.height.value()))
      << difference.describe();
  EXPECT_GT(vulkan.gpu()->statistics().offscreen_passes, 0U);
}

TEST_F(VulkanSceneTest, ShapesAndShapeStylesMatchTheSoftwareRaster) {
  constexpr SurfaceSize extent{.width = SurfaceWidth{640},
                               .height = SurfaceHeight{360}};
  auto gpu = offscreen({.size = extent, .images = 3});
  if (!gpu.has_value()) {
    GTEST_SKIP() << skip_reason(gpu.error());
  }
  const auto ramp = [] {
    return graphics::Gradient{std::vector<theme::ColourValue>{
        theme::Colour::hex(0xFF2D55), theme::Colour::hex(0x3478F6)}};
  };
  const auto block = [](theme::Shape shape, graphics::ShapeStyle style) {
    return components::VStack()
        .frame({.width = 96.0F, .height = 72.0F})
        .background(std::move(style), shape);
  };
  const auto scene = [&] {
    return components::ZStack(
        components::ZStack()
            .frame({.width = 640.0F, .height = 360.0F})
            .background(theme::Colour::hex(0xF2F2F7)),
        components::VStack(
            components::HStack(
                block(theme::Ellipse{}, theme::Colour::hex(0x5856D6)),
                block(theme::UnevenRoundedRectangle{0.0F, 12.0F, 24.0F, 36.0F},
                      theme::Colour::hex(0x30B0C7)),
                block(theme::RoundedRectangle(16.0F),
                      graphics::LinearGradient{.gradient = ramp()}),
                block(theme::Capsule{},
                      graphics::EllipticalGradient{.gradient = ramp()})),
            components::HStack(
                block(theme::RoundedRectangle(14.0F),
                      graphics::ShapeStyle{theme::Colour::hex(0xFFFFFF)}.shadow(
                          graphics::ShadowStyle::inner(
                              theme::Colour::hex(0x000000), 6.0F, 2.0F, 3.0F))),
                block(theme::Ellipse{},
                      graphics::RadialGradient{.gradient = ramp(),
                                               .end_radius = 40.0F})
                    .border(theme::Colour::hex(0x000000), 2.0F),
                block(theme::UnevenRoundedRectangle{20.0F, 0.0F, 20.0F, 0.0F},
                      graphics::AngularGradient{.gradient = ramp()}),
                components::ZStack(
                    block(theme::RoundedRectangle(24.0F),
                          theme::Colour::hex(0xE5E5EA)),
                    components::VStack()
                        .frame({.width = 72.0F, .height = 48.0F})
                        .background(theme::Colour::hex(0x34C759),
                                    theme::ConcentricRectangle{}))
                    .container_shape(theme::RoundedRectangle(24.0F)))));
  };
  Run vulkan{extent, std::move(*gpu)};
  Run reference{extent, std::nullopt, true};
  vulkan.start(scene(), false);
  reference.start(scene(), false);
  for (int index = 0; index < 4; ++index) {
    vulkan.step();
    reference.step();
  }
  const auto seen = vulkan.capture();
  const auto expected = reference.capture();
  ASSERT_TRUE(seen.has_value());
  ASSERT_TRUE(expected.has_value());
  const Difference difference = compare(*seen, *expected);
  std::println("shapes: {}", difference.describe());
  EXPECT_TRUE(difference.within_tolerance(
      static_cast<std::size_t>(extent.width.value()) * extent.height.value()))
      << difference.describe();
}

TEST_F(VulkanSceneTest, AnAppShaderEffectRunsOnTheGpu) {
  constexpr SurfaceSize extent{.width = SurfaceWidth{320},
                               .height = SurfaceHeight{180}};
  auto gpu = offscreen({.size = extent, .images = 2});
  if (!gpu.has_value()) {
    GTEST_SKIP() << skip_reason(gpu.error());
  }
  Run vulkan{extent, std::move(*gpu)};
  vulkan.start(
      components::ZStack(components::VStack()
                             .frame({.width = 192.0F, .height = 96.0F})
                             .background(theme::Colour::hex(0xFFFFFF),
                                         theme::Shape::rectangle())
                             .color_effect(graphics::Shader{
                                 examples::GalleryShaders::stripes(), {24.0F}}))
          .frame({.width = 320.0F, .height = 180.0F})
          .background(theme::Colour::hex(0x000000)),
      false);
  for (int index = 0; index < 4; ++index) {
    vulkan.step();
  }
  const auto seen = vulkan.capture();
  ASSERT_TRUE(seen.has_value());
  std::uint32_t bright = 0;
  std::uint32_t dimmed = 0;
  std::uint32_t changes = 0;
  bool was_bright = false;
  const std::uint32_t row = seen->height() / 2;
  for (std::uint32_t x = 0; x < seen->width(); ++x) {
    const auto pixel = seen->pixel(x, row);
    ASSERT_TRUE(pixel.has_value());
    const int level = (*pixel)[0];
    if (level > 220) {
      ++bright;
      changes += was_bright ? 0U : 1U;
      was_bright = true;
    } else if (level > 100 && level < 180) {
      ++dimmed;
      changes += was_bright ? 1U : 0U;
      was_bright = false;
    } else {
      was_bright = false;
    }
  }
  EXPECT_GT(bright, 0U);
  EXPECT_GT(dimmed, 0U);
  EXPECT_GE(changes, 6U);
}

TEST_F(VulkanSceneTest, CanvasCompositingAndColourMatchTheSoftwareRaster) {
  constexpr SurfaceSize extent{.width = SurfaceWidth{480},
                               .height = SurfaceHeight{200}};
  auto gpu = offscreen({.size = extent, .images = 3});
  if (!gpu.has_value()) {
    GTEST_SKIP() << skip_reason(gpu.error());
  }
  using Context = components::GraphicsContext;
  const auto drawn = [](Context::BlendMode mode) {
    return components::Canvas([mode](Context& context, composition::Size) {
             context.fill(
                 graphics::Path{composition::Rect{
                     .x = 0.0F, .y = 0.0F, .width = 40.0F, .height = 80.0F}},
                 graphics::ShapeStyle{theme::Colour::hex(0x0000FF)});
             context.set_blend_mode(mode);
             context.fill(
                 graphics::Path{composition::Rect{
                     .x = 20.0F, .y = 20.0F, .width = 40.0F, .height = 40.0F}},
                 graphics::ShapeStyle{theme::Colour{1.0F, 0.0F, 0.0F, 0.5F}});
           })
        .frame({.width = 80.0F, .height = 80.0F});
  };
  const auto scene = [&] {
    return components::ZStack(
        components::ZStack()
            .frame({.width = 480.0F, .height = 200.0F})
            .background(theme::Colour::hex(0xFFFFFF)),
        components::VStack(
            components::HStack(drawn(Context::BlendMode::source_in),
                               drawn(Context::BlendMode::destination_out),
                               drawn(Context::BlendMode::source_atop),
                               components::ShapeView(theme::Circle())
                                   .fill(theme::Colour::hex(0xFF9500))
                                   .stroke(theme::Colour::hex(0x5856D6), 6.0F)
                                   .frame({.width = 120.0F, .height = 80.0F})),
            components::HStack(
                components::ShapeView(graphics::AnyShape{theme::Rectangle()}
                                          .size(40.0F, 80.0F)
                                          .union_with(theme::Circle()))
                    .fill(theme::Colour::hex(0x30B0C7))
                    .frame({.width = 80.0F, .height = 80.0F}),
                components::ShapeView(theme::RoundedRectangle(12.0F))
                    .fill(theme::Colour::hex(0xFFFFFF))
                    .frame({.width = 160.0F, .height = 80.0F}),
                components::VStack()
                    .frame({.width = 80.0F, .height = 80.0F})
                    .background(theme::Colour::hex(0x000000),
                                theme::Shape::rectangle())
                    .colour_matrix(
                        graphics::ColourMatrix::of(
                            {0.5F, 0.0F, 0.0F, 0.0F, 0.5F, 0.0F, 0.5F,
                             0.0F, 0.0F, 0.5F, 0.0F, 0.0F, 0.5F, 0.0F,
                             0.5F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F}),
                        false, true))));
  };
  Run vulkan{extent, std::move(*gpu)};
  Run reference{extent, std::nullopt, true};
  vulkan.start(scene(), false);
  reference.start(scene(), false);
  for (int index = 0; index < 4; ++index) {
    vulkan.step();
    reference.step();
  }
  const auto seen = vulkan.capture();
  const auto expected = reference.capture();
  ASSERT_TRUE(seen.has_value());
  ASSERT_TRUE(expected.has_value());
  const Difference difference = compare(*seen, *expected);
  std::println("canvas: {}", difference.describe());
  EXPECT_TRUE(difference.within_tolerance(
      static_cast<std::size_t>(extent.width.value()) * extent.height.value()))
      << difference.describe();
}

TEST_F(VulkanSceneTest, AShaderWithArrayArgumentsRunsOnTheGpu) {
  constexpr SurfaceSize extent{.width = SurfaceWidth{320},
                               .height = SurfaceHeight{180}};
  auto gpu = offscreen({.size = extent, .images = 2});
  if (!gpu.has_value()) {
    GTEST_SKIP() << skip_reason(gpu.error());
  }
  const std::array<float, 3> weights{1.0F, 1.0F, 2.0F};
  const std::array<theme::Colour, 3> palette{theme::Colour::hex(0xFF3B30),
                                             theme::Colour::hex(0x34C759),
                                             theme::Colour::hex(0x007AFF)};
  Run vulkan{extent, std::move(*gpu)};
  vulkan.start(
      components::ZStack(
          components::VStack()
              .frame({.width = 160.0F, .height = 80.0F})
              .background(theme::Colour::hex(0xFFFFFF),
                          theme::Shape::rectangle())
              .color_effect(
                  graphics::Shader{examples::GalleryShaders::bands()}
                      .argument(
                          graphics::Shader::Argument::float_array(weights))
                      .argument(
                          graphics::Shader::Argument::color_array(palette))
                      .argument(graphics::Shader::Argument::bounding_rect())))
          .frame({.width = 320.0F, .height = 180.0F})
          .background(theme::Colour::hex(0x000000)),
      false);
  for (int index = 0; index < 4; ++index) {
    vulkan.step();
  }
  const auto seen = vulkan.capture();
  ASSERT_TRUE(seen.has_value());
  std::array<std::uint32_t, 3> widths{};
  std::array<std::uint32_t, 3> first{};
  const std::uint32_t row = seen->height() / 2;
  for (std::uint32_t x = 0; x < seen->width(); ++x) {
    const auto pixel = seen->pixel(x, row);
    ASSERT_TRUE(pixel.has_value());
    for (std::size_t band = 0; band < palette.size(); ++band) {
      const theme::Colour wanted = palette[band];
      if (std::abs((*pixel)[0] - static_cast<int>(wanted.red * 255.0F)) < 4 &&
          std::abs((*pixel)[1] - static_cast<int>(wanted.green * 255.0F)) < 4 &&
          std::abs((*pixel)[2] - static_cast<int>(wanted.blue * 255.0F)) < 4) {
        if (widths[band] == 0) {
          first[band] = x;
        }
        ++widths[band];
      }
    }
  }
  ASSERT_GT(widths[0], 0U);
  EXPECT_LT(first[0], first[1]);
  EXPECT_LT(first[1], first[2]);
  EXPECT_NEAR(static_cast<float>(widths[1]), static_cast<float>(widths[0]),
              2.0F);
  EXPECT_NEAR(static_cast<float>(widths[2]),
              2.0F * static_cast<float>(widths[0]), 3.0F);
}

}
