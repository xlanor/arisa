#include <arisa/components/button.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/provider/gpu/frame_hook.hpp>
#include <arisa/provider/gpu/frame_info.hpp>
#include <arisa/provider/gpu/shape_vertex.hpp>
#include <arisa/provider/gpu/vertex.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/headless/null_gpu.hpp>
#include <arisa/provider/input/provider.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/runtime/application_options.hpp>
#include <arisa/runtime/frame_timings.hpp>
#include <arisa/theme/colour.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

namespace arisa::runtime {

using namespace std::chrono_literals;

class FramePacingTest : public ::testing::Test {
 protected:
  static constexpr std::chrono::nanoseconds period{16'666'667};
  static constexpr std::uint32_t off_rgb = 0x2050C0;
  static constexpr std::uint32_t on_rgb = 0xC05020;
  static constexpr std::uint32_t presses = 20;

  class Toggle final : public composition::Component {
   public:
    explicit Toggle(composition::Observable<bool>& lit) noexcept : lit_(lit) {}

    auto operator==(const Toggle& other) const -> bool {
      return &lit_.get() == &other.lit_.get();
    }

    auto body(composition::Context& context) const
        -> composition::Element override {
      const bool lit = context.read(lit_.get());
      return components::VStack(
          components::Button("Toggle",
                             [observable = &lit_.get()] {
                               observable->set(!observable->get());
                             }),
          components::VStack()
              .frame({.width = 40.0F, .height = 40.0F})
              .background(theme::Colour::hex(lit ? on_rgb : off_rgb)));
    }

   private:
    std::reference_wrapper<composition::Observable<bool>> lit_;
  };

  class Movers final : public composition::Component {
   public:
    Movers(const composition::Observable<bool>& slow,
           const composition::Observable<bool>& fast) noexcept
        : slow_(slow), fast_(fast) {}

    auto operator==(const Movers& other) const -> bool {
      return &slow_.get() == &other.slow_.get() &&
             &fast_.get() == &other.fast_.get();
    }

    auto body(composition::Context& context) const
        -> composition::Element override {
      const bool slow = context.read(slow_.get());
      const bool fast = context.read(fast_.get());
      return components::VStack(components::VStack()
                                    .frame({.width = 30.0F, .height = 30.0F})
                                    .background(theme::Colour::hex(off_rgb))
                                    .offset(slow ? 120.0F : 0.0F, 0.0F),
                                components::VStack()
                                    .frame({.width = 30.0F, .height = 30.0F})
                                    .background(theme::Colour::hex(on_rgb))
                                    .offset(fast ? 120.0F : 0.0F, 0.0F));
    }

   private:
    std::reference_wrapper<const composition::Observable<bool>> slow_;
    std::reference_wrapper<const composition::Observable<bool>> fast_;
  };

  struct Run {
    double latency_frames{0.0};
    double build_phase{0.0};
    std::uint32_t measured{0};
  };

  static auto colour(std::uint32_t rgb) -> std::uint32_t {
    return provider::Vertex::rgba(static_cast<std::uint8_t>(rgb >> 16U),
                                  static_cast<std::uint8_t>(rgb >> 8U),
                                  static_cast<std::uint8_t>(rgb), 0);
  }

  static auto lit(provider::headless::NullGpu& gpu) -> std::optional<bool> {
    const std::uint32_t mask = colour(0xFFFFFF);
    for (const provider::ShapeVertex& vertex : gpu.last_shapes().vertices) {
      if (vertex.kind != provider::ShapeKind::fill) {
        continue;
      }
      if ((vertex.colour & mask) == colour(on_rgb)) {
        return true;
      }
      if ((vertex.colour & mask) == colour(off_rgb)) {
        return false;
      }
    }
    return std::nullopt;
  }

  static void hold(provider::headless::HeadlessProviderFactory& providers) {
    provider::InputFrame held;
    held.pads[0] = provider::PadState{.connected = true,
                                      .buttons = {provider::Button::a},
                                      .left = {},
                                      .right = {}};
    providers.input().push(std::move(held));
  }

  static void reach(provider::headless::HeadlessProviderFactory& providers,
                    std::chrono::nanoseconds at) {
    const std::chrono::nanoseconds now = providers.clock().now().since_origin();
    if (at > now) {
      ASSERT_TRUE(providers.clock().advance(at - now).has_value());
    }
  }

  static auto after_vblank(std::chrono::nanoseconds now)
      -> std::chrono::nanoseconds {
    return (((now / period) + 1) * period) + 1us;
  }

