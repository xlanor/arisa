#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/container_background_placement.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/exclusive_frame.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/composition/overlay_layer.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/presentation/navigation_stack.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/provider/gpu/frame_hook.hpp>
#include <arisa/provider/gpu/frame_info.hpp>
#include <arisa/provider/gpu/image.hpp>
#include <arisa/provider/gpu/render_queue.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/headless/null_gpu.hpp>
#include <arisa/provider/input/provider.hpp>
#include <arisa/provider/surface/surface_scale.hpp>
#include <arisa/provider/surface/surface_size.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/runtime/application_options.hpp>
#include <arisa/structure/navigation_split_view.hpp>
#include <arisa/structure/scaffold.hpp>
#include <arisa/theme/color.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/shape.hpp>

#include <runtime/frame_loop.hpp>

#include <gtest/gtest.h>
#include <boost/sml.hpp>

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <map>
#include <optional>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>

namespace arisa::runtime {

using namespace std::chrono_literals;

class ExclusiveFrameTest : public ::testing::Test {
 protected:
  using NullGpu = provider::headless::NullGpu;
  using Layer = composition::OverlayLayer;
  using Placement = composition::ContainerBackgroundPlacement;

  static constexpr std::uint32_t width = 160;
  static constexpr std::uint32_t height = 96;
  static constexpr std::uint32_t control_rgb = 0x20C040;
  static constexpr std::uint32_t hud_rgb = 0xE0A010;
  static constexpr std::uint32_t toast_rgb = 0x1060E0;
  static constexpr std::uint32_t dialog_rgb = 0xA02080;
  static constexpr std::uint32_t base_rgb = 0xC03030;
  static constexpr std::uint32_t mover_rgb = 0x2050C0;
  static constexpr int settle_limit = 40;

  struct Flags {
    composition::Observable<bool> exclusive{false};
    composition::Observable<bool> dialog{false};
    composition::Observable<bool> mounted{true};
    composition::Observable<bool> moved{false};
    composition::Observable<bool> base{false};
    composition::Observable<bool> glass{false};
    std::vector<Layer> keep;
    std::atomic<int> control_taps{0};
    std::atomic<int> dialog_taps{0};
    std::atomic<int> exits{0};
  };

  class Stage final : public composition::Component {
   public:
    explicit Stage(Flags& flags) : flags_(flags) {}

    auto operator==(const Stage& other) const -> bool {
      return &flags_.get() == &other.flags_.get();
    }

    auto body(composition::Context& context) const
        -> composition::Element override {
      Flags& flags = flags_;
      const bool exclusive = context.read(flags.exclusive);
      const bool dialog = context.read(flags.dialog);
      const bool mounted = context.read(flags.mounted);
      const bool moved = context.read(flags.moved);
      const bool base = context.read(flags.base);
      const bool glass = context.read(flags.glass);
      composition::Element content =
          components::ZStack(
              components::VStack()
                  .frame({.width = 8.0F, .height = 8.0F})
                  .background(theme::Colour::hex(mover_rgb))
                  .offset(moved ? 60.0F : 0.0F, 40.0F),
              block(control_rgb, 48.0F, 24.0F,
                    [&flags] { flags.control_taps.fetch_add(1); })
                  .id("control"),
              glass ? composition::Element{components::ZStack()
                                               .frame({.width = 40.0F,
                                                       .height = 40.0F})
                                               .glass_effect(
                                                   theme::Glass::regular(),
                                                   theme::Shape::rounded(8.0F))
                                               .offset(-50.0F, 0.0F)}
                    : composition::Element{components::ZStack()})
              .frame({.max_width = composition::infinity,
                      .max_height = composition::infinity});
      if (mounted) {
        content = std::move(content).exclusive_frame(exclusive, flags.keep);
      }
      content =
          std::move(content)
              .on_exit_command("Leave", [&flags] { flags.exits.fetch_add(1); })
              .overlay(true, Layer::hud, swatch(hud_rgb, -60.0F, -30.0F))
              .overlay(true, Layer::toast, swatch(toast_rgb, 60.0F, -30.0F))
              .overlay(dialog, Layer::dialog,
                       block(dialog_rgb, 32.0F, 16.0F,
                             [&flags] { flags.dialog_taps.fetch_add(1); })
                           .id("dialog")
                           .offset(0.0F, 34.0F));
      if (base) {
        content = std::move(content).container_background(
            theme::Colour::hex(base_rgb), Placement::window);
      }
      return content;
    }

