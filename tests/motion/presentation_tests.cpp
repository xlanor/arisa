#include <arisa/motion/presentation.hpp>
#include <arisa/motion/transaction.hpp>
#include <arisa/motion/transaction_stack.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/provider/standard/std_threading.hpp>
#include <arisa/theme/colour.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <stop_token>

namespace arisa::motion {

using namespace std::chrono_literals;

class PresentationTest : public ::testing::Test {
 protected:
  using Property = Presentation::Property;
  using Tint = Presentation::Tint;

  static auto moment(const Transaction& transaction,
                     std::chrono::nanoseconds now, bool reduce = false)
      -> Presentation::Moment {
    return Presentation::Moment{
        .transaction = transaction, .now = now, .reduce_motion = reduce};
  }
};

TEST_F(PresentationTest, FirstValueAndUnanimatedChangesSnap) {
  Presentation presentation;
  const Transaction none;
  EXPECT_EQ(presentation.animate(Property::opacity, AnimatableVector{0.25},
                                 moment(none, 0ms)),
            AnimatableVector{0.25});
  EXPECT_EQ(presentation.animate(Property::opacity, AnimatableVector{0.75},
                                 moment(none, 1ms)),
            AnimatableVector{0.75});
  EXPECT_FALSE(presentation.animating());
}

TEST_F(PresentationTest, AnimatedChangesInterpolateThenSettleExactly) {
  Presentation presentation;
  const Transaction linear{Animation::linear(1.0)};
  presentation.animate(Property::frame, AnimatableVector{0.0, 0.0, 100.0, 50.0},
                       moment(linear, 0ms));
  const AnimatableVector start = presentation.animate(
      Property::frame, AnimatableVector{100.0, 0.0, 300.0, 50.0},
      moment(linear, 0ms));
  EXPECT_EQ(start, AnimatableVector(0.0, 0.0, 100.0, 50.0));
  EXPECT_TRUE(presentation.in_flight(Property::frame));
  const AnimatableVector half = presentation.animate(
      Property::frame, AnimatableVector{100.0, 0.0, 300.0, 50.0},
      moment(linear, 500ms));
  EXPECT_NEAR(half[0], 50.0, 1.0e-6);
  EXPECT_NEAR(half[2], 200.0, 1.0e-6);
  const AnimatableVector done = presentation.animate(
      Property::frame, AnimatableVector{100.0, 0.0, 300.0, 50.0},
      moment(linear, 1s));
  EXPECT_EQ(done, AnimatableVector(100.0, 0.0, 300.0, 50.0));
  EXPECT_FALSE(presentation.animating());
}

TEST_F(PresentationTest, ColoursInterpolateInOklab) {
  Presentation presentation;
  const Transaction linear{Animation::linear(1.0)};
  const theme::Colour black{0.0F, 0.0F, 0.0F, 1.0F};
  const theme::Colour white{1.0F, 1.0F, 1.0F, 1.0F};
  presentation.animate(Tint::background, black, moment(linear, 0ms));
  presentation.animate(Tint::background, white, moment(linear, 0ms));
  const theme::Colour middle =
      presentation.animate(Tint::background, white, moment(linear, 500ms));
  EXPECT_NEAR(middle.red, 0.389F, 0.01F);
  EXPECT_NEAR(middle.green, middle.blue, 1.0e-3F);
  EXPECT_EQ(presentation.animate(Tint::background, white, moment(linear, 1s)),
            white);
}

TEST_F(PresentationTest, ReduceMotionSnapsGeometryAndFadesColour) {
  Presentation presentation;
  const Transaction spring{Animation::bouncy()};
  presentation.animate(Property::offset, AnimatableVector{0.0},
                       moment(spring, 0ms, true));
  EXPECT_EQ(presentation.animate(Property::offset, AnimatableVector{40.0},
                                 moment(spring, 0ms, true)),
            AnimatableVector{40.0});
  EXPECT_FALSE(presentation.in_flight(Property::offset));
  presentation.animate(Property::opacity, AnimatableVector{0.0},
                       moment(spring, 0ms, true));
  presentation.animate(Property::opacity, AnimatableVector{1.0},
                       moment(spring, 0ms, true));
  EXPECT_TRUE(presentation.in_flight(Property::opacity));
  presentation.animate(Property::opacity, AnimatableVector{1.0},
                       moment(spring, 250ms, true));
  EXPECT_FALSE(presentation.in_flight(Property::opacity));
}

TEST_F(PresentationTest, InsertionAndRemovalRunTheTransition) {
  Presentation presentation;
  const Transaction linear{Animation::linear(1.0)};
  presentation.insert(Transition::opacity(), linear);
  const auto first = presentation.appearance(moment(linear, 0ms));
  ASSERT_TRUE(first.has_value());
  EXPECT_FLOAT_EQ(first->opacity, 0.0F);
  const auto half = presentation.appearance(moment(linear, 500ms));
  ASSERT_TRUE(half.has_value());
  EXPECT_NEAR(half->opacity, 0.5F, 1.0e-5F);
  EXPECT_FALSE(presentation.appearance(moment(linear, 1s)).has_value());
  presentation.remove(Transition::move(theme::Edge::leading), linear);
  EXPECT_FALSE(presentation.removal_finished());
  const auto left = presentation.appearance(moment(linear, 1s));
  ASSERT_TRUE(left.has_value());
  EXPECT_FLOAT_EQ(left->relative_x, 0.0F);
  const auto leaving = presentation.appearance(moment(linear, 1250ms));
  ASSERT_TRUE(leaving.has_value());
  EXPECT_NEAR(leaving->relative_x, -0.25F, 1.0e-5F);
  EXPECT_FALSE(presentation.removal_finished());
  presentation.appearance(moment(linear, 2s));
  EXPECT_TRUE(presentation.removal_finished());
  EXPECT_FALSE(presentation.animating());
}

TEST_F(PresentationTest, CompletionRunsOnceEveryTrackSettles) {
  int completed = 0;
  auto queue = std::make_shared<CompletionQueue>();
  Presentation presentation;
  {
    Transaction transaction{Animation::linear(1.0)};
    transaction.add_animation_completion([&completed] { ++completed; });
    transaction.attach(queue);
    presentation.animate(Property::opacity, AnimatableVector{0.0},
                         moment(transaction, 0ms));
    presentation.animate(Property::opacity, AnimatableVector{1.0},
                         moment(transaction, 0ms));
    presentation.animate(Property::scale, AnimatableVector{1.0, 1.0},
                         moment(transaction, 0ms));
    presentation.animate(Property::scale, AnimatableVector{2.0, 2.0},
                         moment(transaction, 500ms));
  }
  const Transaction none;
  presentation.animate(Property::opacity, AnimatableVector{1.0},
                       moment(none, 1s));
  EXPECT_FALSE(queue->pending());
  presentation.animate(Property::scale, AnimatableVector{2.0, 2.0},
                       moment(none, 1200ms));
  EXPECT_FALSE(queue->pending());
  presentation.animate(Property::scale, AnimatableVector{2.0, 2.0},
                       moment(none, 1500ms));
  EXPECT_TRUE(queue->pending());
  EXPECT_EQ(queue->drain(), 1U);
  EXPECT_EQ(completed, 1);
  EXPECT_EQ(queue->drain(), 0U);
}

TEST(TransactionStackTest, WithAnimationScopesTheAmbientTransaction) {
  EXPECT_FALSE(TransactionStack::current().has_value());
  with_animation(Animation::snappy(), [] {
    const auto outer = TransactionStack::current();
    ASSERT_TRUE(outer.has_value());
    EXPECT_EQ(outer->get().animation(), Animation::snappy());
    Transaction quiet;
    quiet.set_disables_animations(true);
    with_transaction(quiet, [] {
      const auto inner = TransactionStack::current();
      ASSERT_TRUE(inner.has_value());
      EXPECT_FALSE(inner->get().animation().has_value());
      EXPECT_TRUE(inner->get().disables_animations());
    });
    EXPECT_EQ(TransactionStack::current()->get().animation(),
              Animation::snappy());
    bool elsewhere = true;
    provider::standard::StdThreadingProvider threading;
    {
      auto other = threading.start(
          provider::ThreadSpec{.name = "elsewhere",
                               .role = provider::ThreadRole::background},
          [&elsewhere](std::stop_token) {
            elsewhere = TransactionStack::current().has_value();
          });
      ASSERT_TRUE(other.has_value());
    }
    EXPECT_FALSE(elsewhere);
  });
  EXPECT_FALSE(TransactionStack::current().has_value());
  EXPECT_EQ(with_animation(Animation::linear(), [] { return 7; }), 7);
}

TEST(TransactionStackTest, UnclaimedCompletionRunsWhenTheTransactionEnds) {
  int completed = 0;
  with_animation(Animation::linear(), [] {}, [&completed] { ++completed; });
  EXPECT_EQ(completed, 1);
}

}

