#include <arisa/components/button.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/alignment.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/composition/overlay_layer.hpp>
#include <arisa/controls/progress_view.hpp>
#include <arisa/provider/gpu/colour.hpp>
#include <arisa/provider/gpu/frame_info.hpp>
#include <arisa/provider/gpu/frame_source.hpp>
#include <arisa/provider/gpu/pixel_rows.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/headless/null_gpu.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/provider/surface/surface_scale.hpp>
#include <arisa/provider/surface/surface_size.hpp>
#include <arisa/provider/threading/thread.hpp>
#include <arisa/provider/threading/thread_role.hpp>
#include <arisa/provider/threading/thread_spec.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/runtime/embedded_root.hpp>
#include <arisa/runtime/overlay_handle.hpp>
#include <arisa/runtime/overlays.hpp>
#include <arisa/runtime/root.hpp>
#include <arisa/theme/colour.hpp>

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <stop_token>
#include <thread>
#include <utility>

namespace arisa::runtime {

using namespace std::chrono_literals;

class EmbeddedOverlayTest : public ::testing::Test {
 protected:
  using NullGpu = provider::headless::NullGpu;

  static constexpr std::uint32_t width = 160;
  static constexpr std::uint32_t height = 90;
  static constexpr provider::SurfaceSize extent{
      .width = provider::SurfaceWidth{width},
      .height = provider::SurfaceHeight{height}};
  static constexpr provider::Colour video{
      .red = 0.0F, .green = 0.0F, .blue = 1.0F, .alpha = 1.0F};

  class Dock final : public composition::Component {
   public:
    Dock(const composition::Observable<int>& shade,
         std::atomic<int>& pressed) noexcept
        : shade_(shade), pressed_(pressed) {}

    auto operator==(const Dock& other) const -> bool {
      return &shade_.get() == &other.shade_.get() &&
             &pressed_.get() == &other.pressed_.get();
    }

    auto body(composition::Context& context) const
        -> composition::Element override {
      const auto level =
          static_cast<std::uint32_t>(0xC0 + (context.read(shade_.get()) % 32));
      std::atomic<int>& pressed = pressed_.get();
      return components::VStack(
                 components::Button("dock",
                                    [&pressed] { pressed.fetch_add(1); })
                     .frame({.width = 40.0F, .height = 30.0F})
                     .background(theme::Colour::hex(level << 16U))
                     .id("dock"))
          .frame({.width = 160.0F,
                  .height = 90.0F,
                  .alignment = composition::Alignment::top_leading});
    }

   private:
    std::reference_wrapper<const composition::Observable<int>> shade_;
    std::reference_wrapper<std::atomic<int>> pressed_;
  };

  class Blank final : public composition::Component {
   public:
    auto operator==(const Blank&) const -> bool { return true; }

    auto body(composition::Context&) const -> composition::Element override {
      return components::VStack().frame({.width = 160.0F, .height = 90.0F});
    }
  };

  class Closer final : public composition::Component {
   public:
    explicit Closer(std::optional<EmbeddedRoot>& embedded) noexcept
        : embedded_(embedded) {}

    auto operator==(const Closer& other) const -> bool {
      return &embedded_.get() == &other.embedded_.get();
    }

    auto body(composition::Context&) const -> composition::Element override {
      std::optional<EmbeddedRoot>& embedded = embedded_.get();
      return components::VStack(components::VStack()
                                    .frame({.width = 40.0F, .height = 30.0F})
                                    .background(theme::Colour::hex(0xC00000))
                                    .focusable()
                                    .id("closer"))
          .frame({.width = 160.0F,
                  .height = 90.0F,
                  .alignment = composition::Alignment::top_leading})
          .on_command(provider::Button::a, "Close",
                      [&embedded] { embedded.reset(); });
    }

   private:
    std::reference_wrapper<std::optional<EmbeddedRoot>> embedded_;
  };

