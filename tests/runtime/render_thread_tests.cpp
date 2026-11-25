#include <arisa/components/image.hpp>
#include <arisa/components/surface_view.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/provider/factory.hpp>
#include <arisa/provider/gpu/external_texture.hpp>
#include <arisa/provider/gpu/frame_hook.hpp>
#include <arisa/provider/gpu/gpu_surface.hpp>
#include <arisa/provider/gpu/render_queue.hpp>
#include <arisa/provider/gpu/shape_vertex.hpp>
#include <arisa/provider/gpu/texture.hpp>
#include <arisa/provider/gpu/vertex.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/headless/null_gpu.hpp>
#include <arisa/provider/headless/software_raster.hpp>
#include <arisa/provider/standard/standard_clock.hpp>
#include <arisa/provider/surface/surface_scale.hpp>
#include <arisa/provider/surface/surface_size.hpp>
#include <arisa/provider/threading/thread.hpp>
#include <arisa/provider/threading/thread_role.hpp>
#include <arisa/provider/threading/thread_spec.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/runtime/application_options.hpp>
#include <arisa/runtime/captured_frame.hpp>
#include <arisa/runtime/frame_breakdown.hpp>
#include <arisa/runtime/frame_timings.hpp>
#include <arisa/theme/colour.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <stop_token>
#include <thread>
#include <utility>
#include <vector>

namespace arisa::runtime {

using namespace std::chrono_literals;

class RenderThreadTest : public ::testing::Test {
 protected:
  static constexpr std::uint32_t mover_rgb = 0x2050C0;
  static constexpr std::uint32_t still_rgb = 0xC05020;
  static constexpr float travel = 160.0F;
  static constexpr float faded = 0.4F;

  class Stage final : public composition::Component {
   public:
    Stage(const composition::Observable<bool>& moved,
          const composition::Observable<bool>& stalled,
          std::function<void()>& stall)
        : moved_(moved), stalled_(stalled), stall_(stall) {}

    auto operator==(const Stage& other) const -> bool {
      return &moved_.get() == &other.moved_.get() &&
             &stalled_.get() == &other.stalled_.get() &&
             &stall_.get() == &other.stall_.get();
    }

    auto body(composition::Context& context) const
        -> composition::Element override {
      const bool moved = context.read(moved_.get());
      if (context.read(stalled_.get()) && stall_.get()) {
        std::exchange(stall_.get(), nullptr)();
      }
      return components::VStack(components::VStack()
                                    .frame({.width = 40.0F, .height = 40.0F})
                                    .background(theme::Colour::hex(mover_rgb))
                                    .offset(moved ? travel : 0.0F, 0.0F)
                                    .opacity(moved ? faded : 1.0F),
                                components::VStack()
                                    .frame({.width = 40.0F, .height = 40.0F})
                                    .background(theme::Colour::hex(still_rgb)));
    }

   private:
    std::reference_wrapper<const composition::Observable<bool>> moved_;
    std::reference_wrapper<const composition::Observable<bool>> stalled_;
    std::reference_wrapper<std::function<void()>> stall_;
  };

  struct Fill {
    float left{0.0F};
    std::uint32_t alpha{0};
  };

  auto start() -> Application {
    auto created =
        Application::create(providers_, Stage{moved_, stalled_, stall_});
    EXPECT_TRUE(created.has_value());
    Application application = std::move(*created);
    for (int index = 0; index < 2; ++index) {
      advance(16ms);
      EXPECT_TRUE(application.frame().has_value());
    }
    return application;
  }

  void launch(Application& application, motion::Animation animation) {
    motion::with_animation(animation, [this] { moved_.set(true); });
    EXPECT_TRUE(application.frame().has_value());
    advance(16ms);
    EXPECT_TRUE(application.frame().has_value());
  }

  void advance(std::chrono::nanoseconds step) {
    ASSERT_TRUE(providers_.clock().advance(step).has_value());
  }

  auto fill(std::uint32_t rgb) -> std::optional<Fill> {
    const std::uint32_t mask = colour(0xFFFFFF);
    std::optional<Fill> found;
    for (const provider::ShapeVertex& vertex :
         providers_.gpu().last_shapes().vertices) {
      if (vertex.kind != provider::ShapeKind::fill ||
          (vertex.colour & mask) != colour(rgb)) {
        continue;
      }
      if (!found.has_value()) {
        found = Fill{.left = std::numeric_limits<float>::max(),
                     .alpha = vertex.colour >> 24U};
      }
      found->left = std::min(found->left, vertex.position[0]);
    }
    return found;
  }

