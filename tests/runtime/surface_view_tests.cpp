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
#include <arisa/provider/gpu/draw_data.hpp>
#include <arisa/provider/gpu/gpu_surface.hpp>
#include <arisa/provider/gpu/image.hpp>
#include <arisa/provider/gpu/surface_frame.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/headless/null_gpu.hpp>
#include <arisa/provider/headless/software_raster.hpp>
#include <arisa/provider/input/provider.hpp>
#include <arisa/provider/surface/surface_scale.hpp>
#include <arisa/provider/surface/surface_size.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/shape.hpp>

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace arisa::runtime {

using namespace std::chrono_literals;

class SurfaceViewTest : public ::testing::Test {
 protected:
  using NullGpu = provider::headless::NullGpu;
  using Factory = provider::headless::HeadlessProviderFactory;

  enum class Scene : std::uint8_t { bare, glass, flat, backdrop, scrolling };

  static constexpr std::uint32_t width = 240;
  static constexpr std::uint32_t height = 160;
  static constexpr float view_width = 64.0F;
  static constexpr float view_height = 48.0F;
  static constexpr provider::ClipRect view{
      .x = 88, .y = 56, .width = 64, .height = 48};
  static constexpr provider::Colour painted{.red = 230.0F / 255.0F,
                                            .green = 26.0F / 255.0F,
                                            .blue = 51.0F / 255.0F,
                                            .alpha = 1.0F};
  static constexpr std::uint32_t painted_rgb = 0xE61A33;
  static constexpr std::uint32_t backdrop_rgb = 0x2050C0;
  static constexpr int settle_limit = 60;

  class Recorder {
   public:
    void record(const provider::SurfaceFrame& frame,
                std::optional<std::array<std::uint32_t, 2>> canvas) {
      const std::scoped_lock lock{mutex_};
      ++calls_;
      last_ = frame;
      canvas_ = canvas;
    }

    auto calls() const -> int {
      const std::scoped_lock lock{mutex_};
      return calls_;
    }

    auto last() const -> provider::SurfaceFrame {
      const std::scoped_lock lock{mutex_};
      return last_;
    }

    auto canvas() const -> std::optional<std::array<std::uint32_t, 2>> {
      const std::scoped_lock lock{mutex_};
      return canvas_;
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
    provider::SurfaceFrame last_{};
    std::optional<std::array<std::uint32_t, 2>> canvas_;
    bool paints_{true};
  };

  struct Controls {
    composition::Observable<bool> shown{true};
    composition::Observable<float> width{view_width};
    composition::Observable<bool> away{false};
    composition::Observable<int> unrelated{0};
  };

  class Stage final : public composition::Component {
   public:
    Stage(std::optional<std::reference_wrapper<const provider::GpuSurface>>
              surface,
          Scene scene, const Controls& controls) noexcept
        : surface_(surface), scene_(scene), controls_(controls) {}

    auto operator==(const Stage& other) const -> bool {
      return surface_.has_value() == other.surface_.has_value() &&
             (!surface_.has_value() ||
              &surface_->get() == &other.surface_->get()) &&
             scene_ == other.scene_ &&
             &controls_.get() == &other.controls_.get();
    }

    auto body(composition::Context& context) const
        -> composition::Element override {
      const Controls& controls = controls_;
      const bool shown = context.read(controls.shown);
      const float wide = context.read(controls.width);
      const bool away = context.read(controls.away);
      static_cast<void>(context.read(controls.unrelated));
      const composition::FrameSpec size{.width = wide, .height = view_height};
      composition::Element shown_view =
          scene_ == Scene::flat || !surface_.has_value()
              ? composition::Element{components::VStack()
                                         .frame(size)
                                         .background(
                                             theme::Colour::hex(painted_rgb))}
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
                  .frame({.width = static_cast<float>(width),
                          .height = static_cast<float>(height)})
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
    std::optional<std::reference_wrapper<const provider::GpuSurface>> surface_;
    Scene scene_;
    std::reference_wrapper<const Controls> controls_;
  };

  class Harness {
   public:
    Harness(Scene scene, provider::SurfaceOptions options,
            provider::SurfaceScale scale = provider::SurfaceScale::unit(),
            bool partial = true) {
      const provider::SurfaceSize extent{
          .width = provider::SurfaceWidth{width},
          .height = provider::SurfaceHeight{height}};
      providers_.surface().resize(extent);
      providers_.surface().set_scale(scale);
      providers_.gpu().rasterize(extent);
      providers_.gpu().swapchain(1, partial);
      if (scene != Scene::flat) {
        auto made = providers_.gpu().add_surface(
            options,
            [recorder = recorder_](NullGpu::FrameCanvas canvas,
                                   const provider::SurfaceFrame& frame) {
              std::optional<std::array<std::uint32_t, 2>> size;
              if (canvas.has_value()) {
                size =
                    std::array{canvas->get().width(), canvas->get().height()};
                if (recorder->paints()) {
                  if (frame.mode == provider::SurfaceMode::texture) {
                    canvas->get().fill(painted);
                  } else {
                    canvas->get().fill(frame.scissor, painted);
                  }
                }
              }
              recorder->record(frame, size);
            });
        EXPECT_TRUE(made.has_value());
        surface_.emplace(std::move(*made));
      }
      std::optional<std::reference_wrapper<const provider::GpuSurface>> shown;
      if (surface_.has_value()) {
        shown = std::cref(*surface_);
      }
      auto created =
          Application::create(providers_, Stage{shown, scene, controls_});
      EXPECT_TRUE(created.has_value());
      application_.emplace(std::move(*created));
    }

    void frame() {
      ASSERT_TRUE(providers_.clock().advance(16ms).has_value());
      ASSERT_TRUE(application_->frame().has_value());
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

    void touch(provider::PixelPoint point, provider::TouchPhase phase) {
      provider::InputFrame input;
      input.touches.push_back(provider::Touch{
          .id = provider::TouchId{3}, .position = point, .phase = phase});
      providers_.input().push(input);
      frame();
    }

    auto colour(std::uint32_t x, std::uint32_t y) -> std::array<int, 3> {
      const auto image = providers_.gpu().capture();
      EXPECT_TRUE(image.has_value());
      if (!image.has_value()) {
        return {};
      }
      const auto found = image->pixel(x, y);
      EXPECT_TRUE(found.has_value());
      if (!found.has_value()) {
        return {};
      }
      return {(*found)[0], (*found)[1], (*found)[2]};
    }

    auto top_of_paint(std::uint32_t x) -> std::optional<std::uint32_t> {
      const auto image = providers_.gpu().capture();
      if (!image.has_value()) {
        return std::nullopt;
      }
      for (std::uint32_t y = 0; y < height; ++y) {
        const auto found = image->pixel(x, y);
        if (found.has_value() && near({(*found)[0], (*found)[1], (*found)[2]},
                                      rgb(painted_rgb), 2)) {
          return y;
        }
      }
      return std::nullopt;
    }

    auto gpu() -> NullGpu& { return providers_.gpu(); }

    auto application() -> Application& { return *application_; }

    auto controls() -> Controls& { return controls_; }

    auto recorder() -> Recorder& { return *recorder_; }

    auto surface() -> provider::GpuSurface& { return *surface_; }

    void drop_surface() { surface_.reset(); }

   private:
    Factory providers_;
    Controls controls_;
    std::shared_ptr<Recorder> recorder_ = std::make_shared<Recorder>();
    std::optional<provider::GpuSurface> surface_;
    std::optional<Application> application_;
  };

  static auto rgb(std::uint32_t hex) -> std::array<int, 3> {
    return {static_cast<int>((hex >> 16U) & 0xFFU),
            static_cast<int>((hex >> 8U) & 0xFFU),
            static_cast<int>(hex & 0xFFU)};
  }

  static auto near(std::array<int, 3> left, std::array<int, 3> right,
                   int tolerance) -> bool {
    for (std::size_t channel = 0; channel < 3; ++channel) {
      if (std::abs(left[channel] - right[channel]) > tolerance) {
        return false;
      }
    }
    return true;
  }

  static auto contains(provider::ClipRect outer, provider::ClipRect inner)
      -> bool {
    return outer.x <= inner.x && outer.y <= inner.y &&
           outer.x + outer.width >= inner.x + inner.width &&
           outer.y + outer.height >= inner.y + inner.height;
  }

  static constexpr provider::SurfaceOptions texture_on_demand{
      .mode = provider::SurfaceMode::texture};
  static constexpr provider::SurfaceOptions texture_continuous{
      .mode = provider::SurfaceMode::texture,
      .drive = provider::FrameDrive::continuous};
  static constexpr provider::SurfaceOptions underlay_on_demand{
      .mode = provider::SurfaceMode::underlay};
};

TEST_F(SurfaceViewTest, AContinuousTextureSurfaceRendersOncePerPresentedFrame) {
  Harness harness{Scene::bare, texture_continuous};
  harness.frames(4);
  const std::size_t presented = harness.gpu().frames_presented();
  const int calls = harness.recorder().calls();
  harness.frames(12);
  EXPECT_EQ(harness.gpu().frames_presented() - presented, 12U);
  EXPECT_EQ(harness.recorder().calls() - calls, 12);
  EXPECT_EQ(harness.recorder().last().mode, provider::SurfaceMode::texture);
}

TEST_F(SurfaceViewTest, AnOnDemandTextureSurfaceRendersOnlyWhenRequested) {
  Harness harness{Scene::glass, texture_on_demand};
  harness.settle();
  EXPECT_EQ(harness.recorder().calls(), 1);
  harness.controls().unrelated.set(1);
  harness.frames(3);
  EXPECT_EQ(harness.recorder().calls(), 1);
  harness.settle();
  const std::size_t uploads = harness.gpu().uploads();
  harness.surface().request_frame();
  harness.frame();
  const FrameTimings& timings = harness.application().last_frame();
  EXPECT_TRUE(timings.presented);
  EXPECT_EQ(timings.evaluations, 0U);
  EXPECT_FALSE(timings.laid_out);
  EXPECT_EQ(timings.uploaded_vertices, 0U);
  EXPECT_EQ(harness.gpu().uploads(), uploads);
  EXPECT_EQ(harness.recorder().calls(), 2);
  const auto damage = harness.gpu().last_damage();
  ASSERT_TRUE(damage.has_value());
  EXPECT_TRUE(contains(*damage, view));
  EXPECT_LT(damage->x, view.x);
  EXPECT_TRUE(
      contains(provider::ClipRect{.x = 60, .y = 50, .width = 120, .height = 60},
               *damage));
  harness.frame();
  EXPECT_FALSE(harness.application().last_frame().presented);
  EXPECT_EQ(harness.recorder().calls(), 2);
}

TEST_F(SurfaceViewTest, TheRenderTargetIsTheLaidOutPixelRectAtEveryScale) {
  for (const float factor : {1.0F, 1.25F}) {
    Harness harness{Scene::bare, texture_on_demand,
                    provider::SurfaceScale::create(factor).value_or(
                        provider::SurfaceScale::unit())};
    harness.settle();
    const auto expected_width =
        static_cast<std::uint32_t>(std::lround(view_width * factor));
    const auto expected_height =
        static_cast<std::uint32_t>(std::lround(view_height * factor));
    const provider::SurfaceFrame frame = harness.recorder().last();
    EXPECT_EQ(frame.frame.extent.width.value(), expected_width) << factor;
    EXPECT_EQ(frame.frame.extent.height.value(), expected_height) << factor;
    EXPECT_EQ(frame.viewport.width, static_cast<float>(expected_width));
    EXPECT_EQ(frame.scissor.width, expected_width);
    const auto canvas = harness.recorder().canvas();
    ASSERT_TRUE(canvas.has_value());
    EXPECT_EQ((*canvas)[0], expected_width);
    EXPECT_EQ((*canvas)[1], expected_height);
  }
}

TEST_F(SurfaceViewTest, ALayoutChangeResizesTheTargetAndRetiresTheOldOne) {
  Harness harness{Scene::bare, texture_on_demand};
  harness.settle();
  const std::size_t targets = harness.gpu().render_targets();
  const std::size_t live = harness.gpu().live_textures();
  harness.gpu().hold_completion(2);
  harness.surface().request_frame();
  harness.frame();
  EXPECT_EQ(harness.recorder().calls(), 2);
  harness.controls().width.set(96.0F);
  harness.frame();
  EXPECT_EQ(harness.recorder().calls(), 3);
  EXPECT_EQ(harness.recorder().last().frame.extent.width.value(), 96U);
  EXPECT_EQ(harness.gpu().render_targets(), targets + 1);
  EXPECT_EQ(harness.gpu().live_textures(), live + 1);
  harness.gpu().hold_completion(0);
  EXPECT_EQ(harness.gpu().live_textures(), live);
  EXPECT_EQ(harness.colour(view.x - 14, view.y + 10), rgb(painted_rgb));
}

TEST_F(SurfaceViewTest, ContentClipsInsideAScrollViewAndScrollsWithIt) {
  Harness harness{Scene::scrolling, texture_on_demand};
  harness.settle();
  EXPECT_EQ(harness.top_of_paint(120), std::optional<std::uint32_t>{90U});
  EXPECT_TRUE(near(harness.colour(120, 129), rgb(painted_rgb), 2));
  EXPECT_FALSE(near(harness.colour(120, 131), rgb(painted_rgb), 40));
  harness.touch({.x = 120.0F, .y = 120.0F}, provider::TouchPhase::began);
  for (int step = 1; step <= 8; ++step) {
    harness.touch(
        {.x = 120.0F, .y = 120.0F - (static_cast<float>(step) * 8.0F)},
        provider::TouchPhase::moved);
  }
  for (int hold = 0; hold < 6; ++hold) {
    harness.touch({.x = 120.0F, .y = 56.0F}, provider::TouchPhase::moved);
  }
  harness.touch({.x = 120.0F, .y = 56.0F}, provider::TouchPhase::ended);
  harness.settle();
  EXPECT_EQ(harness.recorder().calls(), 1);
  const auto top = harness.top_of_paint(120);
  ASSERT_TRUE(top.has_value());
  EXPECT_LT(*top, 70U);
  EXPECT_GE(*top, 30U);
  EXPECT_FALSE(near(harness.colour(120, 29), rgb(painted_rgb), 40));
}

TEST_F(SurfaceViewTest, GlassOverTheSurfaceRefractsItsContent) {
  Harness surface{Scene::glass, texture_on_demand};
  surface.settle();
  Harness flat{Scene::flat, texture_on_demand};
  flat.settle();
  Harness empty{Scene::glass, texture_on_demand};
  empty.recorder().paint(false);
  empty.controls().shown.set(false);
  empty.settle();
  const std::array<int, 3> refracted = surface.colour(120, 80);
  const std::array<int, 3> reference = flat.colour(120, 80);
  const std::array<int, 3> plain = empty.colour(120, 80);
  EXPECT_TRUE(near(refracted, reference, 4))
      << refracted[0] << "," << refracted[1] << "," << refracted[2] << " vs "
      << reference[0] << "," << reference[1] << "," << reference[2];
  EXPECT_FALSE(near(refracted, plain, 30));
}

TEST_F(SurfaceViewTest, RemovingTheViewReleasesItsTargetBySerialAndStopsCalls) {
  Harness harness{Scene::bare, texture_continuous};
  harness.gpu().hold_completion(2);
  harness.frames(4);
  const std::size_t live = harness.gpu().live_textures();
  harness.controls().shown.set(false);
  harness.frame();
  const int calls = harness.recorder().calls();
  EXPECT_EQ(harness.gpu().live_textures(), live);
  harness.gpu().hold_completion(0);
  EXPECT_EQ(harness.gpu().live_textures(), live - 1);
  harness.frames(6);
  EXPECT_EQ(harness.recorder().calls(), calls);
}

TEST_F(SurfaceViewTest, ACulledViewGetsNoCallsAndKeepsItsTarget) {
  Harness harness{Scene::bare, texture_continuous};
  harness.frames(4);
  const std::size_t targets = harness.gpu().render_targets();
  harness.controls().away.set(true);
  harness.frame();
  const int calls = harness.recorder().calls();
  harness.frames(6);
  EXPECT_EQ(harness.recorder().calls(), calls);
  harness.controls().away.set(false);
  harness.frames(3);
  EXPECT_GT(harness.recorder().calls(), calls);
  EXPECT_EQ(harness.gpu().render_targets(), targets);
}

TEST_F(SurfaceViewTest, UnderlayGetsThePixelRectAndAHoleInTheBackground) {
  Harness harness{Scene::backdrop, underlay_on_demand};
  harness.recorder().paint(false);
  harness.settle();
  ASSERT_GT(harness.recorder().calls(), 0);
  const provider::SurfaceFrame frame = harness.recorder().last();
  EXPECT_EQ(frame.mode, provider::SurfaceMode::underlay);
  EXPECT_EQ(frame.viewport,
            (provider::PassRegion{.x = static_cast<float>(view.x),
                                  .y = static_cast<float>(view.y),
                                  .width = view_width,
                                  .height = view_height}));
  EXPECT_EQ(frame.scissor, view);
  EXPECT_EQ(frame.frame.extent.width.value(), width);
  const auto clear = harness.gpu().last_clear();
  ASSERT_TRUE(clear.has_value());
  const std::array<int, 3> cleared{
      static_cast<int>(std::lround(clear->red * 255.0F)),
      static_cast<int>(std::lround(clear->green * 255.0F)),
      static_cast<int>(std::lround(clear->blue * 255.0F))};
  EXPECT_TRUE(near(harness.colour(view.x + 10, view.y + 10), cleared, 1));
  EXPECT_EQ(harness.colour(view.x - 10, view.y + 10), rgb(backdrop_rgb));
  harness.recorder().paint(true);
  harness.surface().request_frame();
  harness.frame();
  EXPECT_TRUE(harness.application().last_frame().presented);
  EXPECT_EQ(harness.colour(view.x, view.y), rgb(painted_rgb));
  EXPECT_EQ(harness.colour(view.x + view.width - 1, view.y + view.height - 1),
            rgb(painted_rgb));
  EXPECT_EQ(harness.colour(view.x - 1, view.y), rgb(backdrop_rgb));
  EXPECT_EQ(harness.colour(view.x + view.width, view.y + view.height),
            rgb(backdrop_rgb));
}

TEST_F(SurfaceViewTest, AnUnderlayRunsOnEveryPresentedFrameWhileShown) {
  Harness harness{Scene::backdrop, underlay_on_demand};
  harness.settle();
  const int calls = harness.recorder().calls();
  const std::size_t presented = harness.gpu().frames_presented();
  harness.controls().unrelated.set(4);
  harness.frame();
  harness.surface().request_frame();
  harness.frame();
  EXPECT_EQ(harness.recorder().calls() - calls,
            static_cast<int>(harness.gpu().frames_presented() - presented));
  harness.controls().shown.set(false);
  harness.frames(2);
  const int hidden = harness.recorder().calls();
  harness.surface().request_frame();
  harness.frames(2);
  EXPECT_EQ(harness.recorder().calls(), hidden);
}

TEST_F(SurfaceViewTest, DroppingTheSurfaceEndsItsRegistrationAndItsDrive) {
  Harness harness{Scene::bare, texture_continuous};
  harness.frames(3);
  harness.controls().shown.set(false);
  harness.frame();
  harness.drop_surface();
  harness.settle();
  const int calls = harness.recorder().calls();
  harness.frame();
  EXPECT_FALSE(harness.application().last_frame().presented);
  EXPECT_EQ(harness.recorder().calls(), calls);
}

class OwnedSurfaceTest : public SurfaceViewTest {
 protected:
  class Owned final : public composition::Component {
   public:
    Owned(NullGpu& gpu, const composition::Observable<bool>& shown,
          std::shared_ptr<Recorder> recorder) noexcept
        : gpu_(gpu), shown_(shown), recorder_(std::move(recorder)) {}

    auto operator==(const Owned& other) const -> bool {
      return &gpu_.get() == &other.gpu_.get() &&
             &shown_.get() == &other.shown_.get();
    }

    auto body(composition::Context& context) const
        -> composition::Element override {
      if (!context.read(shown_.get())) {
        return components::ZStack();
      }
      return components::ZStack(
          components::SurfaceView(
              gpu_.get(), texture_continuous,
              [recorder = recorder_](NullGpu::FrameCanvas,
                                     const provider::SurfaceFrame& frame) {
                recorder->record(frame, std::nullopt);
              })
              .frame({.width = view_width, .height = view_height}));
    }

   private:
    std::reference_wrapper<NullGpu> gpu_;
    std::reference_wrapper<const composition::Observable<bool>> shown_;
    std::shared_ptr<Recorder> recorder_;
  };
};

TEST_F(OwnedSurfaceTest, AViewOwnedSurfaceRegistersWithTheViewAndEndsWithIt) {
  Factory providers;
  const provider::SurfaceSize extent{.width = provider::SurfaceWidth{width},
                                     .height = provider::SurfaceHeight{height}};
  providers.surface().resize(extent);
  providers.surface().set_scale(provider::SurfaceScale::unit());
  providers.gpu().rasterize(extent);
  composition::Observable<bool> shown{true};
  auto recorder = std::make_shared<Recorder>();
  auto created =
      Application::create(providers, Owned{providers.gpu(), shown, recorder});
  ASSERT_TRUE(created.has_value());
  const auto step = [&] {
    ASSERT_TRUE(providers.clock().advance(16ms).has_value());
    ASSERT_TRUE(created->frame().has_value());
  };
  for (int index = 0; index < 5; ++index) {
    step();
  }
  EXPECT_GE(recorder->calls(), 4);
  EXPECT_EQ(recorder->last().frame.extent.width.value(), 64U);
  shown.set(false);
  step();
  step();
  const int calls = recorder->calls();
  for (int index = 0; index < 4; ++index) {
    step();
  }
  EXPECT_EQ(recorder->calls(), calls);
  EXPECT_FALSE(created->last_frame().presented);
}

}