  class Spinner final : public composition::Component {
   public:
    auto operator==(const Spinner&) const -> bool { return true; }

    auto body(composition::Context&) const -> composition::Element override {
      return components::VStack(controls::ProgressView())
          .frame({.width = 160.0F, .height = 90.0F});
    }
  };

  class Backdrop final : public composition::Component {
   public:
    explicit Backdrop(std::atomic<int>& pressed) noexcept : pressed_(pressed) {}

    auto operator==(const Backdrop& other) const -> bool {
      return &pressed_.get() == &other.pressed_.get();
    }

    auto body(composition::Context&) const -> composition::Element override {
      std::atomic<int>& pressed = pressed_.get();
      return components::VStack(
          components::Button("main", [&pressed] { pressed.fetch_add(1); })
              .frame({.width = 160.0F, .height = 90.0F})
              .id("main"));
    }

   private:
    std::reference_wrapper<std::atomic<int>> pressed_;
  };

  void SetUp() override {
    providers_.surface().resize(extent);
    providers_.surface().set_scale(provider::SurfaceScale::unit());
    providers_.gpu().rasterize(extent);
    providers_.gpu().swapchain(2, true);
  }

  auto gpu() -> NullGpu& { return providers_.gpu(); }

  auto start() -> Application {
    auto created = Application::create(providers_, Backdrop{main_pressed_});
    EXPECT_TRUE(created.has_value());
    Application application = std::move(*created);
    frames(application, 2);
    return application;
  }

  void frames(Application& application, int count) {
    for (int index = 0; index < count; ++index) {
      ASSERT_TRUE(providers_.clock().advance(16ms).has_value());
      ASSERT_TRUE(application.frame().has_value());
    }
  }

  static auto present(NullGpu::Takeover& taken, NullGpu::Overlay& overlay)
      -> std::expected<bool, provider::ProviderError> {
    const auto image = taken.acquire();
    if (!image.has_value()) {
      return std::unexpected(image.error());
    }
    NullGpu::FrameCanvas target = taken.target(*image);
    if (target.has_value()) {
      target->get().fill(video);
    }
    auto recorded = overlay.record(target);
    if (!taken.present(*image).has_value()) {
      return std::unexpected(provider::ProviderError{
          provider::ProviderErrorCode::invalid_argument});
    }
    return recorded;
  }

  static auto press(provider::Button which) -> provider::InputFrame {
    provider::InputFrame frame;
    frame.pads[0] = provider::PadState{
        .connected = true, .buttons = {which}, .left = {}, .right = {}};
    return frame;
  }

  static auto release() -> provider::InputFrame {
    provider::InputFrame frame;
    frame.pads[0] = provider::PadState{
        .connected = true, .buttons = {}, .left = {}, .right = {}};
    return frame;
  }

  auto embed_dock(Application& application, NullGpu::Overlay& overlay,
                  Root& dock) -> std::optional<EmbeddedRoot> {
    auto embedded = application.embed(dock, overlay.frames(), extent);
    EXPECT_TRUE(embedded.has_value());
    if (!embedded.has_value()) {
      return std::nullopt;
    }
    return std::move(*embedded);
  }