  static auto colour(std::uint32_t rgb) -> std::uint32_t {
    return provider::Vertex::rgba(static_cast<std::uint8_t>(rgb >> 16U),
                                  static_cast<std::uint8_t>(rgb >> 8U),
                                  static_cast<std::uint8_t>(rgb), 0);
  }

  auto replays_over(Application& application, int steps) -> std::uint64_t {
    const std::uint64_t before = application.replayed_frames();
    for (int index = 0; index < steps; ++index) {
      advance(16ms);
      application.step_animations();
    }
    return application.replayed_frames() - before;
  }

  provider::headless::HeadlessProviderFactory providers_;
  composition::Observable<bool> moved_{false};
  composition::Observable<bool> stalled_{false};
  std::function<void()> stall_;
};

TEST_F(RenderThreadTest, AnimationsKeepAdvancingWhileTheUiThreadIsBlocked) {
  Application application = start();
  launch(application, motion::Animation::linear(1.0));
  const std::optional<Fill> painted = fill(mover_rgb);
  const std::optional<Fill> still = fill(still_rgb);
  ASSERT_TRUE(painted.has_value());
  ASSERT_TRUE(still.has_value());
  const std::size_t presented = providers_.gpu().frames_presented();
  std::optional<Fill> previous = painted;
  for (int index = 0; index < 6; ++index) {
    advance(16ms);
    application.step_animations();
    const std::optional<Fill> now = fill(mover_rgb);
    ASSERT_TRUE(now.has_value());
    EXPECT_GE(now->left, previous->left);
    EXPECT_LE(now->alpha, previous->alpha);
    previous = now;
  }
  EXPECT_GE(application.replayed_frames(), 5U);
  EXPECT_EQ(providers_.gpu().frames_presented() - presented,
            application.replayed_frames());
  EXPECT_GT(previous->left, painted->left + 10.0F);
  EXPECT_LT(previous->alpha, painted->alpha);
  const std::optional<Fill> unmoved = fill(still_rgb);
  ASSERT_TRUE(unmoved.has_value());
  EXPECT_FLOAT_EQ(unmoved->left, still->left);
  EXPECT_EQ(unmoved->alpha, still->alpha);
  ASSERT_TRUE(application.frame().has_value());
  const std::optional<Fill> resumed = fill(mover_rgb);
  ASSERT_TRUE(resumed.has_value());
  EXPECT_NEAR(resumed->left, previous->left, 0.5F);
  EXPECT_NEAR(static_cast<double>(resumed->alpha),
              static_cast<double>(previous->alpha), 1.0);
}

TEST_F(RenderThreadTest, AnimationsAdvanceWhileABodyBlocksTheUiThread) {
  Application application = start();
  launch(application, motion::Animation::linear(1.0));
  const std::optional<Fill> painted = fill(mover_rgb);
  ASSERT_TRUE(painted.has_value());
  constexpr int blocked_frames = 6;
  std::uint64_t replayed = 0;
  std::optional<Fill> during;
  stall_ = [&] {
    {
      auto ticker = providers_.threading().start(
          provider::ThreadSpec{.name = "test-ticker",
                               .role = provider::ThreadRole::app},
          [&](const std::stop_token&) {
            for (int index = 0; index < blocked_frames; ++index) {
              static_cast<void>(providers_.clock().advance(16ms));
              application.step_animations();
            }
          });
      ASSERT_TRUE(ticker.has_value());
    }
    replayed = application.replayed_frames();
    during = fill(mover_rgb);
  };
  stalled_.set(true);
  ASSERT_TRUE(application.frame().has_value());
  EXPECT_FALSE(stall_);
  EXPECT_GE(replayed, static_cast<std::uint64_t>(blocked_frames - 1));
  ASSERT_TRUE(during.has_value());
  EXPECT_GT(during->left, painted->left + 10.0F);
  const std::optional<Fill> resumed = fill(mover_rgb);
  ASSERT_TRUE(resumed.has_value());
  EXPECT_NEAR(resumed->left, during->left, 0.5F);
}

TEST_F(RenderThreadTest, ReplaysOnlyAfterTheUiThreadMissesAFrame) {
  Application application = start();
  launch(application, motion::Animation::linear(1.0));
  EXPECT_EQ(replays_over(application, 1), 0U);
  ASSERT_TRUE(application.frame().has_value());
  EXPECT_EQ(replays_over(application, 8), 7U);
}

TEST_F(RenderThreadTest, ReplaysHonourTheAnimationFrameRate) {
  Application application = start();
  launch(application,
         motion::Animation::linear(1.0).preferred_frame_rate(30.0));
  EXPECT_EQ(replays_over(application, 8), 3U);
}

TEST_F(RenderThreadTest, SettledAnimationsStopReplaying) {
  Application application = start();
  launch(application, motion::Animation::linear(0.1));
  EXPECT_LE(replays_over(application, 20), 6U);
  EXPECT_EQ(replays_over(application, 10), 0U);
}

class RenderThreadRealTimeTest : public ::testing::Test {
 protected:
  using Canvas = provider::headless::SoftwareRaster::Canvas;

