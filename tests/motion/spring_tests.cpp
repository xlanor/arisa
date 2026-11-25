#include <arisa/motion/animation.hpp>
#include <arisa/motion/animation_track.hpp>
#include <arisa/motion/spring.hpp>

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cmath>
#include <numbers>

namespace arisa::motion {

class SpringTest : public ::testing::Test {
 protected:
  struct Oscillator {
    double position;
    double velocity;
  };

  static auto integrate(const Spring& spring, Oscillator start, double until)
      -> Oscillator {
    const double natural = 2.0 * std::numbers::pi / spring.response();
    const double stiffness = natural * natural;
    const double damping = 2.0 * spring.damping_ratio() * natural;
    const auto acceleration = [&](double position, double velocity) {
      return (-stiffness * position) - (damping * velocity);
    };
    constexpr double step = 1.0e-5;
    Oscillator state = start;
    for (double time = 0.0; time < until - (step * 0.5); time += step) {
      const double k1x = state.velocity;
      const double k1v = acceleration(state.position, state.velocity);
      const double k2x = state.velocity + (0.5 * step * k1v);
      const double k2v = acceleration(state.position + (0.5 * step * k1x),
                                      state.velocity + (0.5 * step * k1v));
      const double k3x = state.velocity + (0.5 * step * k2v);
      const double k3v = acceleration(state.position + (0.5 * step * k2x),
                                      state.velocity + (0.5 * step * k2v));
      const double k4x = state.velocity + (step * k3v);
      const double k4v = acceleration(state.position + (step * k3x),
                                      state.velocity + (step * k3v));
      state.position += step / 6.0 * (k1x + (2.0 * k2x) + (2.0 * k3x) + k4x);
      state.velocity += step / 6.0 * (k1v + (2.0 * k2v) + (2.0 * k3v) + k4v);
    }
    return state;
  }