  provider::headless::HeadlessProviderFactory providers_;
  composition::Observable<int> shade_{0};
  std::atomic<int> pressed_{0};
  std::atomic<int> main_pressed_{0};
};

TEST_F(EmbeddedOverlayTest,
       TheOverlayIsRecordedIntoEveryAppFrameWithoutAClear) {
  Application application = start();
  auto taken = gpu().take_over();
  ASSERT_TRUE(taken.has_value());
  auto overlay = taken->overlay();
  ASSERT_TRUE(overlay.has_value());
  Root dock{Dock{shade_, pressed_}, providers_};
  auto embedded = application.embed(dock, overlay->frames(), extent);
  ASSERT_TRUE(embedded.has_value());
  EXPECT_TRUE(embedded->attached());
  frames(application, 2);
  EXPECT_GE(overlay->statistics().published, 1U);
  for (int index = 0; index < 4; ++index) {
    const auto recorded = present(*taken, *overlay);
    ASSERT_TRUE(recorded.has_value());
    EXPECT_TRUE(*recorded);
  }
  const NullGpu::Overlay::Statistics counted = overlay->statistics();
  EXPECT_EQ(counted.recorded, 4U);
  EXPECT_EQ(counted.fresh, 1U);
  const auto shown = gpu().capture();
  ASSERT_TRUE(shown.has_value());
  const auto docked = shown->pixel(10, 10);
  const auto beside = shown->pixel(120, 70);
  ASSERT_TRUE(docked.has_value());
  ASSERT_TRUE(beside.has_value());
  EXPECT_GT((*docked)[0], 0xA0);
  EXPECT_LT((*docked)[2], 0x40);
  EXPECT_EQ((*beside)[0], 0);
  EXPECT_EQ((*beside)[2], 0xFF);
}

TEST_F(EmbeddedOverlayTest, AnEmptyOverlayRecordsNothing) {
  Application application = start();
  auto taken = gpu().take_over();
  ASSERT_TRUE(taken.has_value());
  auto overlay = taken->overlay();
  ASSERT_TRUE(overlay.has_value());
  Root blank{Blank{}, providers_};
  auto embedded = application.embed(blank, overlay->frames(), extent);
  ASSERT_TRUE(embedded.has_value());
  frames(application, 3);
  for (int index = 0; index < 3; ++index) {
    const auto recorded = present(*taken, *overlay);
    ASSERT_TRUE(recorded.has_value());
    EXPECT_FALSE(*recorded);
  }
  const NullGpu::Overlay::Statistics counted = overlay->statistics();
  EXPECT_GE(counted.published, 1U);
  EXPECT_EQ(counted.empty, counted.published);
  EXPECT_EQ(counted.recorded, 0U);
  EXPECT_EQ(counted.idle, 3U);
  const auto shown = gpu().capture();
  ASSERT_TRUE(shown.has_value());
  const auto corner = shown->pixel(10, 10);
  ASSERT_TRUE(corner.has_value());
  EXPECT_EQ((*corner)[2], 0xFF);
}

TEST_F(EmbeddedOverlayTest, ARecordedSlotIsNotReusedBeforeItsPresent) {
  Application application = start();
  auto taken = gpu().take_over();
  ASSERT_TRUE(taken.has_value());
  auto overlay = taken->overlay();
  ASSERT_TRUE(overlay.has_value());
  Root dock{Dock{shade_, pressed_}, providers_};
  auto embedded = application.embed(dock, overlay->frames(), extent);
  ASSERT_TRUE(embedded.has_value());
  frames(application, 2);
  const auto first = overlay->current_slot();
  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(overlay->record(std::nullopt).has_value());
  shade_.set(1);
  frames(application, 1);
  const auto second = overlay->current_slot();
  ASSERT_TRUE(second.has_value());
  EXPECT_NE(*second, *first);
  ASSERT_TRUE(overlay->record(std::nullopt).has_value());
  const std::uint64_t published = overlay->statistics().published;
  for (int index = 0; index < 4; ++index) {
    shade_.set(2 + index);
    frames(application, 1);
    const auto showing = overlay->current_slot();
    ASSERT_TRUE(showing.has_value());
    EXPECT_NE(*showing, *first);
  }
  const std::uint64_t held = overlay->statistics().published;
  EXPECT_LE(held, published + 1);
  EXPECT_GE(overlay->statistics().refused, 3U);
  const auto image = taken->acquire();
  ASSERT_TRUE(image.has_value());
  ASSERT_TRUE(taken->present(*image).has_value());
  frames(application, 1);
  EXPECT_EQ(overlay->statistics().published, held + 1);
}

TEST_F(EmbeddedOverlayTest, InputReachesTheOverlayWhileTheAppRecords) {
  Application application = start();
  auto taken = gpu().take_over();
  ASSERT_TRUE(taken.has_value());
  auto overlay = taken->overlay();
  ASSERT_TRUE(overlay.has_value());
  Root dock{Dock{shade_, pressed_}, providers_};
  auto embedded = application.embed(dock, overlay->frames(), extent);
  ASSERT_TRUE(embedded.has_value());
  frames(application, 2);
  std::atomic<int> recorded{0};
  std::atomic<bool> failed{false};
  auto started = providers_.threading().start(
      provider::ThreadSpec{.name = "test-presenter",
                           .role = provider::ThreadRole::app},
      [&](const std::stop_token& stop) {
        while (!stop.stop_requested()) {
          const auto drawn = present(*taken, *overlay);
          if (!drawn.has_value()) {
            failed.store(true);
            return;
          }
          recorded.fetch_add(1);
          std::this_thread::sleep_for(200us);
        }
      });
  ASSERT_TRUE(started.has_value());
  std::optional<provider::Thread> presenter{std::move(*started)};
  providers_.input().push(press(provider::Button::a));
  providers_.input().push(release());
  for (int index = 0; index < 8; ++index) {
    shade_.set(index);
    frames(application, 1);
    std::this_thread::sleep_for(1ms);
  }
  const auto deadline = std::chrono::steady_clock::now() + 2s;
  while (recorded.load() < 4 && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(1ms);
  }
  presenter->request_stop();
  presenter.reset();
  EXPECT_FALSE(failed.load());
  EXPECT_GE(recorded.load(), 4);
  EXPECT_EQ(pressed_.load(), 1);
  EXPECT_EQ(main_pressed_.load(), 0);
  EXPECT_GE(overlay->statistics().recorded, 1U);
}

TEST_F(EmbeddedOverlayTest, HandingBackWithAnOverlayAliveIsClean) {
  Application application = start();
  auto taken = gpu().take_over();
  ASSERT_TRUE(taken.has_value());
  auto overlay = taken->overlay();
  ASSERT_TRUE(overlay.has_value());
  Root dock{Dock{shade_, pressed_}, providers_};
  auto embedded = application.embed(dock, overlay->frames(), extent);
  ASSERT_TRUE(embedded.has_value());
  frames(application, 2);
  ASSERT_TRUE(present(*taken, *overlay).has_value());
  taken->release();
  EXPECT_FALSE(overlay->held());
  const auto refused = overlay->record(std::nullopt);
  ASSERT_FALSE(refused.has_value());
  EXPECT_EQ(refused.error().detail(),
            provider::ProviderErrorDetail::not_taken_over);
  const std::uint64_t published = overlay->statistics().published;
  const std::size_t before = gpu().frames_presented();
  shade_.set(4);
  frames(application, 3);
  application.drain();
  EXPECT_EQ(overlay->statistics().published, published);
  EXPECT_GE(gpu().frames_presented() - before, 1U);
  EXPECT_FALSE(application.last_frame().taken_over);
  overlay->release();
  embedded->release();
  frames(application, 1);
  auto again = gpu().take_over();
  ASSERT_TRUE(again.has_value());
  auto reopened = again->overlay();
  ASSERT_TRUE(reopened.has_value());
  EXPECT_TRUE(reopened->held());
}

TEST_F(EmbeddedOverlayTest, OneOverlayPerTakeoverAndOneEmbeddedRoot) {
  Application application = start();
  auto taken = gpu().take_over();
  ASSERT_TRUE(taken.has_value());
  auto overlay = taken->overlay();
  ASSERT_TRUE(overlay.has_value());
  const auto second = taken->overlay();
  ASSERT_FALSE(second.has_value());
  EXPECT_EQ(second.error().detail(),
            provider::ProviderErrorDetail::overlay_open);
  Root dock{Dock{shade_, pressed_}, providers_};
  auto embedded = application.embed(dock, overlay->frames(), extent);
  ASSERT_TRUE(embedded.has_value());
  Root blank{Blank{}, providers_};
  const auto refused = application.embed(blank, overlay->frames(), extent);
  ASSERT_FALSE(refused.has_value());
  EXPECT_EQ(refused.error().code(), ApplicationErrorCode::already_embedded);
  overlay->release();
  EXPECT_TRUE(taken->overlay().has_value());
}

TEST_F(EmbeddedOverlayTest, AnOverlayActionCanDropItsOwnEmbedding) {
  Application application = start();
  auto taken = gpu().take_over();
  ASSERT_TRUE(taken.has_value());
  auto overlay = taken->overlay();
  ASSERT_TRUE(overlay.has_value());
  std::optional<EmbeddedRoot> embedded;
  Root closer{Closer{embedded}, providers_};
  embedded = embed_dock(application, *overlay, closer);
  ASSERT_TRUE(embedded.has_value());
  frames(application, 2);
  ASSERT_TRUE(present(*taken, *overlay).has_value());
  for (int index = 0; index < 3; ++index) {
    providers_.input().push(press(provider::Button::a));
  }
  providers_.input().push(release());
  frames(application, 4);
  EXPECT_FALSE(embedded.has_value());
  EXPECT_EQ(main_pressed_.load(), 0);
  providers_.input().push(press(provider::Button::a));
  providers_.input().push(release());
  frames(application, 2);
  EXPECT_EQ(main_pressed_.load(), 1);
}

TEST_F(EmbeddedOverlayTest, DetachingALiveOverlayLeavesNothingDrawn) {
  Application application = start();
  auto taken = gpu().take_over();
  ASSERT_TRUE(taken.has_value());
  auto overlay = taken->overlay();
  ASSERT_TRUE(overlay.has_value());
  Root dock{Dock{shade_, pressed_}, providers_};
  auto embedded = embed_dock(application, *overlay, dock);
  ASSERT_TRUE(embedded.has_value());
  frames(application, 2);
  const auto drawn = present(*taken, *overlay);
  ASSERT_TRUE(drawn.has_value());
  EXPECT_TRUE(*drawn);
  embedded->release();
  frames(application, 1);
  EXPECT_TRUE(overlay->held());
  EXPECT_FALSE(overlay->current_slot().has_value());
  const auto blank = present(*taken, *overlay);
  ASSERT_TRUE(blank.has_value());
  EXPECT_FALSE(*blank);
  const auto shown = gpu().capture();
  ASSERT_TRUE(shown.has_value());
  const auto corner = shown->pixel(10, 10);
  ASSERT_TRUE(corner.has_value());
  EXPECT_EQ((*corner)[2], 0xFF);
}

TEST_F(EmbeddedOverlayTest, AHandBackDetachesTheEmbeddingAndFreesMainInput) {
  Application application = start();
  auto taken = gpu().take_over();
  ASSERT_TRUE(taken.has_value());
  auto overlay = taken->overlay();
  ASSERT_TRUE(overlay.has_value());
  Root dock{Dock{shade_, pressed_}, providers_};
  auto embedded = embed_dock(application, *overlay, dock);
  ASSERT_TRUE(embedded.has_value());
  frames(application, 2);
  EXPECT_TRUE(embedded->attached());
  taken->release();
  frames(application, 1);
  EXPECT_FALSE(embedded->attached());
  providers_.input().push(press(provider::Button::a));
  providers_.input().push(release());
  frames(application, 2);
  EXPECT_EQ(main_pressed_.load(), 1);
  EXPECT_EQ(pressed_.load(), 0);
}

TEST_F(EmbeddedOverlayTest, AnAnimatingOverlayPaintsAtTheAppsPace) {
  Application application = start();
  auto taken = gpu().take_over();
  ASSERT_TRUE(taken.has_value());
  auto overlay = taken->overlay();
  ASSERT_TRUE(overlay.has_value());
  Root spinner{Spinner{}, providers_};
  auto embedded = embed_dock(application, *overlay, spinner);
  ASSERT_TRUE(embedded.has_value());
  frames(application, 2);
  std::atomic<std::uint64_t> recorded{0};
  std::atomic<bool> failed{false};
  auto started = providers_.threading().start(
      provider::ThreadSpec{.name = "test-presenter",
                           .role = provider::ThreadRole::app},
      [&](const std::stop_token& stop) {
        while (!stop.stop_requested()) {
          if (!present(*taken, *overlay).has_value()) {
            failed.store(true);
            return;
          }
          recorded.fetch_add(1);
          std::this_thread::sleep_for(40ms);
        }
      });
  ASSERT_TRUE(started.has_value());
  std::optional<provider::Thread> presenter{std::move(*started)};
  const std::uint64_t published = overlay->statistics().published;
  const auto began = std::chrono::steady_clock::now();
  std::uint64_t framed = 0;
  const auto ran = application.run_until([&](Root&) {
    ++framed;
    return std::chrono::steady_clock::now() - began >= 400ms;
  });
  const auto elapsed = std::chrono::steady_clock::now() - began;
  presenter->request_stop();
  presenter.reset();
  ASSERT_TRUE(ran.has_value());
  EXPECT_FALSE(failed.load());
  const auto refreshes =
      static_cast<std::uint64_t>(elapsed / Application::idle_poll);
  EXPECT_LE(framed, refreshes + recorded.load() + 4);
  EXPECT_LE(overlay->statistics().published - published, recorded.load() + 2);
  EXPECT_GE(overlay->statistics().published - published, 2U);
}

TEST_F(EmbeddedOverlayTest, AFirstRecordWithOnlyUploadsReportsWork) {
  auto taken = gpu().take_over();
  ASSERT_TRUE(taken.has_value());
  auto overlay = taken->overlay();
  ASSERT_TRUE(overlay.has_value());
  provider::FrameSource frames = overlay->frames();
  auto frame = frames.open();
  ASSERT_TRUE(frame.has_value());
  auto texture = gpu().create_texture({.width = 2, .height = 2});
  ASSERT_TRUE(texture.has_value());
  const std::array<std::byte, 16> texels{};
  ASSERT_TRUE(gpu()
                  .update_texture(*texture,
                                  {.x = 0, .y = 0, .width = 2, .height = 2},
                                  provider::PixelRows{texels})
                  .has_value());
  frame->plan(provider::FramePlan{.predicted_present = 0ns,
                                  .replay = false,
                                  .exclusive = false,
                                  .extent = extent,
                                  .swap_interval = 1,
                                  .capture = false});
  auto sealed = frame->seal();
  ASSERT_TRUE(sealed.has_value());
  ASSERT_TRUE(sealed->submit().has_value());
  const auto first = overlay->record(std::nullopt);
  ASSERT_TRUE(first.has_value());
  EXPECT_TRUE(*first);
  const auto again = overlay->record(std::nullopt);
  ASSERT_TRUE(again.has_value());
  EXPECT_FALSE(*again);
}

TEST_F(EmbeddedOverlayTest, AnOverlayFadingInFromAnEmptySceneKeepsAnimating) {
  Application application = start();
  auto taken = gpu().take_over();
  ASSERT_TRUE(taken.has_value());
  auto overlay = taken->overlay();
  ASSERT_TRUE(overlay.has_value());
  Root blank{Blank{}, providers_};
  auto embedded = embed_dock(application, *overlay, blank);
  ASSERT_TRUE(embedded.has_value());
  frames(application, 2);
  ASSERT_TRUE(present(*taken, *overlay).has_value());
  const std::uint64_t empty = overlay->statistics().empty;
  const OverlayHandle handle =
      blank.overlays().show(composition::OverlayLayer::hud,
                            components::VStack()
                                .frame({.width = 40.0F, .height = 30.0F})
                                .background(theme::Colour::hex(0xC00000)));
  frames(application, 1);
  EXPECT_GT(overlay->statistics().empty, empty);
  for (int index = 0; index < 30; ++index) {
    ASSERT_TRUE(present(*taken, *overlay).has_value());
    frames(application, 1);
  }
  ASSERT_TRUE(present(*taken, *overlay).has_value());
  EXPECT_GE(overlay->statistics().recorded, 1U);
  const auto shown = gpu().capture();
  ASSERT_TRUE(shown.has_value());
  const auto drawn = shown->pixel(80, 80);
  ASSERT_TRUE(drawn.has_value());
  EXPECT_GT((*drawn)[0], 0xA0);
  EXPECT_LT((*drawn)[2], 0x40);
}

TEST_F(EmbeddedOverlayTest, TheAppIsWokenWhenANewSceneIsPublished) {
  Application application = start();
  auto taken = gpu().take_over();
  ASSERT_TRUE(taken.has_value());
  auto overlay = taken->overlay();
  ASSERT_TRUE(overlay.has_value());
  Root dock{Dock{shade_, pressed_}, providers_};
  auto embedded = embed_dock(application, *overlay, dock);
  ASSERT_TRUE(embedded.has_value());
  frames(application, 2);
  const provider::FrameSource watched = overlay->frames();
  const std::uint64_t scenes = watched.scenes();
  EXPECT_GE(scenes, 1U);
  EXPECT_EQ(watched.await_scene(scenes, 1ms), scenes);
  ASSERT_TRUE(present(*taken, *overlay).has_value());
  shade_.set(3);
  frames(application, 1);
  EXPECT_GT(watched.await_scene(scenes, 10s), scenes);
}

TEST_F(EmbeddedOverlayTest, TheLentImagesFollowAResizeAndTheOverlayReembeds) {
  static constexpr provider::SurfaceSize docked{
      .width = provider::SurfaceWidth{320},
      .height = provider::SurfaceHeight{180}};
  Application application = start();
  auto taken = gpu().take_over();
  ASSERT_TRUE(taken.has_value());
  auto overlay = taken->overlay();
  ASSERT_TRUE(overlay.has_value());
  Root dock{Dock{shade_, pressed_}, providers_};
  auto embedded = embed_dock(application, *overlay, dock);
  ASSERT_TRUE(embedded.has_value());
  frames(application, 2);
  ASSERT_TRUE(present(*taken, *overlay).has_value());
  const std::uint64_t generation = taken->generation();
  providers_.surface().resize(docked);
  gpu().rasterize(docked);
  frames(application, 1);
  EXPECT_GT(taken->generation(), generation);
  const auto stale = present(*taken, *overlay);
  ASSERT_FALSE(stale.has_value());
  EXPECT_EQ(stale.error().detail(),
            provider::ProviderErrorDetail::overlay_extent);
  const auto first = gpu().capture();
  ASSERT_TRUE(first.has_value());
  EXPECT_EQ(first->width(), 320U);
  EXPECT_TRUE(embedded->resize(docked));
  frames(application, 2);
  const auto drawn = present(*taken, *overlay);
  ASSERT_TRUE(drawn.has_value());
  EXPECT_TRUE(*drawn);
  const auto shown = gpu().capture();
  ASSERT_TRUE(shown.has_value());
  const auto button = shown->pixel(100, 60);
  const auto video_corner = shown->pixel(300, 170);
  ASSERT_TRUE(button.has_value());
  ASSERT_TRUE(video_corner.has_value());
  EXPECT_GT((*button)[0], 0xA0);
  EXPECT_EQ((*video_corner)[2], 0xFF);
  embedded->release();
  EXPECT_FALSE(embedded->resize(extent));
}
}