  class SlowBuild final : public provider::GpuProvider {
   public:
    explicit SlowBuild(provider::headless::NullGpu& gpu) noexcept : gpu_(gpu) {}

    void hold(std::chrono::nanoseconds duration) noexcept {
      hold_.store(duration.count());
    }

    auto held() const noexcept -> int { return held_.load(); }

    [[nodiscard]] auto create_texture(provider::TextureDescription description)
        -> std::expected<provider::Texture, provider::ProviderError> override {
      return gpu_.get().create_texture(description);
    }

    [[nodiscard]] auto update_texture(provider::Texture& texture,
                                      provider::TextureRegion region,
                                      provider::PixelRows pixels)
        -> std::expected<void, provider::ProviderError> override {
      return gpu_.get().update_texture(texture, region, pixels);
    }

    [[nodiscard]] auto begin_frame()
        -> std::expected<provider::GpuFrame, provider::ProviderError> override {
      auto frame = gpu_.get().begin_frame();
      const std::chrono::nanoseconds duration{hold_.exchange(0)};
      if (duration > std::chrono::nanoseconds{0}) {
        held_.fetch_add(1);
        std::this_thread::sleep_for(duration);
      }
      return frame;
    }

    [[nodiscard]] auto begin_replay()
        -> std::expected<provider::GpuFrame, provider::ProviderError> override {
      return gpu_.get().begin_replay();
    }

    [[nodiscard]] auto capture()
        -> std::expected<provider::Image, provider::ProviderError> override {
      return gpu_.get().capture();
    }

    auto timings() const -> provider::GpuTimings override {
      return gpu_.get().timings();
    }

    auto resources() const -> provider::GpuResources override {
      return gpu_.get().resources();
    }

    auto inventory() const -> provider::GpuInventory override {
      return gpu_.get().inventory();
    }

    auto presents_off_thread() const noexcept -> bool override { return true; }

    auto frame_schedule() noexcept -> std::optional<
        std::reference_wrapper<provider::FrameSchedule>> override {
      return gpu_.get().frame_schedule();
    }

    auto publications() const noexcept -> std::optional<
        std::reference_wrapper<const provider::TexturePublications>> override {
      return gpu_.get().publications();
    }

    auto surfaces() noexcept -> std::optional<
        std::reference_wrapper<provider::SurfaceDirectory>> override {
      return gpu_.get().surfaces();
    }

    auto render_queue() noexcept -> std::optional<
        std::reference_wrapper<provider::RenderQueue>> override {
      return gpu_.get().render_queue();
    }

   private:
    std::reference_wrapper<provider::headless::NullGpu> gpu_;
    std::atomic<std::int64_t> hold_{0};
    std::atomic<int> held_{0};
  };

  class RealTimeProviders final : public provider::ProviderFactory {
   public:
    RealTimeProviders(provider::headless::HeadlessProviderFactory& headless,
                      SlowBuild& gpu) noexcept
        : headless_(headless), gpu_(gpu) {}

    auto info() const -> provider::ProviderInfo override {
      return headless_.get().info();
    }
    auto threading() noexcept -> provider::ThreadingProvider& override {
      return headless_.get().threading();
    }
    auto clock() noexcept -> provider::ClockProvider& override {
      return clock_;
    }
    auto diagnostics() noexcept -> provider::DiagnosticsProvider& override {
      return headless_.get().diagnostics();
    }
    auto assets() noexcept -> provider::AssetProvider& override {
      return headless_.get().assets();
    }
    auto lifecycle() noexcept -> provider::LifecycleProvider& override {
      return headless_.get().lifecycle();
    }
    auto system_info() noexcept -> provider::SystemInfoProvider& override {
      return headless_.get().system_info();
    }
    auto haptics() noexcept -> provider::HapticsProvider& override {
      return headless_.get().haptics();
    }
    auto audio() noexcept -> provider::AudioProvider& override {
      return headless_.get().audio();
    }
    auto clipboard() noexcept -> provider::ClipboardProvider& override {
      return headless_.get().clipboard();
    }
    auto browser() noexcept -> provider::BrowserProvider& override {
      return headless_.get().browser();
    }
    auto fonts() noexcept -> provider::FontProvider& override {
      return headless_.get().fonts();
    }
    auto surface() noexcept -> provider::SurfaceProvider& override {
      return headless_.get().surface();
    }
    auto input() noexcept -> provider::InputProvider& override {
      return headless_.get().input();
    }
    auto gpu() noexcept -> provider::GpuProvider& override { return gpu_; }
    auto text_input() noexcept -> provider::TextInputProvider& override {
      return headless_.get().text_input();
    }

