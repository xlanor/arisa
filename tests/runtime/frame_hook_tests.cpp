#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/provider/gpu/frame_hook.hpp>
#include <arisa/provider/gpu/frame_info.hpp>
#include <arisa/provider/gpu/image.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/headless/null_gpu.hpp>
#include <arisa/provider/headless/software_raster.hpp>
#include <arisa/provider/surface/surface_scale.hpp>
#include <arisa/provider/surface/surface_size.hpp>
#include <arisa/provider/threading/thread_role.hpp>
#include <arisa/provider/threading/thread_spec.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/shape.hpp>

#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <optional>
#include <stop_token>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::runtime {

using namespace std::chrono_literals;

class FrameHookTest : public ::testing::Test {
 protected:
  using NullGpu = provider::headless::NullGpu;
  using Canvas = provider::headless::SoftwareRaster::Canvas;

  static constexpr std::uint32_t width = 160;
  static constexpr std::uint32_t height = 96;
  static constexpr std::uint32_t control_rgb = 0x20C040;
  static constexpr std::uint32_t mover_rgb = 0x2050C0;
  static constexpr float control_width = 48.0F;
  static constexpr float control_height = 24.0F;
  static constexpr int settle_limit = 40;

  class Stage final : public composition::Component {
   public:
    explicit Stage(const composition::Observable<bool>& moved)
        : moved_(moved) {}

    auto operator==(const Stage& other) const -> bool {
      return &moved_.get() == &other.moved_.get();
    }

    auto body(composition::Context& context) const
        -> composition::Element override {
      const bool moved = context.read(moved_.get());
      return components::ZStack(
          components::VStack()
              .frame({.width = 8.0F, .height = 8.0F})
              .background(theme::Colour::hex(mover_rgb))
              .offset(moved ? 60.0F : 0.0F, -40.0F),
          components::Button(
              [] {},
              components::ZStack()
                  .frame({.width = control_width, .height = control_height})
                  .background(theme::Colour::hex(control_rgb),
                              theme::Shape::capsule()))
              .button_style(components::ButtonStyle::plain())
              .id("control"));
    }

   private:
    std::reference_wrapper<const composition::Observable<bool>> moved_;
  };

  void SetUp() override {
    const provider::SurfaceSize extent{
        .width = provider::SurfaceWidth{width},
        .height = provider::SurfaceHeight{height}};
    providers_.surface().resize(extent);
    providers_.surface().set_scale(provider::SurfaceScale::unit());
  }

  auto gpu() -> NullGpu& { return providers_.gpu(); }