   private:
    static auto swatch(std::uint32_t rgb, float x, float y)
        -> composition::Element {
      return components::VStack()
          .frame({.width = 16.0F, .height = 16.0F})
          .background(theme::Colour::hex(rgb))
          .offset(x, y);
    }

    static auto block(std::uint32_t rgb, float across, float down,
                      std::function<void()> action) -> composition::Element {
      return components::Button([action = std::move(action)] { action(); },
                                components::ZStack()
                                    .frame({.width = across, .height = down})
                                    .background(theme::Colour::hex(rgb)))
          .button_style(components::ButtonStyle::plain());
    }

    std::reference_wrapper<Flags> flags_;
  };

  void SetUp() override { prepare(providers_); }

  static void prepare(provider::headless::HeadlessProviderFactory& providers) {
    const provider::SurfaceSize extent{
        .width = provider::SurfaceWidth{width},
        .height = provider::SurfaceHeight{height}};
    providers.surface().resize(extent);
    providers.surface().set_scale(provider::SurfaceScale::unit());
    providers.gpu().rasterize(extent);
  }

  auto gpu() -> NullGpu& { return providers_.gpu(); }

  auto start(composition::Element content) -> Application {
    auto created = Application::create(providers_, std::move(content));
    EXPECT_TRUE(created.has_value());
    Application application = std::move(*created);
    frames(application, 2);
    return application;
  }

  auto start() -> Application { return start(Stage{flags_}); }

  void frames(Application& application, int count) {
    frames(providers_, application, count);
  }

