#include <arisa/components/scroll_view.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/surface_view.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/provider/default_factory.hpp>
#include <arisa/provider/gpu/gpu_surface.hpp>
#include <arisa/provider/gpu/surface_frame.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/provider.hpp>
#include <arisa/provider/providers.hpp>
#include <arisa/provider/sdl3/sdl3_event_pump.hpp>
#include <arisa/provider/sdl3/sdl3_gpu.hpp>
#include <arisa/provider/sdl3/sdl3_offscreen_surface.hpp>
#include <arisa/provider/surface/surface_scale.hpp>
#include <arisa/provider/surface/surface_size.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/runtime/application_options.hpp>
#include <arisa/theme/colour.hpp>

#include "sdl3_test.hpp"

#include <SDL3/SDL_gpu.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <utility>

namespace arisa::provider::sdl3 {

using namespace std::chrono_literals;

class Sdl3SurfaceTest : public Sdl3Test {
 protected:
  enum class Scene : std::uint8_t { bare, glass, flat, backdrop, scrolling };

  static constexpr SurfaceSize stage_size{.width = SurfaceWidth{240},
                                          .height = SurfaceHeight{120}};
  static constexpr float view_width = 64.0F;
  static constexpr float view_height = 48.0F;
  static constexpr ClipRect view{.x = 88, .y = 36, .width = 64, .height = 48};
  static constexpr std::array<std::uint8_t, 4> painted{230, 26, 51, 255};
  static constexpr std::uint32_t backdrop_rgb = 0x2050C0;
  static constexpr std::array<std::uint8_t, 4> backdrop{0x20, 0x50, 0xC0, 255};
  static constexpr int settle_limit = 60;

  class Recorder {
   public:
    void record(const SurfaceFrame& frame) {
      const std::scoped_lock lock{mutex_};
      ++calls_;
      last_ = frame;
    }

    auto calls() const -> int {
      const std::scoped_lock lock{mutex_};
      return calls_;
    }

    auto last() const -> SurfaceFrame {
      const std::scoped_lock lock{mutex_};
      return last_;
    }

    void paint(bool enabled) {
      const std::scoped_lock lock{mutex_};
      paints_ = enabled;
    }

    auto paints() const -> bool {
      const std::scoped_lock lock{mutex_};
      return paints_;
    }

   private:
    mutable std::mutex mutex_;
    int calls_{0};
    SurfaceFrame last_{};
    bool paints_{true};
  };

  class Swatch {
   public:
    Swatch(SDL_GPUDevice* device, std::array<std::uint8_t, 4> colour)
        : device_(device) {
      SDL_GPUTextureCreateInfo info{};
      info.type = SDL_GPU_TEXTURETYPE_2D;
      info.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
      info.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
      info.width = 1;
      info.height = 1;
      info.layer_count_or_depth = 1;
      info.num_levels = 1;
      texture_ = SDL_CreateGPUTexture(device, &info);
      SDL_GPUTransferBufferCreateInfo buffer_info{};
      buffer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
      buffer_info.size = 4;
      SDL_GPUTransferBuffer* const buffer =
          SDL_CreateGPUTransferBuffer(device, &buffer_info);
      void* const mapped = SDL_MapGPUTransferBuffer(device, buffer, false);
      std::memcpy(mapped, colour.data(), colour.size());
      SDL_UnmapGPUTransferBuffer(device, buffer);
      SDL_GPUCommandBuffer* const commands =
          SDL_AcquireGPUCommandBuffer(device);
      SDL_GPUCopyPass* const pass = SDL_BeginGPUCopyPass(commands);
      SDL_GPUTextureTransferInfo source{};
      source.transfer_buffer = buffer;
      SDL_GPUTextureRegion destination{};
      destination.texture = texture_;
      destination.w = 1;
      destination.h = 1;
      destination.d = 1;
      SDL_UploadToGPUTexture(pass, &source, &destination, false);
      SDL_EndGPUCopyPass(pass);
      static_cast<void>(SDL_SubmitGPUCommandBuffer(commands));
      SDL_ReleaseGPUTransferBuffer(device, buffer);
    }