  auto start() -> Application {
    auto created = Application::create(providers_, Stage{moved_});
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

  void settle(Application& application) {
    for (int index = 0; index < settle_limit; ++index) {
      frames(application, 1);
      if (!application.last_frame().presented) {
        return;
      }
    }
    ADD_FAILURE() << "the application never went idle";
  }

  auto presents_over(Application& application, int count) -> std::size_t {
    const std::size_t before = gpu().frames_presented();
    frames(application, count);
    return gpu().frames_presented() - before;
  }

  static auto counter(std::atomic<int>& calls) -> NullGpu::FrameHookFunction {
    return [&calls](const NullGpu::FrameCanvas&, const provider::FrameInfo&) {
      calls.fetch_add(1);
    };
  }

  static auto at(const Canvas& canvas, std::uint32_t x, std::uint32_t y)
      -> std::array<float, 3> {
    const auto texel = canvas.texel(static_cast<std::int64_t>(x),
                                    static_cast<std::int64_t>(y));
    return {texel[0], texel[1], texel[2]};
  }

  static auto pixel(const provider::Image& image, std::uint32_t x,
                    std::uint32_t y) -> std::array<int, 3> {
    const auto found = image.pixel(x, y);
    EXPECT_TRUE(found.has_value());
    if (!found.has_value()) {
      return {};
    }
    return {(*found)[0], (*found)[1], (*found)[2]};
  }

  provider::headless::HeadlessProviderFactory providers_;
  composition::Observable<bool> moved_{false};
};

TEST_F(FrameHookTest, HookRunsOncePerPresentedFrame) {
  std::atomic<int> calls{0};
  const provider::FrameHook hook =
      gpu().add_frame_hook(provider::FrameStage::before_ui, counter(calls));
  const std::size_t before = gpu().frames_presented();
  Application application = start();
  moved_.set(true);
  frames(application, 3);
  settle(application);
  EXPECT_GT(gpu().frames_presented(), before);
  EXPECT_EQ(static_cast<std::size_t>(calls.load()),
            gpu().frames_presented() - before);
}

TEST_F(FrameHookTest, StagesRunBeforeAndAfterTheUi) {
  gpu().rasterize(
      provider::SurfaceSize{.width = provider::SurfaceWidth{width},
                            .height = provider::SurfaceHeight{height}});
  std::vector<std::string_view> order;
  std::optional<std::array<float, 3>> under;
  std::optional<std::array<float, 3>> over;
  std::optional<provider::FrameInfo> seen;
  const provider::FrameHook after = gpu().add_frame_hook(
      provider::FrameStage::after_ui,
      [&](const NullGpu::FrameCanvas& canvas, const provider::FrameInfo&) {
        order.emplace_back("after");
        if (canvas.has_value()) {
          over = at(canvas->get(), width / 2, height / 2);
        }
      });
  const provider::FrameHook before = gpu().add_frame_hook(
      provider::FrameStage::before_ui,
      [&](const NullGpu::FrameCanvas& canvas, const provider::FrameInfo& info) {
        order.emplace_back("before");
        seen = info;
        if (canvas.has_value()) {
          under = at(canvas->get(), width / 2, height / 2);
        }
      });
  Application application = start();
  ASSERT_GE(order.size(), 2U);
  EXPECT_EQ(order[0], "before");
  EXPECT_EQ(order[1], "after");
  ASSERT_TRUE(under.has_value());
  ASSERT_TRUE(over.has_value());
  EXPECT_GT((*under)[0], 0.9F);
  EXPECT_GT((*under)[2], 0.9F);
  EXPECT_GT((*over)[1], 0.6F);
  EXPECT_LT((*over)[0], 0.3F);
  ASSERT_TRUE(seen.has_value());
  EXPECT_EQ(seen->extent.width.value(), width);
  EXPECT_EQ(seen->extent.height.value(), height);
  EXPECT_FALSE(seen->replay);
  EXPECT_EQ(seen->serial, gpu().frame_schedule()->get().serial());
  EXPECT_GT(seen->predicted_present, 0ns);
}

TEST_F(FrameHookTest, HooksOfAStageRunInRegistrationOrder) {
  std::vector<std::string_view> order;
  const auto named = [&order](std::string_view name) {
    return
        [&order, name](const NullGpu::FrameCanvas&,
                       const provider::FrameInfo&) { order.push_back(name); };
  };
  const provider::FrameHook first =
      gpu().add_frame_hook(provider::FrameStage::before_ui, named("first"));
  const provider::FrameHook late =
      gpu().add_frame_hook(provider::FrameStage::after_ui, named("late"));
  const provider::FrameHook second =
      gpu().add_frame_hook(provider::FrameStage::before_ui, named("second"));
  Application application = start();
  settle(application);
  order.clear();
  first.request_frame();
  frames(application, 2);
  EXPECT_EQ(order, (std::vector<std::string_view>{"first", "second", "late"}));
}

TEST_F(FrameHookTest, ContinuousHookKeepsPresentingWhileTheTreeIsIdle) {
  Application application = start();
  settle(application);
  EXPECT_EQ(presents_over(application, 5), 0U);
  std::atomic<int> calls{0};
  std::optional<provider::FrameHook> hook = gpu().add_frame_hook(
      provider::FrameStage::before_ui, counter(calls),
      provider::FrameHookOptions{.drive = provider::FrameDrive::continuous,
                                 .frame_rate = std::nullopt});
  EXPECT_EQ(presents_over(application, 10), 10U);
  EXPECT_EQ(calls.load(), 10);
  hook.reset();
  settle(application);
  EXPECT_EQ(presents_over(application, 5), 0U);
  EXPECT_EQ(calls.load(), 10);
}

TEST_F(FrameHookTest, ContinuousHookHonoursItsFrameRate) {
  Application application = start();
  settle(application);
  std::atomic<int> calls{0};
  const provider::FrameHook hook = gpu().add_frame_hook(
      provider::FrameStage::before_ui, counter(calls),
      provider::FrameHookOptions{.drive = provider::FrameDrive::continuous,
                                 .frame_rate = 30.0});
  const std::size_t presented = presents_over(application, 20);
  EXPECT_GE(presented, 8U);
  EXPECT_LE(presented, 12U);
}

TEST_F(FrameHookTest, OnDemandHookWithoutARequestLetsTheAppIdle) {
  std::atomic<int> calls{0};
  const provider::FrameHook hook =
      gpu().add_frame_hook(provider::FrameStage::before_ui, counter(calls));
  Application application = start();
  settle(application);
  const int settled = calls.load();
  EXPECT_EQ(presents_over(application, 10), 0U);
  EXPECT_EQ(calls.load(), settled);
  EXPECT_TRUE(application.waiting());
}

TEST_F(FrameHookTest, RequestFrameGivesExactlyOneFrame) {
  std::atomic<int> calls{0};
  const provider::FrameHook hook =
      gpu().add_frame_hook(provider::FrameStage::after_ui, counter(calls));
  Application application = start();
  settle(application);
  const int settled = calls.load();
  hook.request_frame();
  EXPECT_EQ(presents_over(application, 5), 1U);
  EXPECT_EQ(calls.load(), settled + 1);
}

TEST_F(FrameHookTest, SwitchingToOnDemandStopsPresenting) {
  std::atomic<int> calls{0};
  const provider::FrameHook hook = gpu().add_frame_hook(
      provider::FrameStage::before_ui, counter(calls),
      provider::FrameHookOptions{.drive = provider::FrameDrive::continuous,
                                 .frame_rate = std::nullopt});
  Application application = start();
  EXPECT_EQ(presents_over(application, 4), 4U);
  hook.set_drive(provider::FrameDrive::on_demand);
  EXPECT_EQ(hook.drive(), provider::FrameDrive::on_demand);
  settle(application);
  EXPECT_EQ(presents_over(application, 6), 0U);
}

TEST_F(FrameHookTest, DestroyingAHookUnregistersIt) {
  std::atomic<int> kept{0};
  std::atomic<int> dropped{0};
  const provider::FrameHook keeper =
      gpu().add_frame_hook(provider::FrameStage::before_ui, counter(kept));
  std::optional<provider::FrameHook> doomed =
      gpu().add_frame_hook(provider::FrameStage::before_ui, counter(dropped));
  Application application = start();
  settle(application);
  const int before = dropped.load();
  EXPECT_GT(before, 0);
  doomed.reset();
  EXPECT_FALSE(gpu().frame_schedule()->get().demand().continuous);
  keeper.request_frame();
  frames(application, 2);
  EXPECT_EQ(dropped.load(), before);
  EXPECT_EQ(kept.load(), before + 1);
  provider::FrameHook source =
      gpu().add_frame_hook(provider::FrameStage::after_ui, counter(dropped));
  const provider::FrameHook target = std::move(source);
  EXPECT_TRUE(target.active());
}

TEST_F(FrameHookTest, HookContentShowsThroughATransparentUi) {
  const provider::SurfaceSize extent{.width = provider::SurfaceWidth{width},
                                     .height = provider::SurfaceHeight{height}};
  gpu().rasterize(extent);
  const provider::FrameHook gradient = gpu().add_frame_hook(
      provider::FrameStage::before_ui,
      [](const NullGpu::FrameCanvas& canvas, const provider::FrameInfo& info) {
        if (!canvas.has_value()) {
          return;
        }
        const std::uint32_t across = info.extent.width.value();
        for (std::uint32_t x = 0; x < across; ++x) {
          const float level =
              static_cast<float>(x) / static_cast<float>(across - 1);
          canvas->get().fill(
              provider::ClipRect{.x = x,
                                 .y = 0,
                                 .width = 1,
                                 .height = info.extent.height.value()},
              provider::Colour{.red = level,
                               .green = 0.0F,
                               .blue = 1.0F - level,
                               .alpha = 1.0F});
        }
      });
  Application application = start();
  settle(application);
  const auto image = gpu().capture();
  ASSERT_TRUE(image.has_value());
  const std::array<int, 3> left = pixel(*image, 4, 4);
  const std::array<int, 3> right = pixel(*image, width - 5, height - 5);
  const std::array<int, 3> middle = pixel(*image, width / 2, height / 2);
  EXPECT_LE(left[0], 12);
  EXPECT_GE(left[2], 243);
  EXPECT_GE(right[0], 243);
  EXPECT_LE(right[2], 12);
  EXPECT_LE(std::abs(middle[0] - 0x20), 2);
  EXPECT_LE(std::abs(middle[1] - 0xC0), 2);
  EXPECT_LE(std::abs(middle[2] - 0x40), 2);
  const std::uint32_t beside = (width / 2) - 34;
  const std::array<int, 3> edge = pixel(*image, beside, height / 2);
  const auto expected = static_cast<int>(255.0F * static_cast<float>(beside) /
                                         static_cast<float>(width - 1));
  EXPECT_LE(std::abs(edge[0] - expected), 3);
  EXPECT_EQ(edge[1], 0);
}

TEST_F(FrameHookTest, HooksChurnSafelyWhileTheRenderThreadReplays) {
  std::atomic<int> calls{0};
  std::atomic<int> replays{0};
  const provider::FrameHook steady =
      gpu().add_frame_hook(provider::FrameStage::before_ui,
                           [&calls, &replays](const NullGpu::FrameCanvas&,
                                              const provider::FrameInfo& info) {
                             calls.fetch_add(1);
                             if (info.replay) {
                               replays.fetch_add(1);
                             }
                           });
  Application application = start();
  motion::with_animation(motion::Animation::linear(1.0),
                         [this] { moved_.set(true); });
  frames(application, 2);
  std::atomic<int> churned{0};
  std::atomic<int> churn_calls{0};
  const std::size_t presented = gpu().frames_presented();
  const int counted = calls.load();
  {
    auto churn = providers_.threading().start(
        provider::ThreadSpec{.name = "test-hook-churn",
                             .role = provider::ThreadRole::app},
        [&](const std::stop_token& stop) {
          while (!stop.stop_requested()) {
            const provider::FrameHook added = gpu().add_frame_hook(
                churned.load() % 2 == 0 ? provider::FrameStage::before_ui
                                        : provider::FrameStage::after_ui,
                counter(churn_calls),
                provider::FrameHookOptions{
                    .drive = churned.load() % 3 == 0
                                 ? provider::FrameDrive::continuous
                                 : provider::FrameDrive::on_demand,
                    .frame_rate = std::nullopt});
            added.request_frame();
            churned.fetch_add(1);
          }
        });
    ASSERT_TRUE(churn.has_value());
    for (int index = 0; index < 30; ++index) {
      ASSERT_TRUE(providers_.clock().advance(16ms).has_value());
      application.step_animations();
    }
  }
  EXPECT_GT(application.replayed_frames(), 0U);
  EXPECT_GT(replays.load(), 0);
  EXPECT_GT(churned.load(), 0);
  EXPECT_EQ(static_cast<std::size_t>(calls.load() - counted),
            gpu().frames_presented() - presented);
}

}