  static void frames(provider::headless::HeadlessProviderFactory& providers,
                     Application& application, int count) {
    for (int index = 0; index < count; ++index) {
      ASSERT_TRUE(providers.clock().advance(16ms).has_value());
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

  void tap(Application& application, float x, float y) {
    provider::InputFrame down;
    down.touches.push_back(
        provider::Touch{.id = provider::TouchId{3},
                        .position = provider::PixelPoint{.x = x, .y = y},
                        .phase = provider::TouchPhase::began});
    provider::InputFrame up;
    up.touches.push_back(
        provider::Touch{.id = provider::TouchId{3},
                        .position = provider::PixelPoint{.x = x, .y = y},
                        .phase = provider::TouchPhase::ended});
    providers_.input().push(down);
    providers_.input().push(up);
    frames(application, 3);
  }

  void press(Application& application, provider::Button button) {
    provider::InputFrame held;
    held.pads[0] = provider::PadState{
        .connected = true, .buttons = {button}, .left = {}, .right = {}};
    providers_.input().push(held);
    providers_.input().push(provider::InputFrame{});
    frames(application, 3);
  }

  auto colour(std::uint32_t x, std::uint32_t y) -> std::array<int, 3> {
    const auto image = gpu().capture();
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

  static auto near(const std::array<int, 3>& seen, std::uint32_t rgb,
                   int tolerance = 3) -> bool {
    const std::array<int, 3> wanted{static_cast<int>((rgb >> 16U) & 0xFFU),
                                    static_cast<int>((rgb >> 8U) & 0xFFU),
                                    static_cast<int>(rgb & 0xFFU)};
    for (std::size_t index = 0; index < 3; ++index) {
      if (std::abs(seen[index] - wanted[index]) > tolerance) {
        return false;
      }
    }
    return true;
  }

  static auto focused(Application& application) -> std::string {
    const auto id = application.focused();
    if (!id.has_value()) {
      return "none";
    }
    const auto node = application.root().tree().node(*id);
    if (!node.has_value() || !node->get().key().has_value()) {
      return "unkeyed";
    }
    return node->get().key()->description();
  }

  static auto hook_counter(std::atomic<int>& calls)
      -> NullGpu::FrameHookFunction {
    return [&calls](const NullGpu::FrameCanvas&, const provider::FrameInfo&) {
      calls.fetch_add(1);
    };
  }

  provider::headless::HeadlessProviderFactory providers_;
  Flags flags_;
};

TEST_F(ExclusiveFrameTest, NothingKeptRecordsZeroQuadsAndStillRunsHooks) {
  std::atomic<int> calls{0};
  const provider::FrameHook hook = gpu().add_frame_hook(
      provider::FrameStage::before_ui, hook_counter(calls));
  Application application = start();
  settle(application);
  hook.request_frame();
  frames(application, 1);
  EXPECT_GT(application.last_frame().quads, 0U);
  const int before = calls.load();
  composition::ExclusiveFrame scope = application.exclusive_frame({});
  frames(application, 1);
  EXPECT_TRUE(application.last_frame().presented);
  EXPECT_TRUE(application.last_frame().exclusive);
  EXPECT_EQ(application.last_frame().quads, 0U);
  EXPECT_EQ(application.last_frame().commands, 0U);
  EXPECT_EQ(calls.load(), before + 1);
  EXPECT_FALSE(near(colour(80, 48), control_rgb));
  EXPECT_FALSE(near(colour(20, 18), hud_rgb));
}

TEST_F(ExclusiveFrameTest,
       IdleExclusiveFrameWithAnOnDemandHookPresentsNothing) {
  std::atomic<int> calls{0};
  const provider::FrameHook hook = gpu().add_frame_hook(
      provider::FrameStage::before_ui, hook_counter(calls));
  Application application = start();
  settle(application);
  composition::ExclusiveFrame scope = application.exclusive_frame({});
  frames(application, 1);
  EXPECT_EQ(presents_over(application, 10), 0U);
  flags_.moved.set(true);
  EXPECT_EQ(presents_over(application, 4), 0U);
  hook.request_frame();
  EXPECT_EQ(presents_over(application, 4), 1U);
  EXPECT_EQ(application.last_frame().quads, 0U);
}

TEST_F(ExclusiveFrameTest, ContinuousHookDrivesTheExclusiveFrameRate) {
  const provider::FrameHook hook = gpu().add_frame_hook(
      provider::FrameStage::before_ui,
      [](const NullGpu::FrameCanvas&, const provider::FrameInfo&) {},
      provider::FrameHookOptions{.drive = provider::FrameDrive::continuous,
                                 .frame_rate = std::nullopt});
  Application application = start();
  composition::ExclusiveFrame scope = application.exclusive_frame({});
  EXPECT_EQ(presents_over(application, 6), 6U);
}

TEST_F(ExclusiveFrameTest, LeasedFrameParksArisaUntilReleased) {
  std::atomic<int> calls{0};
  const provider::FrameHook hook = gpu().add_frame_hook(
      provider::FrameStage::before_ui, hook_counter(calls),
      provider::FrameHookOptions{.drive = provider::FrameDrive::continuous,
                                 .frame_rate = std::nullopt});
  Application application = start();
  frames(application, 3);
  const auto queue = gpu().render_queue();
  ASSERT_TRUE(queue.has_value());
  composition::ExclusiveFrame scope =
      application.exclusive_frame({.keep = {}, .lease = true});
  EXPECT_TRUE(queue->get().parked());
  const int parked_calls = calls.load();
  EXPECT_EQ(presents_over(application, 6), 0U);
  EXPECT_TRUE(application.last_frame().leased);
  EXPECT_EQ(calls.load(), parked_calls);
  {
    std::optional<provider::RenderQueue::Lease> own = queue->get().try_lease();
    ASSERT_TRUE(own.has_value());
    EXPECT_TRUE(own->held());
  }
  scope.release();
  frames(application, 1);
  EXPECT_FALSE(queue->get().parked());
  EXPECT_TRUE(application.last_frame().presented);
  EXPECT_FALSE(application.last_frame().leased);
  EXPECT_FALSE(application.last_frame().exclusive);
  EXPECT_EQ(presents_over(application, 6), 6U);
  EXPECT_GT(calls.load(), parked_calls);
  EXPECT_EQ(queue->get().counters().overlaps, 0U);
  EXPECT_EQ(queue->get().counters().parks, 1U);
}

TEST_F(ExclusiveFrameTest, KeptToastAndHudStillPaintWhileContentIsHidden) {
  Application application = start();
  settle(application);
  EXPECT_TRUE(near(colour(80, 48), control_rgb));
  composition::ExclusiveFrame scope =
      application.exclusive_frame({.keep = {Layer::toast, Layer::hud}});
  frames(application, 1);
  EXPECT_GT(application.last_frame().quads, 0U);
  settle(application);
  EXPECT_TRUE(near(colour(20, 18), hud_rgb));
  EXPECT_TRUE(near(colour(140, 18), toast_rgb));
  EXPECT_FALSE(near(colour(80, 48), control_rgb));
}

TEST_F(ExclusiveFrameTest, KeptLayersStayHitTestableAndContentGetsNoInput) {
  Application application = start();
  settle(application);
  tap(application, 80.0F, 48.0F);
  EXPECT_EQ(flags_.control_taps.load(), 1);
  composition::ExclusiveFrame scope =
      application.exclusive_frame({.keep = {Layer::dialog}});
  settle(application);
  tap(application, 80.0F, 48.0F);
  EXPECT_EQ(flags_.control_taps.load(), 1);
  flags_.dialog.set(true);
  settle(application);
  EXPECT_TRUE(near(colour(80, 82), dialog_rgb));
  EXPECT_EQ(focused(application), "dialog");
  tap(application, 80.0F, 82.0F);
  EXPECT_EQ(flags_.dialog_taps.load(), 1);
  scope.release();
  flags_.dialog.set(false);
  settle(application);
  tap(application, 80.0F, 48.0F);
  EXPECT_EQ(flags_.control_taps.load(), 2);
}

TEST_F(ExclusiveFrameTest, FocusLeavesContentAndReturnsOnRelease) {
  Application application = start();
  settle(application);
  ASSERT_EQ(focused(application), "control");
  composition::ExclusiveFrame scope =
      application.exclusive_frame({.keep = {Layer::toast, Layer::hud}});
  frames(application, 2);
  EXPECT_EQ(focused(application), "none");
  press(application, provider::Button::a);
  EXPECT_EQ(flags_.control_taps.load(), 0);
  scope.release();
  frames(application, 2);
  EXPECT_EQ(focused(application), "control");
}

TEST_F(ExclusiveFrameTest, ReleaseRepaintsWholeOnceAndResumesRetainedPaint) {
  Application application = start();
  settle(application);
  composition::ExclusiveFrame scope = application.exclusive_frame({});
  settle(application);
  flags_.moved.set(true);
  settle(application);
  scope.release();
  frames(application, 1);
  const FrameTimings released = application.last_frame();
  EXPECT_TRUE(released.presented);
  EXPECT_FALSE(released.exclusive);
  EXPECT_EQ(released.evaluations, 0U);
  EXPECT_EQ(released.layout_computations, 0U);
  EXPECT_FALSE(gpu().last_damage().has_value());
  settle(application);
  flags_.moved.set(false);
  frames(application, 1);
  EXPECT_TRUE(application.last_frame().presented);
  EXPECT_GT(application.last_frame().spliced, 0U);
  ASSERT_TRUE(gpu().last_damage().has_value());
  EXPECT_LT(gpu().last_damage()->width * gpu().last_damage()->height,
            width * height);
}

TEST_F(ExclusiveFrameTest, TimersAndTasksKeepRunningWhileExclusive) {
  Application application = start();
  settle(application);
  composition::ExclusiveFrame scope = application.exclusive_frame({});
  settle(application);
  std::atomic<int> fired{0};
  std::atomic<int> posted{0};
  const async::Job timer = application.root().scheduler().after(
      20ms, [&fired](std::stop_token) { fired.fetch_add(1); });
  static_cast<void>(
      application.root().dispatcher().post([&posted] { posted.fetch_add(1); }));
  frames(application, 3);
  EXPECT_EQ(fired.load(), 1);
  EXPECT_EQ(posted.load(), 1);
  EXPECT_TRUE(application.last_frame().exclusive);
}

TEST_F(ExclusiveFrameTest, NestedScopesKeepTheIntersectionInAnyReleaseOrder) {
  Application application = start();
  settle(application);
  composition::ExclusiveFrame outer =
      application.exclusive_frame({.keep = {Layer::toast, Layer::hud}});
  composition::ExclusiveFrame inner =
      application.exclusive_frame({.keep = {Layer::hud}});
  settle(application);
  EXPECT_TRUE(near(colour(20, 18), hud_rgb));
  EXPECT_FALSE(near(colour(140, 18), toast_rgb));
  inner.release();
  settle(application);
  EXPECT_TRUE(near(colour(140, 18), toast_rgb));
  EXPECT_TRUE(application.last_frame().exclusive);
  EXPECT_EQ(focused(application), "none");
  composition::ExclusiveFrame again =
      application.exclusive_frame({.keep = {Layer::hud}});
  settle(application);
  outer.release();
  settle(application);
  EXPECT_FALSE(near(colour(140, 18), toast_rgb));
  EXPECT_EQ(focused(application), "none");
  again.release();
  settle(application);
  EXPECT_FALSE(application.last_frame().exclusive);
  EXPECT_TRUE(near(colour(80, 48), control_rgb));
  EXPECT_EQ(focused(application), "control");
}

TEST_F(ExclusiveFrameTest, ModifierHoldsTheFrameInNodeStateUntilItRetires) {
  flags_.keep = {Layer::hud};
  Application application = start();
  settle(application);
  flags_.exclusive.set(true);
  settle(application);
  EXPECT_TRUE(application.root().tree().exclusive().active());
  EXPECT_FALSE(near(colour(80, 48), control_rgb));
  EXPECT_TRUE(near(colour(20, 18), hud_rgb));
  press(application, provider::Button::b);
  EXPECT_EQ(flags_.exits.load(), 1);
  flags_.mounted.set(false);
  settle(application);
  EXPECT_FALSE(application.root().tree().exclusive().active());
  EXPECT_TRUE(near(colour(80, 48), control_rgb));
  EXPECT_EQ(focused(application), "control");
  flags_.mounted.set(true);
  settle(application);
  EXPECT_TRUE(application.root().tree().exclusive().active());
  flags_.exclusive.set(false);
  settle(application);
  EXPECT_FALSE(application.root().tree().exclusive().active());
}

TEST_F(ExclusiveFrameTest, ScopeOutlivingTheApplicationIsHarmless) {
  std::optional<composition::ExclusiveFrame> survivor;
  {
    Application application = start();
    survivor.emplace(application.exclusive_frame({}));
    frames(application, 1);
  }
  survivor.reset();
  SUCCEED();
}

TEST_F(ExclusiveFrameTest, BaseLayerSitsUnderContent) {
  flags_.base.set(true);
  Application application = start();
  settle(application);
  EXPECT_TRUE(near(colour(4, 90), base_rgb));
  EXPECT_TRUE(near(colour(80, 48), control_rgb));
  EXPECT_TRUE(near(colour(20, 18), hud_rgb));
}

TEST_F(ExclusiveFrameTest, GlassAboveTheBaseLayerRefractsIt) {
  flags_.glass.set(true);
  Application application = start();
  settle(application);
  const std::array<int, 3> without = colour(30, 48);
  flags_.base.set(true);
  settle(application);
  const std::array<int, 3> with = colour(30, 48);
  EXPECT_GT(with[0] - with[2], without[0] - without[2] + 40);
  EXPECT_FALSE(near(with, base_rgb, 1));
}

TEST_F(ExclusiveFrameTest, ExclusiveKeepListAppliesToTheBaseLayer) {
  flags_.base.set(true);
  Application application = start();
  settle(application);
  composition::ExclusiveFrame hidden = application.exclusive_frame({});
  settle(application);
  EXPECT_FALSE(near(colour(4, 90), base_rgb));
  hidden.release();
  composition::ExclusiveFrame kept =
      application.exclusive_frame({.keep = {Layer::base}});
  settle(application);
  EXPECT_TRUE(near(colour(4, 90), base_rgb));
  EXPECT_FALSE(near(colour(80, 48), control_rgb));
}

TEST_F(ExclusiveFrameTest, RetainedPaintMatchesTheFullWalkAcrossExclusiveMode) {
  provider::headless::HeadlessProviderFactory walked_providers;
  prepare(walked_providers);
  Flags walked_flags;
  auto retained_created = Application::create(providers_, Stage{flags_});
  auto walked_created =
      Application::create(walked_providers, Stage{walked_flags},
                          ApplicationOptions{.executors = std::nullopt,
                                             .fonts = {},
                                             .atlas = {},
                                             .interaction = {},
                                             .debug_full_walk = true});
  ASSERT_TRUE(retained_created.has_value());
  ASSERT_TRUE(walked_created.has_value());
  Application& retained = *retained_created;
  Application& walked = *walked_created;
  std::optional<composition::ExclusiveFrame> retained_scope;
  std::optional<composition::ExclusiveFrame> walked_scope;
  std::map<std::uint64_t, std::uint64_t> pairs;
  int compared = 0;
  int mismatched = 0;
  for (int frame = 0; frame < 48; ++frame) {
    if (frame == 8) {
      retained_scope.emplace(retained.exclusive_frame({.keep = {Layer::hud}}));
      walked_scope.emplace(walked.exclusive_frame({.keep = {Layer::hud}}));
    }
    if (frame == 14 || frame == 30) {
      flags_.moved.set(!flags_.moved.get());
      walked_flags.moved.set(!walked_flags.moved.get());
    }
    if (frame == 20) {
      retained_scope.reset();
      walked_scope.reset();
    }
    frames(providers_, retained, 1);
    frames(walked_providers, walked, 1);
    const bool shown = retained.last_frame().presented;
    if (shown != walked.last_frame().presented) {
      ++mismatched;
      continue;
    }
    if (!shown) {
      continue;
    }
    ++compared;
    const auto seen = gpu().last_shapes();
    const auto expected = walked_providers.gpu().last_shapes();
    bool same = seen.commands.size() == expected.commands.size() &&
                seen.indices == expected.indices &&
                seen.vertices == expected.vertices &&
                gpu().last_damage() == walked_providers.gpu().last_damage();
    for (std::size_t index = 0; same && index < seen.commands.size(); ++index) {
      provider::DrawCommand left = seen.commands[index];
      const provider::DrawCommand& right = expected.commands[index];
      const auto paired =
          pairs.try_emplace(left.texture.value(), right.texture.value());
      same = paired.first->second == right.texture.value();
      left.texture = right.texture;
      left.detail = right.detail;
      same = same && left == right;
    }
    mismatched += same ? 0 : 1;
  }
  EXPECT_EQ(mismatched, 0);
  EXPECT_GE(compared, 5);
}

class ContainerBackgroundTest : public ::testing::Test {
 protected:
  using Placement = composition::ContainerBackgroundPlacement;

  static constexpr std::uint32_t across = 640;
  static constexpr std::uint32_t down = 400;
  static constexpr std::uint32_t base_rgb = 0xC03030;
  static constexpr int frame_count = 40;

  static auto filled() -> composition::Element {
    return components::ZStack().frame({.max_width = composition::infinity,
                                       .max_height = composition::infinity});
  }

  static auto over_base(composition::Element content) -> composition::Element {
    return std::move(content).container_background(theme::Colour::hex(base_rgb),
                                                   Placement::window);
  }

  static auto centre_of(composition::Element content) -> std::array<int, 3> {
    provider::headless::HeadlessProviderFactory providers;
    const provider::SurfaceSize extent{.width = provider::SurfaceWidth{across},
                                       .height = provider::SurfaceHeight{down}};
    providers.surface().resize(extent);
    providers.surface().set_scale(provider::SurfaceScale::unit());
    providers.gpu().rasterize(extent);
    auto created = Application::create(providers, std::move(content));
    EXPECT_TRUE(created.has_value());
    if (!created.has_value()) {
      return {};
    }
    for (int index = 0; index < frame_count; ++index) {
      EXPECT_TRUE(providers.clock().advance(16ms).has_value());
      EXPECT_TRUE(created->frame().has_value());
    }
    const auto image = providers.gpu().capture();
    EXPECT_TRUE(image.has_value());
    if (!image.has_value()) {
      return {};
    }
    const auto found = image->pixel(across / 2, down / 2);
    EXPECT_TRUE(found.has_value());
    if (!found.has_value()) {
      return {};
    }
    return {(*found)[0], (*found)[1], (*found)[2]};
  }

  static auto base(const std::array<int, 3>& seen) -> bool {
    return std::abs(seen[0] - 0xC0) <= 3 && std::abs(seen[1] - 0x30) <= 3 &&
           std::abs(seen[2] - 0x30) <= 3;
  }

  static auto opaque(const std::array<int, 3>& seen) -> bool {
    return std::abs(seen[0] - seen[2]) < 40;
  }
};

class Covered final : public composition::Component {
 public:
  static constexpr composition::StateKey<bool> shown_key{"covered-shown", true};

  explicit Covered(bool clear) : clear_(clear) {}

  auto operator==(const Covered&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override {
    const auto shown = context.state(shown_key);
    const bool clear = clear_;
    return composition::Element{
        components::ZStack()
            .frame({.max_width = composition::infinity,
                    .max_height = composition::infinity})
            .full_screen_cover(
                shown,
                [clear] {
                  composition::Element panel = components::ZStack().frame(
                      {.max_width = composition::infinity,
                       .max_height = composition::infinity});
                  if (clear) {
                    panel = std::move(panel).container_background(
                        theme::Color::clear,
                        composition::ContainerBackgroundPlacement::sheet);
                  }
                  return panel;
                })}
        .container_background(
            theme::Colour::hex(0xC03030),
            composition::ContainerBackgroundPlacement::window);
  }

 private:
  bool clear_;
};

TEST_F(ContainerBackgroundTest,
       ClearScaffoldBackgroundLetsTheBaseLayerThrough) {
  const std::array<int, 3> standard =
      centre_of(over_base(structure::Scaffold(filled())));
  EXPECT_TRUE(opaque(standard));
  const std::array<int, 3> cleared = centre_of(over_base(
      composition::Element{structure::Scaffold(filled())}.container_background(
          theme::Color::clear, Placement::scaffold)));
  EXPECT_TRUE(base(cleared));
}

TEST_F(ContainerBackgroundTest,
       ClearNavigationBackgroundLetsTheBaseLayerThrough) {
  const auto navigated = [](bool clear) {
    return over_base(presentation::NavigationStack([clear] {
      composition::Element root = filled();
      if (clear) {
        root = std::move(root).container_background(theme::Color::clear,
                                                    Placement::navigation);
      }
      return root;
    }));
  };
  EXPECT_TRUE(opaque(centre_of(navigated(false))));
  EXPECT_TRUE(base(centre_of(navigated(true))));
  EXPECT_TRUE(base(centre_of(over_base(
      composition::Element{presentation::NavigationStack([] {
        return filled();
      })}.container_background(theme::Color::clear, Placement::navigation)))));
}

TEST_F(ContainerBackgroundTest, ClearCoverBackgroundLetsTheBaseLayerThrough) {
  EXPECT_TRUE(opaque(centre_of(Covered{false})));
  EXPECT_TRUE(base(centre_of(Covered{true})));
}

class ViewBackgroundTest : public ContainerBackgroundTest {
 protected:
  static constexpr std::uint32_t view_rgb = 0x30C040;

  static auto painted() -> composition::Element {
    return components::ZStack()
        .frame({.max_width = composition::infinity,
                .max_height = composition::infinity})
        .background(theme::Colour::hex(view_rgb));
  }

  static auto viewed(const std::array<int, 3>& seen) -> bool {
    return std::abs(seen[0] - 0x30) <= 3 && std::abs(seen[1] - 0xC0) <= 3 &&
           std::abs(seen[2] - 0x40) <= 3;
  }
};

class CoveredView final : public composition::Component {
 public:
  static constexpr composition::StateKey<bool> shown_key{"covered-view-shown",
                                                         true};

  auto operator==(const CoveredView&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override {
    const auto shown = context.state(shown_key);
    return components::ZStack()
        .frame({.max_width = composition::infinity,
                .max_height = composition::infinity})
        .full_screen_cover(shown, [] {
          return components::ZStack()
              .frame({.max_width = composition::infinity,
                      .max_height = composition::infinity})
              .container_background(
                  composition::ContainerBackgroundPlacement::sheet, [] {
                    return components::ZStack()
                        .frame({.max_width = composition::infinity,
                                .max_height = composition::infinity})
                        .background(theme::Colour::hex(0x30C040));
                  });
        });
  }
};

TEST_F(ViewBackgroundTest, AScaffoldDrawsTheViewForm) {
  EXPECT_TRUE(viewed(centre_of(
      composition::Element{structure::Scaffold(filled())}.container_background(
          Placement::scaffold, [] { return painted(); }))));
}

TEST_F(ViewBackgroundTest, ANavigationStackDrawsTheViewForm) {
  EXPECT_TRUE(viewed(centre_of(presentation::NavigationStack([] {
    return filled().container_background(Placement::navigation,
                                         [] { return painted(); });
  }))));
}

TEST_F(ViewBackgroundTest, ASheetDrawsTheViewForm) {
  EXPECT_TRUE(viewed(centre_of(CoveredView{})));
}

TEST_F(ViewBackgroundTest, TheWindowViewFormStillFillsTheBaseLayer) {
  EXPECT_TRUE(viewed(centre_of(filled().container_background(
      Placement::window, [] { return painted(); }))));
}

TEST_F(ContainerBackgroundTest,
       WindowStyleFillsTheBaseLayerAndDefaultsStayOpaque) {
  EXPECT_TRUE(base(centre_of(over_base(filled()))));
  EXPECT_FALSE(base(centre_of(filled())));
}

TEST(ExclusiveFrameLoopTest, DrivesEveryRowOfTheModeTable) {
  using Testing = BasicFrameLoop<boost::sml::testing>;
  for (const bool exclusive : {false, true}) {
    Testing entered;
    entered.start_exclusive(exclusive);
    EXPECT_EQ(entered.enter(), !exclusive);
    EXPECT_TRUE(entered.exclusive());
    Testing left;
    left.start_exclusive(exclusive);
    EXPECT_EQ(left.leave(), exclusive);
    EXPECT_FALSE(left.exclusive());
  }
  FrameLoop loop;
  EXPECT_FALSE(loop.exclusive());
  EXPECT_EQ(loop.plan({.work = false, .animating = false, .exclusive = true}),
            FrameKind::update);
  EXPECT_TRUE(loop.exclusive());
  EXPECT_EQ(loop.plan({.work = false, .animating = false, .exclusive = true}),
            FrameKind::idle);
  EXPECT_TRUE(loop.waiting());
  EXPECT_EQ(loop.plan({.work = false, .animating = true, .exclusive = true}),
            FrameKind::animation);
  EXPECT_EQ(loop.plan({.work = false, .animating = false, .exclusive = false}),
            FrameKind::update);
  EXPECT_FALSE(loop.exclusive());
}

}