namespace arisa::motion {

TEST_F(PresentationTest, FrameRateHintsReachThePresentation) {
  Presentation presentation;
  const Transaction hinted{
      Animation::linear(1.0).repeat_forever(false).preferred_frame_rate(30.0)};
  presentation.animate(Property::rotation, AnimatableVector{0.0},
                       moment(hinted, 0ms));
  presentation.animate(Property::rotation, AnimatableVector{6.0},
                       moment(hinted, 0ms));
  ASSERT_TRUE(presentation.animating());
  EXPECT_EQ(presentation.frame_rate(), 30.0);
  const Transaction plain{Animation::linear(1.0)};
  presentation.animate(Property::opacity, AnimatableVector{0.0},
                       moment(plain, 0ms));
  presentation.animate(Property::opacity, AnimatableVector{1.0},
                       moment(plain, 0ms));
  EXPECT_FALSE(presentation.frame_rate().has_value());
  EXPECT_FALSE(Animation::linear(1.0).frame_rate().has_value());
  EXPECT_FALSE(Animation::linear(1.0)
                   .preferred_frame_rate(0.0)
                   .frame_rate()
                   .has_value());
}

TEST_F(PresentationTest, SuspendedAnimationsResumeWhereTheyPaused) {
  Presentation presentation;
  const Transaction linear{Animation::linear(1.0)};
  presentation.animate(Property::offset, AnimatableVector{0.0, 0.0},
                       moment(linear, 0ms));
  presentation.animate(Property::offset, AnimatableVector{100.0, 0.0},
                       moment(linear, 0ms));
  const AnimatableVector paused = presentation.animate(
      Property::offset, AnimatableVector{100.0, 0.0}, moment(linear, 250ms));
  presentation.suspend(250ms);
  EXPECT_TRUE(presentation.suspended());
  presentation.suspend(400ms);
  presentation.resume(2250ms);
  EXPECT_FALSE(presentation.suspended());
  const AnimatableVector resumed = presentation.animate(
      Property::offset, AnimatableVector{100.0, 0.0}, moment(linear, 2250ms));
  EXPECT_NEAR(resumed[0], paused[0], 1.0e-6);
  const AnimatableVector later = presentation.animate(
      Property::offset, AnimatableVector{100.0, 0.0}, moment(linear, 2500ms));
  EXPECT_NEAR(later[0], 50.0, 1.0e-6);
  EXPECT_TRUE(presentation.in_flight(Property::offset));
}

}