    Swatch(const Swatch&) = delete;
    auto operator=(const Swatch&) -> Swatch& = delete;
    Swatch(Swatch&&) = delete;
    auto operator=(Swatch&&) -> Swatch& = delete;

    ~Swatch() { SDL_ReleaseGPUTexture(device_, texture_); }

    void fill(SDL_GPUCommandBuffer* commands, SDL_GPUTexture* target,
              ClipRect area) const {
      SDL_GPUBlitInfo info{};
      info.source.texture = texture_;
      info.source.w = 1;
      info.source.h = 1;
      info.destination.texture = target;
      info.destination.x = area.x;
      info.destination.y = area.y;
      info.destination.w = area.width;
      info.destination.h = area.height;
      info.load_op = SDL_GPU_LOADOP_LOAD;
      info.filter = SDL_GPU_FILTER_NEAREST;
      SDL_BlitGPUTexture(commands, &info);
    }

   private:
    SDL_GPUDevice* device_;
    SDL_GPUTexture* texture_{nullptr};
  };

  struct Controls {
    composition::Observable<bool> shown{true};
    composition::Observable<float> width{view_width};
    composition::Observable<bool> away{false};
  };

  class SurfaceScene final : public composition::Component {
   public:
    SurfaceScene(
        std::optional<std::reference_wrapper<const GpuSurface>> surface,
        Scene scene, const Controls& controls) noexcept
        : surface_(surface), scene_(scene), controls_(controls) {}

    auto operator==(const SurfaceScene& other) const -> bool {
      return scene_ == other.scene_ &&
             &controls_.get() == &other.controls_.get();
    }

    auto body(composition::Context& context) const
        -> composition::Element override {
      const Controls& controls = controls_;
      const bool shown = context.read(controls.shown);
      const float wide = context.read(controls.width);
      const bool away = context.read(controls.away);
      const composition::FrameSpec size{.width = wide, .height = view_height};
      composition::Element shown_view =
          !surface_.has_value()
              ? composition::Element{components::VStack()
                                         .frame(size)
                                         .background(
                                             theme::Colour::hex(0xE61A33))}
          : shown
              ? composition::Element{components::SurfaceView(surface_->get())
                                         .frame(size)}
              : composition::Element{components::VStack().frame(size)};
      if (away) {
        shown_view = std::move(shown_view).offset(1000.0F, 0.0F);
      }
      switch (scene_) {
        case Scene::glass:
        case Scene::flat:
          return components::ZStack(
              std::move(shown_view),
              components::VStack()
                  .frame({.width = 96.0F, .height = 24.0F})
                  .glass_effect());
        case Scene::backdrop:
          return components::ZStack(
              components::VStack()
                  .frame({.width = 160.0F, .height = 100.0F})
                  .background(theme::Colour::hex(backdrop_rgb)),
              std::move(shown_view));
        case Scene::scrolling:
          return components::ScrollView(
                     components::VStack(
                         components::Spacer().frame({.height = 60.0F}),
                         std::move(shown_view),
                         components::Spacer().frame({.height = 400.0F}))
                         .frame({.max_width = composition::infinity}))
              .frame({.width = 200.0F, .height = 100.0F});
        case Scene::bare:
          break;
      }
      return components::ZStack(std::move(shown_view));
    }

   private:
    std::optional<std::reference_wrapper<const GpuSurface>> surface_;
    Scene scene_;
    std::reference_wrapper<const Controls> controls_;
  };

  class Rig {
   public:
    static auto create(Scene scene, SurfaceOptions options,
                       SurfaceScale scale = SurfaceScale::unit())
        -> std::unique_ptr<Rig> {
      auto rig = std::make_unique<Rig>();
      if (!rig->open(scene, options, scale)) {
        return nullptr;
      }
      return rig;
    }

    Rig() = default;
    Rig(const Rig&) = delete;
    auto operator=(const Rig&) -> Rig& = delete;
    Rig(Rig&&) = delete;
    auto operator=(Rig&&) -> Rig& = delete;
    ~Rig() = default;