  static void expect_matches_integration(const Spring& spring) {
    for (const Oscillator start :
         {Oscillator{.position = 1.0, .velocity = 0.0},
          Oscillator{.position = -40.0, .velocity = 300.0},
          Oscillator{.position = 0.0, .velocity = -5.0}}) {
      for (const double time : {0.0, 0.05, 0.13, 0.37, 0.8}) {
        const Oscillator expected = integrate(spring, start, time);
        const Spring::State actual =
            spring.state(start.position, start.velocity, time);
        const double scale =
            std::max(1.0, std::abs(start.position) + std::abs(start.velocity));
        EXPECT_NEAR(actual.displacement, expected.position, 1.0e-6 * scale)
            << "t=" << time << " damping=" << spring.damping_ratio();
        EXPECT_NEAR(actual.velocity, expected.velocity, 1.0e-5 * scale)
            << "t=" << time << " damping=" << spring.damping_ratio();
      }
    }
  }
};

TEST_F(SpringTest, UnderdampedMatchesNumericalIntegration) {
  expect_matches_integration(
      Spring{Spring::Response{.response = 0.5, .damping_ratio = 0.3}});
}

TEST_F(SpringTest, CriticallyDampedMatchesNumericalIntegration) {
  expect_matches_integration(
      Spring{Spring::Response{.response = 0.4, .damping_ratio = 1.0}});
}

TEST_F(SpringTest, OverdampedMatchesNumericalIntegration) {
  expect_matches_integration(
      Spring{Spring::Response{.response = 0.6, .damping_ratio = 2.5}});
}

TEST_F(SpringTest, UnderdampedCrossesZeroAtTheDampedHalfPeriod) {
  const Spring spring{Spring::Response{.response = 1.0, .damping_ratio = 0.0}};
  EXPECT_NEAR(spring.state(1.0, 0.0, 0.5).displacement, -1.0, 1.0e-9);
  EXPECT_NEAR(spring.state(1.0, 0.0, 0.25).displacement, 0.0, 1.0e-9);
  EXPECT_NEAR(spring.state(1.0, 0.0, 1.0).displacement, 1.0, 1.0e-9);
}

TEST_F(SpringTest, CriticalAndOverdampedNeverOvershoot) {
  for (const double damping : {1.0, 1.6}) {
    const Spring spring{
        Spring::Response{.response = 0.3, .damping_ratio = damping}};
    for (double time = 0.0; time < 3.0; time += 0.01) {
      EXPECT_GE(spring.state(1.0, 0.0, time).displacement, -1.0e-12);
    }
  }
}

TEST_F(SpringTest, DurationAndBounceMapToResponseAndDamping) {
  const Spring bouncy{Spring::Duration{.duration = 0.4, .bounce = 0.3}};
  EXPECT_DOUBLE_EQ(bouncy.response(), 0.4);
  EXPECT_NEAR(bouncy.damping_ratio(), 0.7, 1.0e-12);
  EXPECT_NEAR(bouncy.bounce(), 0.3, 1.0e-12);
  const Spring flatter{Spring::Duration{.duration = 0.4, .bounce = -0.5}};
  EXPECT_NEAR(flatter.damping_ratio(), 2.0, 1.0e-12);
  const Spring physical{
      Spring::Physical{.mass = 1.0, .stiffness = 100.0, .damping = 10.0}};
  EXPECT_NEAR(physical.response(), 2.0 * std::numbers::pi / 10.0, 1.0e-12);
  EXPECT_NEAR(physical.damping_ratio(), 0.5, 1.0e-12);
}

TEST_F(SpringTest, PresetsMatchSwiftUIDefaults) {
  const auto parameters = [](const Animation& animation) {
    return animation.spring_parameters().value();
  };
  EXPECT_DOUBLE_EQ(parameters(Animation::spring()).response(), 0.5);
  EXPECT_DOUBLE_EQ(parameters(Animation::spring()).damping_ratio(), 1.0);
  EXPECT_DOUBLE_EQ(Animation::spring().blend_duration(), 0.0);
  EXPECT_DOUBLE_EQ(parameters(Animation::interactive_spring()).response(),
                   0.15);
  EXPECT_DOUBLE_EQ(parameters(Animation::interactive_spring()).damping_ratio(),
                   0.85);
  EXPECT_DOUBLE_EQ(Animation::interactive_spring().blend_duration(), 0.25);
  EXPECT_DOUBLE_EQ(parameters(Animation::smooth()).response(), 0.5);
  EXPECT_DOUBLE_EQ(parameters(Animation::smooth()).damping_ratio(), 1.0);
  EXPECT_DOUBLE_EQ(parameters(Animation::snappy()).response(), 0.5);
  EXPECT_NEAR(parameters(Animation::snappy()).damping_ratio(), 0.85, 1.0e-12);
  EXPECT_DOUBLE_EQ(parameters(Animation::bouncy()).response(), 0.5);
  EXPECT_NEAR(parameters(Animation::bouncy()).damping_ratio(), 0.7, 1.0e-12);
  EXPECT_NEAR(parameters(Animation::bouncy(0.3, 0.1)).damping_ratio(), 0.6,
              1.0e-12);
  EXPECT_DOUBLE_EQ(parameters(Animation::bouncy(0.3, 0.1)).response(), 0.3);
}

TEST_F(SpringTest, SpringSamplingIsFrameRateIndependent) {
  const Animation animation = Animation::bouncy();
  const Animation::Span span{.from = AnimatableVector{0.0},
                             .to = AnimatableVector{100.0},
                             .velocity = AnimatableVector{},
                             .precision = 1.0e-2};
  const AnimationTrack track{animation, span, std::chrono::nanoseconds{0},
                             nullptr};
  const double coarse = track.sample(std::chrono::milliseconds{300}).value[0];
  double fine = 0.0;
  for (int step = 1; step <= 300; ++step) {
    fine = track.sample(std::chrono::milliseconds{step}).value[0];
  }
  EXPECT_DOUBLE_EQ(coarse, fine);
}

TEST_F(SpringTest, RetargetingKeepsPositionAndVelocityContinuous) {
  const Animation animation = Animation::spring();
  const AnimationTrack first{animation,
                             Animation::Span{.from = AnimatableVector{0.0},
                                             .to = AnimatableVector{100.0},
                                             .velocity = AnimatableVector{},
                                             .precision = 1.0e-2},
                             std::chrono::nanoseconds{0}, nullptr};
  const std::chrono::nanoseconds retarget = std::chrono::milliseconds{120};
  const Animation::Sample before = first.sample(retarget);
  ASSERT_FALSE(before.finished);
  ASSERT_GT(std::abs(before.velocity[0]), 1.0);
  const AnimationTrack second{animation,
                              Animation::Span{.from = before.value,
                                              .to = AnimatableVector{-50.0},
                                              .velocity = before.velocity,
                                              .precision = 1.0e-2},
                              retarget, nullptr};
  const Animation::Sample after = second.sample(retarget);
  EXPECT_NEAR(after.value[0], before.value[0], 1.0e-9);
  EXPECT_NEAR(after.velocity[0], before.velocity[0], 1.0e-9);
  const std::chrono::nanoseconds epsilon = std::chrono::microseconds{10};
  const double slope = (second.sample(retarget + epsilon).value[0] -
                        second.sample(retarget).value[0]) /
                       std::chrono::duration<double>(epsilon).count();
  EXPECT_NEAR(slope, before.velocity[0], std::abs(before.velocity[0]) * 1.0e-2);
}

TEST_F(SpringTest, SettlesWithinPrecision) {
  const Animation animation = Animation::smooth();
  const AnimationTrack track{animation,
                             Animation::Span{.from = AnimatableVector{0.0},
                                             .to = AnimatableVector{1.0},
                                             .velocity = AnimatableVector{},
                                             .precision = 1.0e-3},
                             std::chrono::nanoseconds{0}, nullptr};
  EXPECT_FALSE(track.sample(std::chrono::milliseconds{200}).finished);
  const Animation::Sample settled = track.sample(std::chrono::seconds{3});
  EXPECT_TRUE(settled.finished);
  EXPECT_EQ(settled.value, AnimatableVector{1.0});
}

}
