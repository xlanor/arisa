#include <arisa/components/environment.hpp>
#include <arisa/components/for_each.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/transition.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/provider/gpu/vertex.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/runtime/frame_timings.hpp>
#include <arisa/runtime/root.hpp>
#include <arisa/theme/colour.hpp>

#include <gtest/gtest.h>

#include <runtime/frame_loop.hpp>

#include <boost/sml.hpp>

#include <chrono>
#include <cstdint>
#include <functional>
#include <ranges>
#include <vector>

namespace arisa::runtime {

using namespace std::chrono_literals;

class MotionRuntimeTest : public ::testing::Test {
 protected:
  static constexpr std::uint32_t box_rgb = 0x102030;
  static constexpr std::uint32_t row_rgb = 0x405060;

  class Stage final : public composition::Component {
   public:
    Stage(const composition::Observable<bool>& wide,
          const composition::Observable<int>& rows,
          motion::Transition transition)
        : wide_(wide), rows_(rows), transition_(transition) {}

    auto operator==(const Stage& other) const -> bool {
      return &wide_.get() == &other.wide_.get() &&
             &rows_.get() == &other.rows_.get() &&
             transition_ == other.transition_;
    }

    auto body(composition::Context& context) const
        -> composition::Element override {
      const bool wide = context.read(wide_.get());
      const int rows = context.read(rows_.get());
      const motion::Transition transition = transition_;
      return components::VStack(
          components::VStack()
              .frame({.width = wide ? 400.0F : 200.0F, .height = 40.0F})
              .background(theme::Colour::hex(box_rgb))
              .id("box"),
          components::ForEach(
              std::views::iota(0, rows), [](int row) { return row; },
              [transition](int) {
                return components::VStack()
                    .frame({.width = 100.0F, .height = 20.0F})
                    .background(theme::Colour::hex(row_rgb))
                    .transition(transition);
              }));
    }

   private:
    std::reference_wrapper<const composition::Observable<bool>> wide_;
    std::reference_wrapper<const composition::Observable<int>> rows_;
    motion::Transition transition_;
  };

  auto start(bool reduce = false,
             motion::Transition transition = motion::Transition::opacity())
      -> Application {
    auto created = Application::create(
        providers_,
        components::VStack(Stage{wide_, rows_, transition})
            .environment(components::Environment::reduce_motion, reduce));
    EXPECT_TRUE(created.has_value());
    return std::move(*created);
  }

  auto start_cross_fading(motion::Transition transition) -> Application {
    auto created = Application::create(
        providers_,
        components::VStack(Stage{wide_, rows_, transition})
            .environment(components::Environment::
                             accessibility_prefers_cross_fade_transitions,
                         true));
    EXPECT_TRUE(created.has_value());
    return std::move(*created);
  }

  static void frame(Application& application) {
    ASSERT_TRUE(application.frame().has_value());
  }

  void advance(std::chrono::nanoseconds step) {
    ASSERT_TRUE(providers_.clock().advance(step).has_value());
  }

  static auto colour(std::uint32_t rgb) -> std::uint32_t {
    return provider::Vertex::rgba(static_cast<std::uint8_t>(rgb >> 16U),
                                  static_cast<std::uint8_t>(rgb >> 8U),
                                  static_cast<std::uint8_t>(rgb), 0);
  }

  auto fills(std::uint32_t rgb) -> std::vector<provider::ShapeVertex> {
    const std::uint32_t mask = colour(0xffffff);
    std::vector<provider::ShapeVertex> found;
    for (const provider::ShapeVertex& vertex :
         providers_.gpu().last_shapes().vertices) {
      if (vertex.kind == provider::ShapeKind::fill &&
          (vertex.colour & mask) == colour(rgb)) {
        found.push_back(vertex);
      }
    }
    return found;
  }

  auto box_half_width() -> float {
    const auto found = fills(box_rgb);
    EXPECT_EQ(found.size(), 4U);
    return found.empty() ? 0.0F : found.front().half_size[0];
  }

  static auto alpha(const provider::ShapeVertex& vertex) -> std::uint32_t {
    return (vertex.colour & ~colour(0xffffff)) >> 24U;
  }