    void frame() {
      ASSERT_TRUE(headless_.clock().advance(16ms).has_value());
      ASSERT_TRUE(application_->frame().has_value());
      if (application_->last_frame().presented) {
        ++presented_;
      }
    }

    void frames(int count) {
      for (int index = 0; index < count; ++index) {
        frame();
      }
    }

    void settle() {
      for (int index = 0; index < settle_limit; ++index) {
        frame();
        if (!application_->last_frame().presented) {
          return;
        }
      }
      ADD_FAILURE() << "the stage never went idle";
    }

    void touch(PixelPoint point, TouchPhase phase) {
      InputFrame input;
      input.touches.push_back(
          Touch{.id = TouchId{3}, .position = point, .phase = phase});
      headless_.input().push(input);
      frame();
    }

    auto colour(std::uint32_t x, std::uint32_t y)
        -> std::array<std::uint8_t, 4> {
      const auto image = gpu_->capture();
      EXPECT_TRUE(image.has_value());
      if (!image.has_value()) {
        return {};
      }
      return image->pixel(x, y).value_or(std::array<std::uint8_t, 4>{});
    }

    auto top_of_paint(std::uint32_t x) -> std::optional<std::uint32_t> {
      const auto image = gpu_->capture();
      if (!image.has_value()) {
        return std::nullopt;
      }
      for (std::uint32_t y = 0; y < image->height(); ++y) {
        const auto found = image->pixel(x, y);
        if (found.has_value() && near(*found, painted, 2)) {
          return y;
        }
      }
      return std::nullopt;
    }

    auto settles_to(std::size_t live) -> bool {
      const auto deadline = std::chrono::steady_clock::now() + 5s;
      while (gpu_->live_textures() != live &&
             std::chrono::steady_clock::now() < deadline) {
        frame();
        std::this_thread::yield();
      }
      return gpu_->live_textures() == live;
    }

    auto gpu() -> Sdl3Gpu& { return *gpu_; }
    auto application() -> runtime::Application& { return *application_; }
    auto recorder() -> Recorder& { return recorder_; }
    auto controls() -> Controls& { return controls_; }
    auto surface() -> GpuSurface& { return *surface_; }
    auto presented() const -> int { return presented_; }

   private:
    auto open(Scene scene, SurfaceOptions options, SurfaceScale scale) -> bool {
      auto pump = Sdl3EventPump::create(headless_.threading());
      if (!pump.has_value()) {
        return false;
      }
      pump_.emplace(std::move(*pump));
      auto offscreen = Sdl3OffscreenSurface::create(*pump_, stage_size);
      if (!offscreen.has_value()) {
        return false;
      }
      offscreen_.emplace(std::move(*offscreen));
      auto gpu = Sdl3Gpu::create(*offscreen_);
      if (!gpu.has_value()) {
        return false;
      }
      gpu_.emplace(std::move(*gpu));
      swatch_.emplace(gpu_->device(), painted);
      headless_.surface().resize(stage_size);
      headless_.surface().set_scale(scale);
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
      if (scene != Scene::flat) {
        auto made = gpu_->add_surface(
            options, [this](const Sdl3Gpu::FrameTarget& target,
                            const SurfaceFrame& frame) {
              if (recorder_.paints()) {
                swatch_->fill(target.commands, target.texture, frame.scissor);
              }
              recorder_.record(frame);
            });
        if (!made.has_value()) {
          return false;
        }
        surface_.emplace(std::move(*made));
      }
      std::optional<std::reference_wrapper<const GpuSurface>> shown;
      if (surface_.has_value()) {
        shown = std::cref(*surface_);
      }
      auto created = runtime::Application::create(
          *factory_, SurfaceScene{shown, scene, controls_});
      if (!created.has_value()) {
        return false;
      }
      application_.emplace(std::move(*created));
      return true;
    }

    headless::HeadlessProviderFactory headless_;
    std::optional<Sdl3EventPump> pump_;
    std::optional<Sdl3OffscreenSurface> offscreen_;
    std::optional<Sdl3Gpu> gpu_;
    std::optional<Swatch> swatch_;
    std::optional<DefaultProviderFactory> factory_;
    Recorder recorder_;
    Controls controls_;
    std::optional<GpuSurface> surface_;
    std::optional<runtime::Application> application_;
    int presented_{0};
  };