  static auto play(bool paced) -> Run {
    provider::headless::HeadlessProviderFactory providers;
    providers.gpu().display(period);
    composition::Observable<bool> on{false};
    const provider::FrameHook hook = providers.gpu().add_frame_hook(
        provider::FrameStage::before_ui,
        [](provider::headless::NullGpu::FrameCanvas,
           const provider::FrameInfo&) {},
        provider::FrameHookOptions{.drive = provider::FrameDrive::continuous,
                                   .frame_rate = std::nullopt});
    auto created = Application::create(providers, Toggle{on});
    EXPECT_TRUE(created.has_value());
    Application application = std::move(*created);
    const auto next = [&]() {
      const std::chrono::nanoseconds now =
          providers.clock().now().since_origin();
      if (paced) {
        return application.next_build().value_or(after_vblank(now));
      }
      return after_vblank(now);
    };
    for (int index = 0; index < 20; ++index) {
      reach(providers, next());
      EXPECT_TRUE(application.frame().has_value());
    }
    Run run;
    double latency = 0.0;
    double phase = 0.0;
    std::uint32_t phases = 0;
    std::optional<bool> shown = lit(providers.gpu());
    for (std::uint32_t press = 0; press < presses; ++press) {
      const std::int64_t slot =
          (providers.clock().now().since_origin() / period) + 6;
      const std::chrono::nanoseconds released =
          (slot * period) - period + (period * ((2 * (press % 5)) + 1) / 10);
      bool seen = false;
      for (int frame = 0; frame < 24 && !seen; ++frame) {
        const std::chrono::nanoseconds at = next();
        if (at < released && at + (3 * period) >= released) {
          hold(providers);
        }
        reach(providers, at);
        EXPECT_TRUE(application.frame().has_value());
        const FrameTimings& timings = application.last_frame();
        if (timings.presented) {
          phase += static_cast<double>((timings.sampled % period).count()) /
                   static_cast<double>(period.count());
          ++phases;
        }
        const std::optional<bool> now_lit = lit(providers.gpu());
        if (at >= released && now_lit != shown && timings.displayed) {
          latency +=
              static_cast<double>((*timings.displayed - released).count()) /
              static_cast<double>(period.count());
          ++run.measured;
          shown = now_lit;
          seen = true;
        }
      }
    }
    run.latency_frames = run.measured > 0 ? latency / run.measured : 0.0;
    run.build_phase = phases > 0 ? phase / phases : 0.0;
    return run;
  }
};

TEST_F(FramePacingTest, EarlierInputShowsAPressInTheFrameThatSampledIt) {
  provider::headless::HeadlessProviderFactory providers;
  composition::Observable<bool> on{false};
  auto created = Application::create(providers, Toggle{on});
  ASSERT_TRUE(created.has_value());
  Application application = std::move(*created);
  for (int index = 0; index < 4; ++index) {
    ASSERT_TRUE(providers.clock().advance(16ms).has_value());
    ASSERT_TRUE(application.frame().has_value());
  }
  for (int round = 0; round < 3; ++round) {
    const std::optional<bool> before = lit(providers.gpu());
    ASSERT_TRUE(before.has_value());
    hold(providers);
    ASSERT_TRUE(providers.clock().advance(16ms).has_value());
    ASSERT_TRUE(application.frame().has_value());
    EXPECT_EQ(lit(providers.gpu()), before);
    ASSERT_TRUE(providers.clock().advance(16ms).has_value());
    ASSERT_TRUE(application.frame().has_value());
    EXPECT_TRUE(application.last_frame().presented);
    EXPECT_GT(application.last_frame().evaluations, 0U);
    EXPECT_EQ(lit(providers.gpu()), std::optional<bool>{!*before});
    EXPECT_EQ(on.get(), !*before);
    for (int index = 0; index < 3; ++index) {
      ASSERT_TRUE(providers.clock().advance(16ms).has_value());
      ASSERT_TRUE(application.frame().has_value());
    }
  }
}

TEST_F(FramePacingTest, WaitingAtTheStartMovesTheBuildLaterAndCutsLatency) {
  const Run eager = play(false);
  const Run paced = play(true);
  ASSERT_EQ(eager.measured, presses);
  ASSERT_EQ(paced.measured, presses);
  EXPECT_LT(eager.build_phase, 0.1);
  EXPECT_GT(paced.build_phase, 0.75);
  EXPECT_NEAR(eager.latency_frames, 1.5, 0.2);
  EXPECT_LT(paced.latency_frames, 0.8);
  EXPECT_GT(eager.latency_frames - paced.latency_frames, 0.75);
}

TEST_F(FramePacingTest, HalfRateHintsPresentWithSwapIntervalTwo) {
  provider::headless::HeadlessProviderFactory providers;
  composition::Observable<bool> slow{false};
  composition::Observable<bool> fast{false};
  auto created = Application::create(providers, Movers{slow, fast});
  ASSERT_TRUE(created.has_value());
  Application application = std::move(*created);
  std::uint32_t shown = 0;
  const auto frames = [&](int count) {
    for (int index = 0; index < count; ++index) {
      ASSERT_TRUE(providers.clock().advance(16ms).has_value());
      ASSERT_TRUE(application.frame().has_value());
      if (application.last_frame().presented) {
        shown = application.last_frame().swap_interval;
      }
    }
  };
  frames(3);
  EXPECT_EQ(providers.gpu().swap_interval(), 1U);
  const std::size_t changes = providers.gpu().swap_interval_changes();
  motion::with_animation(
      motion::Animation::linear(2.0).preferred_frame_rate(30.0),
      [&slow] { slow.set(true); });
  frames(8);
  EXPECT_EQ(providers.gpu().swap_interval(), 2U);
  EXPECT_EQ(shown, 2U);
  EXPECT_EQ(providers.gpu().swap_interval_changes(), changes + 1);
  motion::with_animation(motion::Animation::linear(0.5),
                         [&fast] { fast.set(true); });
  frames(4);
  EXPECT_EQ(providers.gpu().swap_interval(), 1U);
  EXPECT_EQ(shown, 1U);
  EXPECT_EQ(providers.gpu().swap_interval_changes(), changes + 2);
}

}