   private:
    std::reference_wrapper<provider::headless::HeadlessProviderFactory>
        headless_;
    std::reference_wrapper<SlowBuild> gpu_;
    provider::standard::StandardClock clock_;
  };

  class Scene final : public composition::Component {
   public:
    Scene(const composition::Observable<bool>& moved,
          std::optional<provider::TextureId> picture,
          const composition::Observable<bool>& stalled)
        : moved_(moved), picture_(picture), stalled_(stalled) {}

    auto operator==(const Scene& other) const -> bool {
      return &moved_.get() == &other.moved_.get() &&
             picture_ == other.picture_ &&
             &stalled_.get() == &other.stalled_.get();
    }

    auto body(composition::Context& context) const
        -> composition::Element override {
      const bool moved = context.read(moved_.get());
      if (context.read(stalled_.get())) {
        std::this_thread::sleep_for(3ms);
      }
      std::vector<composition::Element> layers;
      layers.push_back(components::VStack()
                           .frame({.width = 40.0F, .height = 40.0F})
                           .background(theme::Colour::hex(0x2050C0))
                           .offset(moved ? 120.0F : 0.0F, 0.0F));
      if (picture_.has_value()) {
        layers.push_back(components::Image::External(*picture_).frame(
            {.width = 32.0F, .height = 32.0F}));
      }
      return components::ZStack(components::ZStackOptions{}, std::move(layers));
    }

   private:
    std::reference_wrapper<const composition::Observable<bool>> moved_;
    std::optional<provider::TextureId> picture_;
    std::reference_wrapper<const composition::Observable<bool>> stalled_;
  };

  class Tinted final : public composition::Component {
   public:
    explicit Tinted(const composition::Observable<int>& shade)
        : shade_(shade) {}

    static auto red(int shade) -> int { return 16 + (shade * 9); }

    auto operator==(const Tinted& other) const -> bool {
      return &shade_.get() == &other.shade_.get();
    }

    auto body(composition::Context& context) const
        -> composition::Element override {
      const auto level =
          static_cast<std::uint32_t>(red(context.read(shade_.get())));
      return components::VStack()
          .frame({.width = 160.0F, .height = 90.0F})
          .background(theme::Colour::hex((level << 16U) | 0x40U));
    }

   private:
    std::reference_wrapper<const composition::Observable<int>> shade_;
  };

  class Surfaced final : public composition::Component {
   public:
    Surfaced(std::optional<provider::GpuSurface>& surface,
             const composition::Observable<int>& version)
        : surface_(surface), version_(version) {}

    auto operator==(const Surfaced& other) const -> bool {
      return &surface_.get() == &other.surface_.get() &&
             &version_.get() == &other.version_.get();
    }

    auto body(composition::Context& context) const
        -> composition::Element override {
      static_cast<void>(context.read(version_.get()));
      if (!surface_.get().has_value()) {
        return components::VStack().frame({.width = 24.0F, .height = 24.0F});
      }
      return composition::Element{components::SurfaceView(*surface_.get())}
          .frame({.width = 24.0F, .height = 24.0F});
    }

   private:
    std::reference_wrapper<std::optional<provider::GpuSurface>> surface_;
    std::reference_wrapper<const composition::Observable<int>> version_;
  };

  void SetUp() override {
    const provider::SurfaceSize extent{.width = provider::SurfaceWidth{160},
                                       .height = provider::SurfaceHeight{90}};
    headless_.surface().resize(extent);
    headless_.surface().set_scale(provider::SurfaceScale::unit());
    headless_.gpu().rasterize(extent);
  }