  static auto near(std::array<std::uint8_t, 4> left,
                   std::array<std::uint8_t, 4> right, int tolerance) -> bool {
    for (std::size_t channel = 0; channel < 3; ++channel) {
      if (std::abs(static_cast<int>(left[channel]) -
                   static_cast<int>(right[channel])) > tolerance) {
        return false;
      }
    }
    return true;
  }

  static constexpr SurfaceOptions on_demand{.mode =
                                                provider::SurfaceMode::texture};
  static constexpr SurfaceOptions continuous{
      .mode = provider::SurfaceMode::texture, .drive = FrameDrive::continuous};
  static constexpr SurfaceOptions underlay{.mode =
                                               provider::SurfaceMode::underlay};
};

TEST_F(Sdl3SurfaceTest,
       TextureSurfacesRunPerFrameOrOnRequestAsPaintOnlyFrames) {
  auto rig = Rig::create(Scene::glass, continuous);
  if (rig == nullptr) {
    GTEST_SKIP() << "no SDL_GPU device under the offscreen driver";
  }
  rig->frames(3);
  const int presented = rig->presented();
  const int calls = rig->recorder().calls();
  rig->frames(10);
  EXPECT_EQ(rig->presented() - presented, 10);
  EXPECT_EQ(rig->recorder().calls() - calls, 10);
  EXPECT_EQ(rig->colour(view.x + 4, view.y + 4), painted);
  rig->surface().set_drive(FrameDrive::on_demand);
  rig->settle();
  const int idle = rig->recorder().calls();
  rig->surface().request_frame();
  rig->frame();
  const runtime::FrameTimings& timings = rig->application().last_frame();
  EXPECT_TRUE(timings.presented);
  EXPECT_EQ(timings.evaluations, 0U);
  EXPECT_EQ(timings.uploaded_vertices, 0U);
  EXPECT_EQ(rig->recorder().calls() - idle, 1);
  rig->frame();
  EXPECT_FALSE(rig->application().last_frame().presented);
}

TEST_F(Sdl3SurfaceTest, TheTargetIsTheLaidOutPixelRectAtEveryScale) {
  for (const float factor : {1.0F, 1.25F}) {
    auto rig = Rig::create(
        Scene::bare, on_demand,
        SurfaceScale::create(factor).value_or(SurfaceScale::unit()));
    if (rig == nullptr) {
      GTEST_SKIP() << "no SDL_GPU device under the offscreen driver";
    }
    rig->settle();
    const SurfaceFrame frame = rig->recorder().last();
    EXPECT_EQ(frame.frame.extent.width.value(),
              static_cast<std::uint32_t>(std::lround(view_width * factor)));
    EXPECT_EQ(frame.frame.extent.height.value(),
              static_cast<std::uint32_t>(std::lround(view_height * factor)));
  }
}

TEST_F(Sdl3SurfaceTest, ResizesRemovalAndCullingFollowTheView) {
  auto rig = Rig::create(Scene::bare, continuous);
  if (rig == nullptr) {
    GTEST_SKIP() << "no SDL_GPU device under the offscreen driver";
  }
  rig->frames(4);
  const std::size_t live = rig->gpu().live_textures();
  rig->controls().width.set(96.0F);
  rig->frames(4);
  EXPECT_EQ(rig->recorder().last().frame.extent.width.value(), 96U);
  EXPECT_TRUE(rig->settles_to(live));
  EXPECT_EQ(rig->colour(view.x - 14, view.y + 4), painted);
  rig->controls().away.set(true);
  rig->frame();
  const int culled = rig->recorder().calls();
  rig->frames(4);
  EXPECT_EQ(rig->recorder().calls(), culled);
  rig->controls().away.set(false);
  rig->frames(2);
  EXPECT_GT(rig->recorder().calls(), culled);
  rig->controls().shown.set(false);
  rig->frame();
  const int removed = rig->recorder().calls();
  rig->frames(4);
  EXPECT_EQ(rig->recorder().calls(), removed);
  EXPECT_TRUE(rig->settles_to(live - 1));
  EXPECT_EQ(rig->recorder().calls(), removed);
}

TEST_F(Sdl3SurfaceTest, ContentClipsInsideAScrollViewAndScrollsWithIt) {
  auto rig = Rig::create(Scene::scrolling, on_demand);
  if (rig == nullptr) {
    GTEST_SKIP() << "no SDL_GPU device under the offscreen driver";
  }
  rig->settle();
  EXPECT_EQ(rig->top_of_paint(120), std::optional<std::uint32_t>{70U});
  EXPECT_FALSE(near(rig->colour(120, 111), painted, 40));
  rig->touch({.x = 120.0F, .y = 100.0F}, TouchPhase::began);
  for (int step = 1; step <= 6; ++step) {
    rig->touch({.x = 120.0F, .y = 100.0F - (static_cast<float>(step) * 8.0F)},
               TouchPhase::moved);
  }
  for (int hold = 0; hold < 6; ++hold) {
    rig->touch({.x = 120.0F, .y = 52.0F}, TouchPhase::moved);
  }
  rig->touch({.x = 120.0F, .y = 52.0F}, TouchPhase::ended);
  rig->settle();
  EXPECT_EQ(rig->recorder().calls(), 1);
  const auto top = rig->top_of_paint(120);
  ASSERT_TRUE(top.has_value());
  EXPECT_LT(*top, 60U);
  EXPECT_GE(*top, 10U);
}

TEST_F(Sdl3SurfaceTest, GlassOverTheSurfaceRefractsItsContent) {
  std::array<std::uint8_t, 4> refracted{};
  std::array<std::uint8_t, 4> reference{};
  for (const Scene scene : {Scene::glass, Scene::flat}) {
    auto rig = Rig::create(scene, on_demand);
    if (rig == nullptr) {
      GTEST_SKIP() << "no SDL_GPU device under the offscreen driver";
    }
    rig->settle();
    (scene == Scene::glass ? refracted : reference) = rig->colour(120, 60);
  }
  EXPECT_TRUE(near(refracted, reference, 6))
      << static_cast<int>(refracted[0]) << "," << static_cast<int>(refracted[1])
      << "," << static_cast<int>(refracted[2]) << " vs "
      << static_cast<int>(reference[0]) << "," << static_cast<int>(reference[1])
      << "," << static_cast<int>(reference[2]);
  EXPECT_GT(static_cast<int>(refracted[0]),
            static_cast<int>(refracted[2]) + 60);
}

TEST_F(Sdl3SurfaceTest, AnUnderlayDrawsPixelExactInsideAHoleInTheBackground) {
  auto rig = Rig::create(Scene::backdrop, underlay);
  if (rig == nullptr) {
    GTEST_SKIP() << "no SDL_GPU device under the offscreen driver";
  }
  rig->recorder().paint(false);
  rig->settle();
  ASSERT_GT(rig->recorder().calls(), 0);
  const SurfaceFrame frame = rig->recorder().last();
  EXPECT_EQ(frame.viewport, (PassRegion{.x = static_cast<float>(view.x),
                                        .y = static_cast<float>(view.y),
                                        .width = view_width,
                                        .height = view_height}));
  EXPECT_EQ(frame.scissor, view);
  const std::array<std::uint8_t, 4> clear = rig->colour(4, 4);
  EXPECT_TRUE(near(rig->colour(view.x + 10, view.y + 10), clear, 1));
  EXPECT_EQ(rig->colour(view.x - 10, view.y + 10), backdrop);
  rig->recorder().paint(true);
  rig->surface().request_frame();
  rig->frame();
  EXPECT_EQ(rig->colour(view.x, view.y), painted);
  EXPECT_EQ(rig->colour(view.x + view.width - 1, view.y + view.height - 1),
            painted);
  EXPECT_EQ(rig->colour(view.x - 1, view.y), backdrop);
  EXPECT_EQ(rig->colour(view.x + view.width, view.y + view.height), backdrop);
}

}
