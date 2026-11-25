#include <arisa/motion/angle.hpp>
#include <arisa/motion/cubic_keyframe.hpp>
#include <arisa/motion/keyframe_sequence.hpp>
#include <arisa/motion/keyframe_timeline.hpp>
#include <arisa/motion/keyframe_track.hpp>
#include <arisa/motion/keyframes.hpp>
#include <arisa/motion/linear_keyframe.hpp>
#include <arisa/motion/motion_path.hpp>
#include <arisa/motion/move_keyframe.hpp>
#include <arisa/motion/spring.hpp>
#include <arisa/motion/spring_keyframe.hpp>
#include <arisa/motion/unit_curve.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <initializer_list>
#include <numbers>
#include <vector>

namespace arisa::motion {

class KeyframeTest : public ::testing::Test {
 protected:
  struct Values {
    double scale{1.0};
    double lift{0.0};
    Angle spin{};
  };

  template <class... Keyframes>
  static auto sequence(double initial, const Keyframes&... keyframes)
      -> KeyframeSequence {
    const std::vector<KeyframeSequence::Keyframe> specs{
        keyframes.keyframe()...};
    return KeyframeSequence{AnimatableVector{initial}, specs};
  }
};

TEST_F(KeyframeTest, LinearKeyframesInterpolateAlongTheirTimingCurve) {
  const KeyframeSequence straight = sequence(0.0, LinearKeyframe(10.0, 1.0));
  EXPECT_DOUBLE_EQ(straight.value(0.0)[0], 0.0);
  EXPECT_NEAR(straight.value(0.25)[0], 2.5, 1.0e-6);
  EXPECT_NEAR(straight.value(0.5)[0], 5.0, 1.0e-6);
  EXPECT_DOUBLE_EQ(straight.value(2.0)[0], 10.0);
  EXPECT_DOUBLE_EQ(straight.duration(), 1.0);
  const KeyframeSequence eased =
      sequence(0.0, LinearKeyframe(10.0, 1.0, UnitCurve::ease_in));
  EXPECT_NEAR(eased.value(0.5)[0], 10.0 * UnitCurve::ease_in.value(0.5),
              1.0e-6);
  EXPECT_LT(eased.value(0.25)[0], 2.5);
}

TEST_F(KeyframeTest, MoveKeyframesJumpWithoutTakingTime) {
  const KeyframeSequence jumped =
      sequence(0.0, MoveKeyframe(5.0), LinearKeyframe(10.0, 1.0));
  EXPECT_DOUBLE_EQ(jumped.duration(), 1.0);
  EXPECT_DOUBLE_EQ(jumped.value(0.0)[0], 5.0);
  EXPECT_NEAR(jumped.value(0.5)[0], 7.5, 1.0e-9);
  const KeyframeSequence landed =
      sequence(0.0, LinearKeyframe(10.0, 1.0), MoveKeyframe(-3.0));
  EXPECT_DOUBLE_EQ(landed.value(1.0)[0], -3.0);
}

TEST_F(KeyframeTest, SpringKeyframesFollowTheClosedFormSpring) {
  const KeyframeSequence sprung = sequence(
      0.0, SpringKeyframe(1.0, 0.5,
                          Spring{Spring::Response{.response = 0.5,
                                                  .damping_ratio = 1.0}}));
  const double expected =
      1.0 - (std::exp(-std::numbers::pi) * (1.0 + std::numbers::pi));
  EXPECT_NEAR(sprung.value(0.25)[0], expected, 1.0e-9);
  EXPECT_DOUBLE_EQ(sprung.duration(), 0.5);
  const KeyframeSequence settled =
      sequence(0.0, SpringKeyframe(4.0, Spring::smooth()));
  EXPECT_DOUBLE_EQ(settled.duration(), Spring::smooth().settling_duration());
  EXPECT_DOUBLE_EQ(settled.value(settled.duration() + 1.0)[0], 4.0);
}

TEST_F(KeyframeTest, SpringKeyframesHandTheirVelocityToTheNextKeyframe) {
  const Spring spring{Spring::Response{.response = 0.5, .damping_ratio = 1.0}};
  const KeyframeSequence chained = sequence(
      0.0, SpringKeyframe(1.0, 0.25, spring), SpringKeyframe(1.0, 0.5, spring));
  const Spring::State handed = spring.state(-1.0, 0.0, 0.25);
  const Spring::State later =
      spring.state(handed.displacement, handed.velocity, 0.1);
  EXPECT_NEAR(chained.value(0.35)[0], 1.0 + later.displacement, 1.0e-9);
}

TEST_F(KeyframeTest, AdjacentCubicKeyframesFormACatmullRomSpline) {
  const KeyframeSequence spline =
      sequence(0.0, CubicKeyframe(10.0, 1.0), CubicKeyframe(30.0, 1.0));
  EXPECT_NEAR(spline.value(0.5)[0], 3.125, 1.0e-9);
  EXPECT_NEAR(spline.value(1.0)[0], 10.0, 1.0e-9);
  EXPECT_NEAR(spline.value(1.5)[0], 21.875, 1.0e-9);
  EXPECT_NEAR(spline.sample(0.999999).velocity[0], 15.0, 1.0e-3);
  EXPECT_NEAR(spline.sample(1.000001).velocity[0], 15.0, 1.0e-3);
  const KeyframeSequence pinned =
      sequence(0.0, CubicKeyframe(10.0, 1.0, 0.0, 20.0));
  EXPECT_NEAR(pinned.value(0.5)[0], 5.0 - 2.5, 1.0e-9);
}

TEST_F(KeyframeTest, TimelineDrivesEachTrackOnItsOwnClock) {
  const Values initial{};
  const KeyframeTimeline<Values> timeline{
      initial,
      Keyframes{KeyframeTrack(&Values::scale, LinearKeyframe(2.0, 0.5),
                              LinearKeyframe(1.0, 0.5)),
                KeyframeTrack(&Values::lift, LinearKeyframe(30.0, 2.0)),
                KeyframeTrack(&Values::spin,
                              LinearKeyframe(Angle::degrees(90.0), 1.0))}};
  EXPECT_DOUBLE_EQ(timeline.duration(), 2.0);
  const Values quarter = timeline.value(0.25);
  EXPECT_NEAR(quarter.scale, 1.5, 1.0e-6);
  EXPECT_NEAR(quarter.lift, 3.75, 1.0e-6);
  EXPECT_NEAR(quarter.spin.degrees(), 22.5, 1.0e-5);
  const Values late = timeline.value(1.5);
  EXPECT_DOUBLE_EQ(late.scale, 1.0);
  EXPECT_NEAR(late.lift, 22.5, 1.0e-6);
  EXPECT_NEAR(late.spin.degrees(), 90.0, 1.0e-9);
}

TEST_F(KeyframeTest, PortedEasingsMatchTheirReferenceFormulas) {
  EXPECT_NEAR(UnitCurve::circular_ease_in.value(0.5), 1.0 - std::sqrt(0.75),
              1.0e-12);
  EXPECT_NEAR(UnitCurve::circular_ease_out.value(0.5), std::sqrt(0.75),
              1.0e-12);
  EXPECT_NEAR(UnitCurve::back_ease_in.value(0.5), -0.0876975, 1.0e-7);
  EXPECT_NEAR(UnitCurve::back_ease_out.value(0.5), 1.0876975, 1.0e-7);
  EXPECT_NEAR(UnitCurve::elastic_ease_out.value(0.5), 1.015625, 1.0e-9);
  EXPECT_NEAR(UnitCurve::elastic_ease_in.value(0.5), -0.015625, 1.0e-9);
  EXPECT_NEAR(UnitCurve::bounce_ease_out.value(0.5), 0.765625, 1.0e-12);
  EXPECT_NEAR(UnitCurve::bounce_ease_in.value(0.5), 1.0 - 0.765625, 1.0e-12);
  EXPECT_NEAR(UnitCurve::bounce_ease_in_out.value(0.25), 0.5 * (1.0 - 0.765625),
              1.0e-12);
  for (const UnitCurve& curve :
       {UnitCurve::circular_ease_in_out, UnitCurve::back_ease_in_out,
        UnitCurve::elastic_ease_in_out, UnitCurve::bounce_ease_in_out}) {
    EXPECT_DOUBLE_EQ(curve.value(0.0), 0.0);
    EXPECT_DOUBLE_EQ(curve.value(1.0), 1.0);
    EXPECT_NEAR(curve.value(0.5), 0.5, 1.0e-12);
  }
  EXPECT_NEAR(UnitCurve::circular_ease_in.velocity(0.5), 0.5 / std::sqrt(0.75),
              1.0e-6);
  EXPECT_NE(UnitCurve::back_ease_in, UnitCurve::back_ease_out);
}

TEST_F(KeyframeTest, CatmullRomPathsPassThroughTheirPoints) {
  const MotionPath path = MotionPath::catmull_rom({{.x = 0.0F, .y = 0.0F},
                                                   {.x = 100.0F, .y = 0.0F},
                                                   {.x = 100.0F, .y = 100.0F}});
  EXPECT_EQ(path.point(0.0), (MotionPath::Point{.x = 0.0F, .y = 0.0F}));
  EXPECT_EQ(path.point(0.5), (MotionPath::Point{.x = 100.0F, .y = 0.0F}));
  EXPECT_EQ(path.point(1.0), (MotionPath::Point{.x = 100.0F, .y = 100.0F}));
  const MotionPath::Point quarter = path.point(0.25);
  EXPECT_NEAR(quarter.x, 50.0F, 1.0e-4F);
  EXPECT_NEAR(quarter.y, -6.25F, 1.0e-4F);
  const MotionPath tight = MotionPath::catmull_rom({{.x = 0.0F, .y = 0.0F},
                                                    {.x = 100.0F, .y = 0.0F},
                                                    {.x = 100.0F, .y = 100.0F}},
                                                   1.0);
  EXPECT_NEAR(tight.point(0.25).y, 0.0F, 1.0e-4F);
}

}