  auto start(std::optional<provider::TextureId> picture = std::nullopt)
      -> Application {
    auto created =
        Application::create(providers_, Scene{moved_, picture, stalled_});
    EXPECT_TRUE(created.has_value());
    return std::move(*created);
  }

  provider::headless::HeadlessProviderFactory headless_;
  SlowBuild slow_{headless_.gpu()};
  RealTimeProviders providers_{headless_, slow_};
  composition::Observable<bool> moved_{false};
  composition::Observable<bool> stalled_{false};
};

TEST_F(RenderThreadTest, LockstepFramesMatchTheInlinePath) {
  constexpr int frames = 24;
  struct Run {
    std::vector<FrameTimings> timings;
    std::vector<std::vector<provider::ShapeVertex>> shapes;
    std::vector<provider::Image> images;
    std::size_t presented{0};
  };
  const auto play = [](bool threaded) {
    provider::headless::HeadlessProviderFactory providers;
    const provider::SurfaceSize extent{.width = provider::SurfaceWidth{200},
                                       .height = provider::SurfaceHeight{120}};
    providers.surface().resize(extent);
    providers.surface().set_scale(provider::SurfaceScale::unit());
    providers.gpu().rasterize(extent);
    composition::Observable<bool> moved{false};
    composition::Observable<bool> stalled{false};
    std::function<void()> stall;
    auto created =
        Application::create(providers, Stage{moved, stalled, stall},
                            ApplicationOptions{.executors = std::nullopt,
                                               .fonts = {},
                                               .atlas = {},
                                               .interaction = {},
                                               .render_thread = threaded});
    EXPECT_TRUE(created.has_value());
    Application application = std::move(*created);
    EXPECT_EQ(application.render_thread(), threaded);
    Run run;
    for (int index = 0; index < frames; ++index) {
      if (index == 3) {
        motion::with_animation(motion::Animation::linear(0.2),
                               [&moved] { moved.set(true); });
      }
      static_cast<void>(providers.clock().advance(16ms));
      EXPECT_TRUE(application.frame().has_value());
      FrameTimings timings = application.last_frame();
      EXPECT_EQ(timings.threaded, threaded && timings.presented);
      timings.threaded = false;
      timings.effects_gpu.reset();
      timings.interface_gpu.reset();
      timings.hooks_gpu.reset();
      run.timings.push_back(timings);
      run.shapes.push_back(providers.gpu().last_shapes().vertices);
      if (auto captured = providers.gpu().capture(); captured.has_value()) {
        run.images.push_back(std::move(*captured));
      }
    }
    run.presented = providers.gpu().frames_presented();
    return run;
  };
  const Run inline_run = play(false);
  const Run threaded_run = play(true);
  ASSERT_EQ(inline_run.timings.size(), threaded_run.timings.size());
  for (std::size_t index = 0; index < inline_run.timings.size(); ++index) {
    EXPECT_EQ(inline_run.timings[index], threaded_run.timings[index])
        << "frame " << index << "\n"
        << FrameBreakdown::describe(inline_run.timings[index], std::nullopt)
        << "\n"
        << FrameBreakdown::describe(threaded_run.timings[index], std::nullopt);
  }
  EXPECT_EQ(inline_run.shapes, threaded_run.shapes);
  ASSERT_EQ(inline_run.images.size(), threaded_run.images.size());
  for (std::size_t index = 0; index < inline_run.images.size(); ++index) {
    EXPECT_TRUE(std::ranges::equal(inline_run.images[index].pixels(),
                                   threaded_run.images[index].pixels()))
        << "frame " << index;
  }
  EXPECT_EQ(inline_run.presented, threaded_run.presented);
  EXPECT_GT(threaded_run.presented, 10U);
}

TEST_F(RenderThreadTest, ReplaysAndFramesShareOneSubmittingThread) {
  std::mutex guard;
  std::vector<provider::ThreadId> seen;
  std::atomic<int> inside{0};
  std::atomic<int> overlapped{0};
  const provider::FrameHook hook = providers_.gpu().add_frame_hook(
      provider::FrameStage::before_ui,
      [&](provider::headless::NullGpu::FrameCanvas,
          const provider::FrameInfo&) {
        if (inside.fetch_add(1) != 0) {
          overlapped.fetch_add(1);
        }
        {
          const std::scoped_lock lock{guard};
          seen.push_back(providers_.threading().current_thread());
        }
        inside.fetch_sub(1);
      });
  Application application = start();
  launch(application, motion::Animation::linear(1.0));
  static_cast<void>(replays_over(application, 6));
  ASSERT_TRUE(application.frame().has_value());
  EXPECT_GE(application.replayed_frames(), 4U);
  const provider::ThreadId ui = providers_.threading().current_thread();
  const std::scoped_lock lock{guard};
  ASSERT_FALSE(seen.empty());
  EXPECT_EQ(seen.size(), providers_.gpu().frames_presented());
  EXPECT_TRUE(std::ranges::all_of(
      seen, [&](provider::ThreadId id) { return id == seen.front(); }));
  EXPECT_NE(seen.front(), ui);
  EXPECT_EQ(overlapped.load(), 0);
}

TEST_F(RenderThreadRealTimeTest, ASlowGpuMakesTheUiWaitAndDropsNothing) {
  constexpr int frames = 40;
  static constexpr std::chrono::milliseconds slow{4};
  const provider::FrameHook hook = headless_.gpu().add_frame_hook(
      provider::FrameStage::after_ui,
      [](provider::headless::NullGpu::FrameCanvas, const provider::FrameInfo&) {
        std::this_thread::sleep_for(slow);
      },
      provider::FrameHookOptions{.drive = provider::FrameDrive::continuous,
                                 .frame_rate = std::nullopt});
  Application application = start();
  ASSERT_TRUE(application.render_thread());
  std::size_t presented = 0;
  std::chrono::nanoseconds stalled{0};
  for (int index = 0; index < frames; ++index) {
    ASSERT_TRUE(application.frame().has_value());
    const FrameTimings& timings = application.last_frame();
    if (timings.presented) {
      ++presented;
      EXPECT_TRUE(timings.threaded);
      stalled += timings.stall;
    }
  }
  application.drain();
  EXPECT_EQ(presented, static_cast<std::size_t>(frames));
  EXPECT_EQ(headless_.gpu().frames_presented(),
            presented + application.replayed_frames());
  EXPECT_EQ(headless_.gpu().frames_discarded(), 0U);
  EXPECT_GT(stalled, slow * (frames / 2));
}

TEST_F(RenderThreadRealTimeTest, RetiredTexturesOutliveThePacketsThatUseThem) {
  constexpr int frames = 30;
  std::atomic<int> released{0};
  auto canvas = std::make_shared<Canvas>(16, 16);
  canvas->fill(provider::Colour{.red = 1.0F, .alpha = 1.0F});
  auto imported = headless_.gpu().import_texture(
      canvas, provider::ImportDescription{.width = 16, .height = 16},
      support::ScopedCallback{[&released]() noexcept { ++released; }});
  ASSERT_TRUE(imported.has_value());
  std::optional<provider::headless::NullGpu::Imported> picture{
      std::move(*imported)};
  const std::size_t textures = headless_.gpu().live_textures();
  Application application = start(picture->id());
  for (int index = 0; index < frames; ++index) {
    picture->publish();
    ASSERT_TRUE(application.frame().has_value());
  }
  picture.reset();
  EXPECT_LE(released.load(), 1);
  for (int index = 0; index < 4; ++index) {
    ASSERT_TRUE(application.frame().has_value());
  }
  application.drain();
  EXPECT_EQ(released.load(), 1);
  EXPECT_LT(headless_.gpu().live_textures(), textures + 1);
}

TEST_F(RenderThreadRealTimeTest, SurfacesComeAndGoWhileFramesAreInFlight) {
  constexpr int rounds = 12;
  std::atomic<int> calls{0};
  std::optional<provider::GpuSurface> surface;
  composition::Observable<int> version{0};
  auto created = Application::create(providers_, Surfaced{surface, version});
  ASSERT_TRUE(created.has_value());
  Application application = std::move(*created);
  for (int round = 0; round < rounds; ++round) {
    auto added = headless_.gpu().add_surface(
        provider::SurfaceOptions{.mode = provider::SurfaceMode::texture,
                                 .drive = provider::FrameDrive::continuous,
                                 .frame_rate = std::nullopt},
        [&calls](provider::headless::NullGpu::FrameCanvas,
                 const provider::SurfaceFrame&) { ++calls; });
    ASSERT_TRUE(added.has_value());
    surface.emplace(std::move(*added));
    version.set(version.get() + 1);
    for (int index = 0; index < 3; ++index) {
      ASSERT_TRUE(application.frame().has_value());
    }
    std::optional<provider::GpuSurface> leaving = std::move(surface);
    surface.reset();
    version.set(version.get() + 1);
    ASSERT_TRUE(application.frame().has_value());
    leaving.reset();
    const int settled = calls.load();
    ASSERT_TRUE(application.frame().has_value());
    application.drain();
    EXPECT_EQ(calls.load(), settled);
  }
  EXPECT_GT(calls.load(), 0);
}

TEST_F(RenderThreadRealTimeTest, PacketsReplaysAndPublicationsRunTogether) {
  constexpr int frames = 120;
  auto canvas = std::make_shared<Canvas>(8, 8);
  auto imported = headless_.gpu().import_texture(
      canvas, provider::ImportDescription{.width = 8, .height = 8});
  ASSERT_TRUE(imported.has_value());
  const provider::headless::NullGpu::Imported picture = std::move(*imported);
  Application application = start(picture.id());
  std::atomic<bool> running{true};
  std::atomic<int> published{0};
  std::optional<provider::Thread> importer;
  auto started = headless_.threading().start(
      provider::ThreadSpec{.name = "test-importer",
                           .role = provider::ThreadRole::app},
      [&](const std::stop_token& stop) {
        while (!stop.stop_requested() && running.load()) {
          picture.publish();
          auto spare = headless_.gpu().import_texture(
              std::make_shared<Canvas>(4, 4),
              provider::ImportDescription{.width = 4, .height = 4});
          if (spare.has_value()) {
            spare->publish();
          }
          published.fetch_add(1);
          std::this_thread::sleep_for(200us);
        }
      });
  ASSERT_TRUE(started.has_value());
  importer.emplace(std::move(*started));
  std::size_t presented = 0;
  for (int index = 0; index < frames; ++index) {
    if (index % 20 == 0) {
      motion::with_animation(motion::Animation::linear(0.3),
                             [this] { moved_.set(!moved_.get()); });
    }
    stalled_.set(index % 9 == 4);
    ASSERT_TRUE(application.frame().has_value());
    presented += application.last_frame().presented ? 1U : 0U;
    if (index % 7 == 0) {
      std::this_thread::sleep_for(1ms);
    }
  }
  running.store(false);
  importer->request_stop();
  importer.reset();
  application.drain();
  EXPECT_GT(published.load(), 0);
  EXPECT_EQ(headless_.gpu().frames_presented(),
            presented + application.replayed_frames());
  EXPECT_EQ(headless_.gpu().frames_discarded(), 0U);
}

TEST_F(RenderThreadRealTimeTest, ReplaysContinueWhileTheUiThreadHoldsItsBuild) {
  static constexpr std::chrono::milliseconds held{150};
  Application application = start();
  ASSERT_TRUE(application.render_thread());
  for (int index = 0; index < 3; ++index) {
    ASSERT_TRUE(application.frame().has_value());
    std::this_thread::sleep_for(16ms);
  }
  motion::with_animation(motion::Animation::linear(2.0),
                         [this] { moved_.set(true); });
  ASSERT_TRUE(application.frame().has_value());
  std::this_thread::sleep_for(8ms);
  ASSERT_TRUE(application.frame().has_value());
  const std::uint64_t before = application.replayed_frames();
  const std::size_t presented = headless_.gpu().frames_presented();
  slow_.hold(held);
  ASSERT_TRUE(application.frame().has_value());
  EXPECT_EQ(slow_.held(), 1);
  const std::uint64_t replayed = application.replayed_frames() - before;
  EXPECT_GE(replayed, 4U);
  application.drain();
  EXPECT_GE(headless_.gpu().frames_presented() - presented, replayed + 1);
  EXPECT_EQ(headless_.gpu().frames_discarded(), 0U);
}

TEST_F(RenderThreadRealTimeTest, HeldBuildsReplaysAndPublicationsRunTogether) {
  constexpr int frames = 90;
  auto canvas = std::make_shared<Canvas>(8, 8);
  auto imported = headless_.gpu().import_texture(
      canvas, provider::ImportDescription{.width = 8, .height = 8});
  ASSERT_TRUE(imported.has_value());
  const provider::headless::NullGpu::Imported picture = std::move(*imported);
  Application application = start(picture.id());
  std::atomic<bool> running{true};
  std::atomic<int> published{0};
  std::optional<provider::Thread> importer;
  auto started = headless_.threading().start(
      provider::ThreadSpec{.name = "test-importer",
                           .role = provider::ThreadRole::app},
      [&](const std::stop_token& stop) {
        while (!stop.stop_requested() && running.load()) {
          picture.publish();
          published.fetch_add(1);
          std::this_thread::sleep_for(300us);
        }
      });
  ASSERT_TRUE(started.has_value());
  importer.emplace(std::move(*started));
  std::size_t presented = 0;
  for (int index = 0; index < frames; ++index) {
    if (index % 15 == 0) {
      motion::with_animation(motion::Animation::linear(0.4),
                             [this] { moved_.set(!moved_.get()); });
    }
    if (index % 6 == 3) {
      slow_.hold(std::chrono::milliseconds{10 + (index % 4) * 10});
    }
    ASSERT_TRUE(application.frame().has_value());
    presented += application.last_frame().presented ? 1U : 0U;
  }
  running.store(false);
  importer->request_stop();
  importer.reset();
  std::this_thread::sleep_for(500ms);
  application.drain();
  EXPECT_GT(published.load(), 0);
  EXPECT_GT(slow_.held(), 0);
  EXPECT_GT(application.replayed_frames(), 0U);
  EXPECT_EQ(headless_.gpu().frames_presented(),
            presented + application.replayed_frames());
  EXPECT_EQ(headless_.gpu().frames_discarded(), 0U);
}

TEST_F(RenderThreadRealTimeTest, CaptureReturnsExactlyTheRequestedFrame) {
  constexpr int frames = 24;
  constexpr int requested = 11;
  composition::Observable<int> shade{0};
  auto created = Application::create(providers_, Tinted{shade});
  ASSERT_TRUE(created.has_value());
  Application application = std::move(*created);
  ASSERT_TRUE(application.render_thread());
  for (int index = 0; index < frames; ++index) {
    shade.set(index);
    if (index == requested) {
      application.request_capture();
    }
    ASSERT_TRUE(application.frame().has_value());
  }
  application.drain();
  const std::optional<CapturedFrame> captured = application.take_capture();
  ASSERT_TRUE(captured.has_value());
  const auto centre = captured->image.pixel(80, 45);
  ASSERT_TRUE(centre.has_value());
  EXPECT_NEAR(static_cast<int>((*centre)[0]), Tinted::red(requested), 2);
  EXPECT_FALSE(application.take_capture().has_value());
  EXPECT_EQ(headless_.gpu().captures(), 1U);
}

TEST_F(RenderThreadRealTimeTest, LeasedQueueNeverOverlapsArisaSubmissions) {
  constexpr int frames = 90;
  std::atomic<bool> leasing{false};
  std::atomic<int> overlaps{0};
  std::atomic<int> submissions{0};
  const provider::FrameHook hook = headless_.gpu().add_frame_hook(
      provider::FrameStage::before_ui,
      [&](provider::headless::NullGpu::FrameCanvas,
          const provider::FrameInfo&) {
        submissions.fetch_add(1);
        if (leasing.load()) {
          overlaps.fetch_add(1);
        }
      },
      provider::FrameHookOptions{.drive = provider::FrameDrive::continuous,
                                 .frame_rate = std::nullopt});
  Application application = start();
  const auto queue = headless_.gpu().render_queue();
  ASSERT_TRUE(queue.has_value());
  std::atomic<bool> running{true};
  std::atomic<int> leases{0};
  std::optional<provider::Thread> worker;
  auto started = headless_.threading().start(
      provider::ThreadSpec{.name = "test-leaser",
                           .role = provider::ThreadRole::app},
      [&](const std::stop_token& stop) {
        while (!stop.stop_requested() && running.load()) {
          provider::RenderQueue::Lease lease = queue->get().lease();
          leasing.store(true);
          std::this_thread::sleep_for(300us);
          leasing.store(false);
          lease.release();
          leases.fetch_add(1);
          std::this_thread::sleep_for(100us);
        }
      });
  ASSERT_TRUE(started.has_value());
  worker.emplace(std::move(*started));
  for (int index = 0; index < frames; ++index) {
    if (index % 30 == 0) {
      motion::with_animation(motion::Animation::linear(0.3),
                             [this] { moved_.set(!moved_.get()); });
    }
    ASSERT_TRUE(application.frame().has_value());
  }
  running.store(false);
  worker->request_stop();
  worker.reset();
  application.drain();
  EXPECT_EQ(overlaps.load(), 0);
  EXPECT_GT(leases.load(), 10);
  EXPECT_GE(submissions.load(), frames);
  const provider::RenderQueue::Counters counted = queue->get().counters();
  EXPECT_EQ(counted.overlaps, 0U);
  EXPECT_GE(counted.submissions, static_cast<std::uint64_t>(frames));
  EXPECT_EQ(counted.leases, static_cast<std::uint64_t>(leases.load()));
}

}
