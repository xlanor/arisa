#include <arisa/motion/animation.hpp>
#include <arisa/motion/unit_curve.hpp>

#include <gtest/gtest.h>

#include <cmath>

namespace arisa::motion {

class AnimationTest : public ::testing::Test {
 protected:
  static auto span() -> Animation::Span {
    return Animation::Span{.from = AnimatableVector{0.0},
                           .to = AnimatableVector{1.0},
                           .velocity = AnimatableVector{},
                           .precision = 1.0e-3};
  }

  static auto at(const Animation& animation, double elapsed) -> double {
    return animation.sample(span(), elapsed).value[0];
  }

  static auto reference(double c0x, double c0y, double c1x, double c1y,
                        double progress) -> double {
    const auto point = [](double first, double second, double time) {
      const double inverse = 1.0 - time;
      return (3.0 * inverse * inverse * time * first) +
             (3.0 * inverse * time * time * second) + (time * time * time);
    };
    double best = 0.0;
    double error = 2.0;
    constexpr int samples = 200000;
    for (int index = 0; index <= samples; ++index) {
      const double time = static_cast<double>(index) / samples;
      const double distance = std::abs(point(c0x, c1x, time) - progress);
      if (distance < error) {
        error = distance;
        best = point(c0y, c1y, time);
      }
    }
    return best;
  }
};

TEST_F(AnimationTest, TimingCurvesMatchAReferenceBezierSolve) {
  for (const double progress : {0.1, 0.25, 0.5, 0.75, 0.9}) {
    EXPECT_NEAR(UnitCurve::ease_in_out.value(progress),
                reference(0.42, 0.0, 0.58, 1.0, progress), 1.0e-4);
    EXPECT_NEAR(UnitCurve::ease_in.value(progress),
                reference(0.42, 0.0, 1.0, 1.0, progress), 1.0e-4);
    EXPECT_NEAR(UnitCurve::ease_out.value(progress),
                reference(0.0, 0.0, 0.58, 1.0, progress), 1.0e-4);
    EXPECT_NEAR(UnitCurve::bezier(0.2, 0.9, 0.1, 1.2).value(progress),
                reference(0.2, 0.9, 0.1, 1.2, progress), 1.0e-4);
    EXPECT_NEAR(UnitCurve::linear.value(progress), progress, 1.0e-6);
  }
  EXPECT_DOUBLE_EQ(UnitCurve::ease_in_out.value(0.0), 0.0);
  EXPECT_DOUBLE_EQ(UnitCurve::ease_in_out.value(1.0), 1.0);
  EXPECT_NEAR(UnitCurve::ease_in_out.value(0.5), 0.5, 1.0e-6);
}

TEST_F(AnimationTest, TimedAnimationsRunForTheirDurationThenFinish) {
  const Animation animation = Animation::ease_in_out(0.4);
  EXPECT_NEAR(at(animation, 0.2), 0.5, 1.0e-6);
  EXPECT_FALSE(animation.sample(span(), 0.39).finished);
  const Animation::Sample done = animation.sample(span(), 0.4);
  EXPECT_TRUE(done.finished);
  EXPECT_DOUBLE_EQ(done.value[0], 1.0);
  EXPECT_DOUBLE_EQ(Animation::linear().cycle_duration(), 0.35);
  EXPECT_NEAR(at(Animation::timing_curve(0.0, 0.0, 1.0, 1.0, 2.0), 0.5), 0.25,
              1.0e-6);
}

TEST_F(AnimationTest, DelayHoldsTheStartValue) {
  const Animation animation = Animation::linear(1.0).delay(0.5);
  EXPECT_DOUBLE_EQ(at(animation, 0.25), 0.0);
  EXPECT_FALSE(animation.sample(span(), 0.25).finished);
  EXPECT_NEAR(at(animation, 1.0), 0.5, 1.0e-9);
  EXPECT_TRUE(animation.sample(span(), 1.5).finished);
  EXPECT_DOUBLE_EQ(animation.delay(0.25).delay_seconds(), 0.75);
}

TEST_F(AnimationTest, SpeedScalesElapsedTime) {
  const Animation animation = Animation::linear(1.0).speed(2.0);
  EXPECT_NEAR(at(animation, 0.25), 0.5, 1.0e-9);
  EXPECT_TRUE(animation.sample(span(), 0.5).finished);
  EXPECT_NEAR(animation.sample(span(), 0.25).velocity[0], 2.0, 1.0e-6);
  const Animation slowed = Animation::linear(1.0).delay(1.0).speed(0.5);
  EXPECT_NEAR(at(slowed, 2.0), 0.5, 1.0e-9);
}

TEST_F(AnimationTest, RepeatCountAutoreversesAndThenLandsOnTheTarget) {
  const Animation animation = Animation::linear(1.0).repeat_count(3);
  EXPECT_NEAR(at(animation, 0.5), 0.5, 1.0e-6);
  EXPECT_NEAR(at(animation, 1.25), 0.75, 1.0e-6);
  EXPECT_NEAR(at(animation, 2.25), 0.25, 1.0e-6);
  EXPECT_LT(animation.sample(span(), 1.25).velocity[0], 0.0);
  EXPECT_TRUE(animation.sample(span(), 3.0).finished);
  EXPECT_DOUBLE_EQ(at(animation, 3.0), 1.0);
  const Animation forward = Animation::linear(1.0).repeat_count(2, false);
  EXPECT_NEAR(at(forward, 1.25), 0.25, 1.0e-6);
}

TEST_F(AnimationTest, RepeatForeverNeverFinishes) {
  const Animation animation = Animation::ease_in(0.5).repeat_forever();
  EXPECT_FALSE(animation.sample(span(), 1000.25).finished);
  ASSERT_TRUE(animation.repetition().has_value());
  EXPECT_FALSE(animation.repetition()->count.has_value());
  const Animation spun = Animation::spring().repeat_forever(false);
  EXPECT_FALSE(spun.sample(span(), 100.0).finished);
}

TEST_F(AnimationTest, ReducedMotionTurnsSpringsIntoShortFades) {
  const Animation reduced = Animation::bouncy().delay(0.1).reduced();
  ASSERT_FALSE(reduced.spring_parameters().has_value());
  EXPECT_DOUBLE_EQ(reduced.cycle_duration(), Animation::cross_fade_duration);
  EXPECT_DOUBLE_EQ(reduced.delay_seconds(), 0.1);
  EXPECT_EQ(Animation::linear(2.0).reduced(), Animation::linear(2.0));
}

}