  provider::headless::HeadlessProviderFactory providers_;
  composition::Observable<bool> wide_{false};
  composition::Observable<int> rows_{0};
};

TEST_F(MotionRuntimeTest, FrameAnimationInterpolatesWhileLayoutRunsOnce) {
  Application application = start();
  frame(application);
  EXPECT_FLOAT_EQ(box_half_width(), 100.0F);
  motion::with_animation(motion::Animation::linear(1.0),
                         [this] { wide_.set(true); });
  frame(application);
  EXPECT_TRUE(application.last_frame().laid_out);
  EXPECT_EQ(application.last_frame().kind, FrameKind::update);
  EXPECT_TRUE(application.last_frame().animating);
  EXPECT_FLOAT_EQ(box_half_width(), 100.0F);
  advance(500ms);
  frame(application);
  EXPECT_FALSE(application.last_frame().laid_out);
  EXPECT_EQ(application.last_frame().kind, FrameKind::animation);
  EXPECT_NEAR(box_half_width(), 150.0F, 0.6F);
  advance(600ms);
  frame(application);
  EXPECT_FLOAT_EQ(box_half_width(), 200.0F);
  EXPECT_FALSE(application.last_frame().animating);
}

TEST_F(MotionRuntimeTest, AnimationOnlyFramesSkipReconcileAndLayout) {
  Application application = start();
  frame(application);
  motion::with_animation(motion::Animation::spring(),
                         [this] { wide_.set(true); });
  frame(application);
  const std::uint64_t evaluations = application.root().tree().evaluations();
  const std::uint64_t revision = application.root().tree().revision();
  const std::size_t presented = providers_.gpu().frames_presented();
  for (int step = 0; step < 10; ++step) {
    advance(16ms);
    frame(application);
    EXPECT_EQ(application.last_frame().kind, FrameKind::animation);
    EXPECT_FALSE(application.last_frame().laid_out);
    EXPECT_TRUE(application.last_frame().presented);
  }
  EXPECT_EQ(application.root().tree().evaluations(), evaluations);
  EXPECT_EQ(application.root().tree().revision(), revision);
  EXPECT_EQ(providers_.gpu().frames_presented(), presented + 10);
}

TEST_F(MotionRuntimeTest, IdleFramesPresentNothingAndTheLoopWaits) {
  Application application = start();
  frame(application);
  EXPECT_FALSE(application.waiting());
  const std::size_t presented = providers_.gpu().frames_presented();
  const std::size_t allocations = providers_.gpu().geometry_allocations();
  for (int step = 0; step < 5; ++step) {
    advance(16ms);
    frame(application);
    EXPECT_EQ(application.last_frame().kind, FrameKind::idle);
    EXPECT_FALSE(application.last_frame().presented);
    EXPECT_TRUE(application.waiting());
  }
  EXPECT_EQ(providers_.gpu().frames_presented(), presented);
  EXPECT_EQ(providers_.gpu().geometry_allocations(), allocations);
  application.wait();
  rows_.set(1);
  frame(application);
  EXPECT_TRUE(application.last_frame().presented);
  EXPECT_FALSE(application.waiting());
}

TEST_F(MotionRuntimeTest, CompletionFiresOnceAfterTheAnimationSettles) {
  Application application = start();
  frame(application);
  int completed = 0;
  motion::with_animation(
      motion::Animation::linear(0.5), [this] { wide_.set(true); },
      [&completed] { ++completed; });
  frame(application);
  advance(300ms);
  frame(application);
  EXPECT_EQ(completed, 0);
  advance(300ms);
  frame(application);
  EXPECT_FALSE(application.last_frame().animating);
  frame(application);
  EXPECT_EQ(completed, 1);
  for (int step = 0; step < 3; ++step) {
    frame(application);
  }
  EXPECT_EQ(completed, 1);
}

TEST_F(MotionRuntimeTest, TransitionsInsertAndHoldDepartingNodesUntilSettled) {
  Application application = start();
  frame(application);
  motion::with_animation(motion::Animation::linear(1.0),
                         [this] { rows_.set(2); });
  frame(application);
  EXPECT_TRUE(fills(row_rgb).empty());
  advance(500ms);
  frame(application);
  const auto halfway = fills(row_rgb);
  ASSERT_EQ(halfway.size(), 8U);
  EXPECT_NEAR(static_cast<double>(alpha(halfway.front())), 128.0, 2.0);
  advance(600ms);
  frame(application);
  ASSERT_EQ(fills(row_rgb).size(), 8U);
  EXPECT_EQ(alpha(fills(row_rgb).front()), 255U);
  motion::with_animation(motion::Animation::linear(1.0),
                         [this] { rows_.set(1); });
  frame(application);
  EXPECT_EQ(application.root().tree().departing(), 1U);
  EXPECT_EQ(application.root().tree().transitions_held(), 1U);
  EXPECT_EQ(fills(row_rgb).size(), 8U);
  advance(500ms);
  frame(application);
  EXPECT_EQ(application.root().tree().departing(), 1U);
  const auto leaving = fills(row_rgb);
  ASSERT_EQ(leaving.size(), 8U);
  EXPECT_NEAR(static_cast<double>(alpha(leaving.back())), 128.0, 2.0);
  advance(600ms);
  frame(application);
  frame(application);
  EXPECT_EQ(application.root().tree().departing(), 0U);
  EXPECT_EQ(application.root().tree().transitions_held(), 0U);
  EXPECT_EQ(fills(row_rgb).size(), 4U);
}

TEST_F(MotionRuntimeTest, RemovalWithoutAnimationRetiresAtOnce) {
  Application application = start();
  rows_.set(2);
  frame(application);
  rows_.set(0);
  frame(application);
  EXPECT_EQ(application.root().tree().departing(), 0U);
  EXPECT_TRUE(fills(row_rgb).empty());
}

TEST_F(MotionRuntimeTest, ReduceMotionSnapsGeometryAndFadesInsteadOfMoving) {
  Application application =
      start(true, motion::Transition::move(theme::Edge::leading));
  frame(application);
  motion::with_animation(motion::Animation::bouncy(), [this] {
    wide_.set(true);
    rows_.set(1);
  });
  frame(application);
  EXPECT_FLOAT_EQ(box_half_width(), 200.0F);
  advance(100ms);
  frame(application);
  const auto row = fills(row_rgb);
  ASSERT_EQ(row.size(), 4U);
  EXPECT_GT(alpha(row.front()), 0U);
  EXPECT_LT(alpha(row.front()), 255U);
  EXPECT_FLOAT_EQ(row.front().position[0], 589.0F);
  advance(200ms);
  frame(application);
  EXPECT_EQ(alpha(fills(row_rgb).front()), 255U);
  EXPECT_FALSE(application.last_frame().animating);
}

TEST_F(MotionRuntimeTest, CrossFadePreferenceFadesInsteadOfMoving) {
  Application application =
      start_cross_fading(motion::Transition::move(theme::Edge::leading));
  frame(application);
  motion::with_animation(motion::Animation::ease_in_out(0.3),
                         [this] { rows_.set(1); });
  frame(application);
  advance(100ms);
  frame(application);
  const auto row = fills(row_rgb);
  ASSERT_EQ(row.size(), 4U);
  EXPECT_GT(alpha(row.front()), 0U);
  EXPECT_LT(alpha(row.front()), 255U);
  EXPECT_FLOAT_EQ(row.front().position[0], 589.0F);
  advance(300ms);
  frame(application);
  EXPECT_EQ(alpha(fills(row_rgb).front()), 255U);
}

TEST(FrameLoopTest, DrivesEveryRowOfTheTable) {
  using Testing = BasicFrameLoop<boost::sml::testing>;
  for (const bool waiting : {false, true}) {
    Testing woken;
    woken.start_waiting(waiting);
    EXPECT_EQ(woken.wake(), waiting);
    EXPECT_FALSE(woken.waiting());
    Testing rested;
    rested.start_waiting(waiting);
    EXPECT_EQ(rested.rest(), !waiting);
    EXPECT_TRUE(rested.waiting());
  }
  FrameLoop loop;
  EXPECT_FALSE(loop.waiting());
  EXPECT_EQ(loop.plan({.work = false, .animating = false}), FrameKind::idle);
  EXPECT_TRUE(loop.waiting());
  EXPECT_EQ(loop.plan({.work = false, .animating = true}),
            FrameKind::animation);
  EXPECT_FALSE(loop.waiting());
  EXPECT_EQ(loop.plan({.work = true, .animating = true}), FrameKind::update);
  EXPECT_EQ(loop.plan({.work = true, .animating = false}), FrameKind::update);
}

}
