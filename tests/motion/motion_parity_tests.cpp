#include <arisa/motion/animatable_pair.hpp>
#include <arisa/motion/animatable_traits.hpp>
#include <arisa/motion/animatable_values.hpp>
#include <arisa/motion/animatable_vector.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/animation_context.hpp>
#include <arisa/motion/animation_state_key.hpp>
#include <arisa/motion/animation_track.hpp>
#include <arisa/motion/base_animation.hpp>
#include <arisa/motion/custom_animation.hpp>
#include <arisa/motion/presentation.hpp>
#include <arisa/motion/spring.hpp>
#include <arisa/motion/transaction.hpp>
#include <arisa/motion/transition.hpp>
#include <arisa/motion/transition_content.hpp>
#include <arisa/motion/transition_phase.hpp>
#include <arisa/motion/transition_properties.hpp>
#include <arisa/motion/unit_curve.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/space.hpp>

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <format>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

namespace arisa::motion {

using namespace std::chrono_literals;

class MotionParityTest : public ::testing::Test {
 protected:
  static constexpr double exact = 1.0e-5;

  static auto reference() -> const std::map<std::string, double>& {
    static const std::map<std::string, double> rows = [] {
      std::map<std::string, double> found;
      std::ifstream stream{ARISA_SWIFTUI_MOTION_PARITY};
      std::string line;
      while (std::getline(stream, line)) {
        if (line.empty() || line.front() == '#' || line.starts_with("key")) {
          continue;
        }
        std::istringstream fields{line};
        std::string key;
        std::string light;
        std::getline(fields, key, '\t');
        std::getline(fields, light, '\t');
        found[key] = std::stod(light);
      }
      return found;
    }();
    return rows;
  }

  static auto value(std::string_view key) -> double {
    const auto found = reference().find(std::string{key});
    EXPECT_NE(found, reference().end()) << key;
    return found == reference().end() ? 0.0 : found->second;
  }

  template <class Sampled>
  static auto series_error(std::string_view series, Sampled sampled) -> double {
    const std::string prefix = std::format("motion_parity.{}.sample.", series);
    double worst = 0.0;
    std::size_t matched = 0;
    for (const auto& [key, expected] : reference()) {
      if (!key.starts_with(prefix)) {
        continue;
      }
      const double time = std::stod(key.substr(prefix.size())) / 1000.0;
      worst = std::max(worst, std::abs(sampled(time) - expected));
      ++matched;
    }
    EXPECT_GT(matched, 50U) << series;
    return worst;
  }

  static auto progress(const Animation& animation, double time) -> double {
    const Animation::Sample sample =
        animation.sample(Animation::Span{.from = AnimatableVector{0.0},
                                         .to = AnimatableVector{1.0},
                                         .velocity = {},
                                         .precision = 1.0e-3},
                         time);
    return sample.finished ? 1.0 : sample.value[0];
  }

  static auto animation_error(const Animation& animation, std::string_view name)
      -> double {
    return series_error(
        std::format("{}.animation", name),
        [&animation](double time) { return progress(animation, time); });
  }

  static void expect_spring(const Spring& spring, std::string_view name) {
    const std::string key = std::format("motion_parity.spring.{}", name);
    EXPECT_NEAR(spring.mass(), value(key + ".mass"), exact) << name;
    EXPECT_NEAR(spring.stiffness(), value(key + ".stiffness"),
                exact * spring.stiffness())
        << name;
    EXPECT_NEAR(spring.damping(), value(key + ".damping"), exact * 10.0)
        << name;
    EXPECT_NEAR(spring.duration(), value(key + ".duration"), exact) << name;
    EXPECT_NEAR(spring.response(), value(key + ".response"), exact) << name;
    EXPECT_NEAR(spring.damping_ratio(), value(key + ".damping_ratio"), exact)
        << name;
    EXPECT_NEAR(spring.bounce(), value(key + ".bounce"), exact) << name;
    EXPECT_NEAR(spring.settling_duration(), value(key + ".settling_duration"),
                exact)
        << name;
  }

  struct Develop {
    static constexpr bool properties = false;

    auto body(TransitionContent content, TransitionPhase phase) const
        -> TransitionContent {
      if (phase.is_identity()) {
        return content;
      }
      return content.grayscale(1.0F)
          .brightness(0.2F)
          .contrast(0.5F)
          .saturation(0.0F)
          .hue_rotation(Angle::degrees(90.0))
          .color_multiply(theme::Colour{1.0F, 0.5F, 0.5F, 1.0F})
          .rotation_3d_effect(Angle::degrees(60.0), 0.0F, 1.0F, 0.0F);
    }
  };

  class Counted final : public CustomAnimation {
   public:
    static const AnimationStateKey<int> calls;

    auto animate(const AnimatableVector& value, double time,
                 AnimationContext& context) const
        -> std::optional<AnimatableVector> override {
      context.set_state(calls, context.state(calls) + 1);
      if (time >= 1.0) {
        return std::nullopt;
      }
      return value * time;
    }
  };
};

const AnimationStateKey<int> MotionParityTest::Counted::calls{"calls", 0};

TEST_F(MotionParityTest, SpringAccessorsAndConstructorsMatchSwiftUI) {
  expect_spring(Spring::smooth(), "smooth");
  expect_spring(Spring::bouncy(), "bouncy");
  expect_spring(Spring{Spring::Response{.response = 0.3, .damping_ratio = 0.6}},
                "response_0_3_damping_0_6");
  expect_spring(Spring{Spring::Physical{
                    .mass = 2.0, .stiffness = 100.0, .damping = 10.0}},
                "mass_2_stiffness_100_damping_10");
  expect_spring(Spring{Spring::Physical{
                    .mass = 1.0, .stiffness = 100.0, .damping = 40.0}},
                "mass_1_stiffness_100_damping_40");
  expect_spring(Spring{Spring::Physical{.mass = 1.0,
                                        .stiffness = 100.0,
                                        .damping = 40.0,
                                        .allow_over_damping = true}},
                "mass_1_stiffness_100_damping_40_over");
  expect_spring(Spring{Spring::Settling{.duration = 1.0, .damping_ratio = 0.5}},
                "settling_1_damping_0_5");
  expect_spring(Spring{Spring::Settling{.duration = 0.5, .damping_ratio = 1.0}},
                "settling_0_5_damping_1");
  expect_spring(Spring{Spring::Settling{
                    .duration = 1.0, .damping_ratio = 0.8, .epsilon = 0.01}},
                "settling_1_damping_0_8_epsilon_0_01");
  expect_spring(Spring{Spring::Settling{.duration = 2.0, .damping_ratio = 0.3}},
                "settling_2_damping_0_3");
}

TEST_F(MotionParityTest, SpringFunctionsMatchSwiftUI) {
  const std::array springs{
      std::pair{std::string_view{"smooth"}, Spring::smooth()},
      std::pair{std::string_view{"bouncy"}, Spring::bouncy()},
      std::pair{
          std::string_view{"response_0_3_damping_0_6"},
          Spring{Spring::Response{.response = 0.3, .damping_ratio = 0.6}}},
      std::pair{std::string_view{"over"},
                Spring{Spring::Physical{.mass = 1.0,
                                        .stiffness = 100.0,
                                        .damping = 40.0,
                                        .allow_over_damping = true}}}};
  for (const auto& [name, spring] : springs) {
    const bool overdamped = spring.damping_ratio() > 1.0;
    EXPECT_LT(series_error(std::format("{}.value_target_2_velocity_1", name),
                           [&spring](double time) {
                             return spring.value(2.0, 1.0, time);
                           }),
              exact)
        << name;
    if (overdamped) {
      constexpr double nudge = 1.0e-6;
      for (const double time : {0.05, 0.2, 0.6}) {
        EXPECT_NEAR(spring.velocity(2.0, 1.0, time),
                    (spring.value(2.0, 1.0, time + nudge) -
                     spring.value(2.0, 1.0, time - nudge)) /
                        (2.0 * nudge),
                    1.0e-3);
      }
    } else {
      EXPECT_LT(
          series_error(std::format("{}.velocity_target_2_velocity_1", name),
                       [&spring](double time) {
                         return spring.velocity(2.0, 1.0, time);
                       }),
          exact * 100.0)
          << name;
      EXPECT_LT(
          series_error(std::format("{}.velocity_from_1_to_3_velocity_2", name),
                       [&spring](double time) {
                         return spring.velocity(1.0, 3.0, 2.0, time);
                       }),
          exact * 100.0)
          << name;
    }
    EXPECT_LT(series_error(std::format("{}.value_from_1_to_3_velocity_2", name),
                           [&spring](double time) {
                             return spring.value(1.0, 3.0, 2.0, time);
                           }),
              exact)
        << name;
    const std::string key = std::format("motion_parity.function.{}", name);
    EXPECT_NEAR(spring.settling_duration(1.0, 0.0, 0.001),
                value(key + ".settling_target_1"), exact)
        << name;
    EXPECT_NEAR(spring.settling_duration(10.0, 0.0, 0.001),
                value(key + ".settling_target_10"), exact)
        << name;
    EXPECT_NEAR(spring.settling_duration(1.0, 5.0, 0.01),
                value(key + ".settling_target_1_velocity_5_epsilon_0_01"),
                exact)
        << name;
    EXPECT_NEAR(spring.settling_duration(1.0, 3.0, 2.0, 0.001),
                value(key + ".settling_from_1_to_3_velocity_2_epsilon_0_001"),
                exact)
        << name;
    EXPECT_NEAR(spring.force(1.0, 0.3, 2.0),
                value(key + ".force_target_1_position_0_3_velocity_2"),
                exact * 100.0)
        << name;
    EXPECT_NEAR(spring.force(1.0, 3.0, 2.0, 1.0),
                value(key + ".force_from_1_to_3_position_2_velocity_1"),
                exact * 100.0)
        << name;
    for (const double step : {1.0 / 60.0, 0.1, 0.5}) {
      if (overdamped) {
        break;
      }
      double position = 0.0;
      double speed = 0.0;
      for (int index = 1; index <= 12 && index * step < 1.0 + 1.0e-9; ++index) {
        const Spring::Motion<double> next =
            spring.update(position, speed, 1.0, step);
        position = next.value;
        speed = next.velocity;
        const std::string tag =
            std::format("{}.update_{:04}.{:02}", key,
                        static_cast<int>(std::lround(step * 1000.0)), index);
        EXPECT_NEAR(position, value(tag + ".value"), exact) << tag;
        EXPECT_NEAR(speed, value(tag + ".velocity"), exact * 100.0) << tag;
      }
    }
  }
}

TEST_F(MotionParityTest, InterpolatingSpringsMatchSwiftUI) {
  constexpr double settled = 1.5e-3;
  constexpr double blended = 0.025;
  EXPECT_LT(animation_error(Animation::interpolating_spring(),
                            "interpolating_default"),
            settled);
  EXPECT_LT(animation_error(Animation::interpolating_spring(Spring::Physical{
                                .stiffness = 100.0, .damping = 10.0}),
                            "interpolating_stiffness_100_damping_10"),
            settled);
  EXPECT_LT(animation_error(
                Animation::interpolating_spring(
                    Spring::Physical{.stiffness = 170.0, .damping = 15.0}, 5.0),
                "interpolating_stiffness_170_damping_15_velocity_5"),
            settled);
  EXPECT_LT(
      animation_error(Animation::interpolating_spring(Spring::Physical{
                          .mass = 2.0, .stiffness = 100.0, .damping = 20.0}),
                      "interpolating_mass_2_stiffness_100_damping_20"),
      settled);
  EXPECT_LT(animation_error(
                Animation::interpolating_spring(
                    Spring::Duration{.duration = 0.5, .bounce = 0.3}, 2.0),
                "interpolating_duration_0_5_bounce_0_3_velocity_2"),
            settled);
  EXPECT_LT(
      animation_error(Animation::interpolating_spring(Spring::bouncy(), 3.0),
                      "interpolating_bouncy_velocity_3"),
      settled);
  EXPECT_LT(
      animation_error(Animation::interpolating_spring(Spring::smooth(), -2.0),
                      "interpolating_smooth_velocity_minus_2"),
      settled);
  EXPECT_LT(
      animation_error(Animation::interactive_spring(Animation::Interactive{
                          .duration = 0.3, .extra_bounce = 0.1}),
                      "interactive_duration_0_3_extra_0_1"),
      blended);
  EXPECT_LT(animation_error(Animation::interactive_spring(
                                Animation::Interactive{.duration = 0.15}),
                            "interactive_duration_default"),
            blended);
  EXPECT_TRUE(Animation::interpolating_spring().additive());
  EXPECT_FALSE(Animation::spring().additive());
  EXPECT_DOUBLE_EQ(
      Animation::interpolating_spring(Spring::smooth(), 2.5).initial_velocity(),
      2.5);
}

TEST_F(MotionParityTest, EveryUnitCurveDrivesATimingCurve) {
  const UnitCurve bezier = UnitCurve::bezier(0.2, 0.9, 0.3, 1.4);
  const std::array curves{
      std::pair{std::string_view{"timing_circular_ease_in_0_5"},
                Animation::timing_curve(UnitCurve::circular_ease_in, 0.5)},
      std::pair{std::string_view{"timing_circular_ease_out_0_5"},
                Animation::timing_curve(UnitCurve::circular_ease_out, 0.5)},
      std::pair{std::string_view{"timing_circular_ease_in_out_0_5"},
                Animation::timing_curve(UnitCurve::circular_ease_in_out, 0.5)},
      std::pair{std::string_view{"timing_ease_in_0_5"},
                Animation::timing_curve(UnitCurve::ease_in, 0.5)},
      std::pair{std::string_view{"timing_ease_out_0_5"},
                Animation::timing_curve(UnitCurve::ease_out, 0.5)},
      std::pair{std::string_view{"timing_ease_in_out_0_5"},
                Animation::timing_curve(UnitCurve::ease_in_out, 0.5)},
      std::pair{std::string_view{"timing_linear_1"},
                Animation::timing_curve(UnitCurve::linear, 1.0)},
      std::pair{std::string_view{"timing_bezier_0_5"},
                Animation::timing_curve(bezier, 0.5)},
      std::pair{std::string_view{"timing_inverse_ease_in_0_5"},
                Animation::timing_curve(UnitCurve::ease_in.inverse(), 0.5)}};
  for (const auto& [name, animation] : curves) {
    EXPECT_LT(animation_error(animation, name), 1.0e-4) << name;
  }
  for (const UnitCurve curve :
       {UnitCurve::back_ease_in, UnitCurve::elastic_ease_out,
        UnitCurve::bounce_ease_in_out}) {
    const Animation timed = Animation::timing_curve(curve, 0.5);
    EXPECT_NEAR(progress(timed, 0.25), curve.value(0.5), exact);
    EXPECT_DOUBLE_EQ(progress(timed, 0.5), 1.0);
  }
}

TEST_F(MotionParityTest, UnitCurveInverseMatchesSwiftUI) {
  const std::array curves{
      std::pair{std::string_view{"linear"}, UnitCurve::linear},
      std::pair{std::string_view{"ease_in"}, UnitCurve::ease_in},
      std::pair{std::string_view{"ease_out"}, UnitCurve::ease_out},
      std::pair{std::string_view{"ease_in_out"}, UnitCurve::ease_in_out},
      std::pair{std::string_view{"circular_ease_in"},
                UnitCurve::circular_ease_in},
      std::pair{std::string_view{"circular_ease_out"},
                UnitCurve::circular_ease_out},
      std::pair{std::string_view{"circular_ease_in_out"},
                UnitCurve::circular_ease_in_out},
      std::pair{std::string_view{"bezier"},
                UnitCurve::bezier(0.2, 0.9, 0.3, 1.4)}};
  for (const auto& [name, curve] : curves) {
    for (int step = 0; step <= 20; ++step) {
      const double at = static_cast<double>(step) / 20.0;
      const std::string tag =
          std::format("motion_parity.curve.{}.{:03}", name, step * 5);
      EXPECT_NEAR(curve.value(at), value(tag + ".value"), 1.0e-4) << tag;
      EXPECT_NEAR(curve.inverse().value(at), value(tag + ".inverse_value"),
                  1.0e-4)
          << tag;
      EXPECT_NEAR(curve.inverse().inverse().value(at),
                  value(tag + ".inverse_inverse_value"), 1.0e-4)
          << tag;
    }
    EXPECT_EQ(curve.inverse().inverse() == curve,
              value(std::format("motion_parity.curve.{}.inverse_inverse_equal",
                                name)) == 1.0)
        << name;
  }
  EXPECT_EQ(UnitCurve::circular_ease_in.inverse(),
            UnitCurve::circular_ease_out);
  EXPECT_EQ(UnitCurve::ease_in.inverse(),
            UnitCurve::bezier(0.0, 0.42, 1.0, 1.0));
}

TEST_F(MotionParityTest, ShouldMergeMatchesSwiftUI) {
  AnimationContext context;
  const AnimatableVector moved{1.0};
  const auto merges = [&](const Animation& next, const Animation& previous) {
    return next.should_merge(previous, moved, 0.1, context) ? 1.0 : 0.0;
  };
  EXPECT_EQ(merges(Animation::spring(), Animation::spring()),
            value("motion_parity.merge.spring_after_spring"));
  EXPECT_EQ(merges(Animation::spring(), Animation::ease_in()),
            value("motion_parity.merge.spring_after_ease_in"));
  EXPECT_EQ(merges(Animation::smooth(), Animation::bouncy()),
            value("motion_parity.merge.smooth_after_bouncy"));
  EXPECT_EQ(merges(Animation::ease_in(), Animation::ease_in()),
            value("motion_parity.merge.ease_in_after_ease_in"));
  EXPECT_EQ(merges(Animation::ease_in(), Animation::spring()),
            value("motion_parity.merge.ease_in_after_spring"));
  EXPECT_EQ(merges(Animation::linear(), Animation::linear()),
            value("motion_parity.merge.linear_after_linear"));
  EXPECT_EQ(merges(Animation::interpolating_spring(),
                   Animation::interpolating_spring()),
            value("motion_parity.merge.interpolating_after_interpolating"));
  EXPECT_EQ(merges(Animation::interpolating_spring(), Animation::spring()),
            value("motion_parity.merge.interpolating_after_spring"));
  EXPECT_EQ(merges(Animation::spring(), Animation::interpolating_spring()),
            value("motion_parity.merge.spring_after_interpolating"));
  EXPECT_EQ(
      merges(Animation::interactive_spring(), Animation::interactive_spring()),
      value("motion_parity.merge.interactive_after_interactive"));
  EXPECT_EQ(merges(Animation::spring().delay(0.1), Animation::spring()),
            value("motion_parity.merge.spring_delay_after_spring"));
  EXPECT_EQ(merges(Animation::spring().speed(2.0), Animation::spring()),
            value("motion_parity.merge.spring_speed_after_spring"));
  EXPECT_EQ(merges(Animation::spring().repeat_count(2), Animation::spring()),
            value("motion_parity.merge.spring_repeat_after_spring"));
  EXPECT_EQ(
      merges(Animation::spring().logically_complete(0.1), Animation::spring()),
      value("motion_parity.merge.spring_logical_after_spring"));
}

TEST_F(MotionParityTest, LogicallyCompleteMatchesSwiftUI) {
  const std::array cases{
      std::pair{std::string_view{"spring_after_0_1"},
                Animation::spring().logically_complete(0.1)},
      std::pair{std::string_view{"linear_1_after_0_25"},
                Animation::linear(1.0).logically_complete(0.25)},
      std::pair{std::string_view{"smooth_after_0_3_delay_0_2"},
                Animation::smooth().logically_complete(0.3).delay(0.2)},
      std::pair{std::string_view{"smooth_delay_0_2_after_0_3"},
                Animation::smooth().delay(0.2).logically_complete(0.3)},
      std::pair{std::string_view{"linear_1_after_0_4_speed_2"},
                Animation::linear(1.0).logically_complete(0.4).speed(2.0)},
      std::pair{std::string_view{"interpolating_default"},
                Animation::interpolating_spring()}};
  for (const auto& [name, animation] : cases) {
    EXPECT_NEAR(animation.logical_duration(),
                value(std::format("motion_parity.logical.{}", name)), 1.0e-3)
        << name;
  }
  const Animation late = Animation::linear(0.2).logically_complete(0.5);
  EXPECT_LT(value("motion_parity.logical.linear_0_2_after_0_5"), 0.0);
  EXPECT_GT(late.logical_duration(), late.cycle_duration());
}

TEST_F(MotionParityTest, VectorArithmeticMatchesSwiftUI) {
  using Pair = AnimatablePair<double, AnimatablePair<double, double>>;
  static_assert(VectorArithmetic<AnimatableVector>);
  static_assert(VectorArithmetic<Pair>);
  static_assert(VectorArithmetic<AnimatableValues<double, double, double>>);
  const Pair pair{.first = 3.0, .second = {.first = 4.0, .second = 12.0}};
  EXPECT_DOUBLE_EQ(pair.magnitude_squared(),
                   value("motion_parity.vector.pair.magnitude_squared"));
  Pair scaled = pair;
  scaled.scale(-0.5);
  EXPECT_DOUBLE_EQ(scaled.first,
                   value("motion_parity.vector.pair.scaled.first"));
  EXPECT_DOUBLE_EQ(scaled.second.second,
                   value("motion_parity.vector.pair.scaled.second.second"));
  EXPECT_EQ(Pair::zero(), Pair{});
  EXPECT_EQ(pair - pair, Pair::zero());
  EXPECT_DOUBLE_EQ((pair + pair).first, 6.0);
  const AnimatableValues<double, double, double> values{1.0, 2.0, 2.0};
  EXPECT_DOUBLE_EQ(values.magnitude_squared(),
                   value("motion_parity.vector.values.magnitude_squared"));
  EXPECT_DOUBLE_EQ(std::get<2>(values.scaled(2.0).value), 4.0);
  EXPECT_DOUBLE_EQ(std::get<1>((values + values).value), 4.0);
  using Triple = AnimatableValues<double, double, double>;
  EXPECT_EQ(
      AnimatableTraits<Triple>::value(AnimatableTraits<Triple>::vector(values)),
      values);
  AnimatableVector towards{1.0};
  towards.interpolate(AnimatableVector{3.0}, 0.25);
  EXPECT_DOUBLE_EQ(towards[0],
                   value("motion_parity.vector.double.interpolate_1_3_0_25"));
  EXPECT_DOUBLE_EQ(
      AnimatableVector{1.0}.interpolated(AnimatableVector{3.0}, 1.5)[0],
      value("motion_parity.vector.double.interpolated_1_3_1_5"));
  const AnimatableVector sides{3.0, 4.0};
  EXPECT_DOUBLE_EQ(sides.magnitude(), 5.0);
  EXPECT_DOUBLE_EQ(sides.magnitude_squared(), 25.0);
  AnimatableVector flipped = sides;
  flipped.negate();
  EXPECT_EQ(flipped, -sides);
  EXPECT_DOUBLE_EQ(flipped[1], -4.0);
}

TEST_F(MotionParityTest, BaseAnimatesLikeItsAnimationAndKeepsCustomCurves) {
  const Animation eased = Animation::ease_in(0.5);
  const std::shared_ptr<const CustomAnimation> base = eased.base();
  ASSERT_NE(base, nullptr);
  AnimationContext context;
  const std::optional<AnimatableVector> halfway =
      base->animate(AnimatableVector{2.0}, 0.25, context);
  ASSERT_TRUE(halfway.has_value());
  EXPECT_NEAR((*halfway)[0], 2.0 * progress(eased, 0.25), exact);
  EXPECT_FALSE(base->animate(AnimatableVector{2.0}, 0.6, context).has_value());
  EXPECT_TRUE(base->should_merge(Animation::spring(), AnimatableVector{1.0},
                                 0.0, context) == false);
  const auto custom = std::make_shared<const Counted>();
  EXPECT_EQ(Animation::custom(custom).base(), custom);
}

TEST_F(MotionParityTest, TypedStateKeysKeepPerTrackCustomState) {
  AnimationTrack track{Animation::custom(Counted{}),
                       Animation::Span{.from = AnimatableVector{0.0},
                                       .to = AnimatableVector{1.0},
                                       .velocity = {},
                                       .precision = 1.0e-3},
                       0ns, nullptr};
  static_cast<void>(track.step(100ms));
  static_cast<void>(track.step(200ms));
  AnimationContext context;
  EXPECT_EQ(context.state(Counted::calls), 0);
  context.set_state(Counted::calls, 5);
  AnimationContext copy = context;
  copy.set_state(Counted::calls, 9);
  EXPECT_EQ(context.state(Counted::calls), 5);
  EXPECT_EQ(copy.state(Counted::calls), 9);
}

TEST_F(MotionParityTest, TransitionsCarryAnimationPropertiesAndEdges) {
  EXPECT_FALSE(Transition::opacity().properties().has_motion());
  EXPECT_FALSE(Transition::identity().properties().has_motion());
  EXPECT_TRUE(Transition::move(theme::Edge::leading).properties().has_motion());
  EXPECT_TRUE(Transition::scale().properties().has_motion());
  EXPECT_TRUE(Transition::offset(10.0F).properties().has_motion());
  EXPECT_TRUE(Transition::blur_replace().properties().has_motion());
  EXPECT_TRUE(Transition::push(theme::Edge::top).properties().has_motion());
  EXPECT_TRUE(Transition::slide().properties().has_motion());
  EXPECT_TRUE(Transition::asymmetric(Transition::opacity(),
                                     Transition::move(theme::Edge::top))
                  .properties()
                  .has_motion());
  EXPECT_FALSE(
      Transition::asymmetric(Transition::opacity(), Transition::opacity())
          .properties()
          .has_motion());
  EXPECT_EQ(Transition::move(theme::Edge::bottom).edge(), theme::Edge::bottom);
  EXPECT_EQ(Transition::push(theme::Edge::leading).edge(),
            theme::Edge::leading);
  EXPECT_FALSE(Transition::opacity().edge().has_value());
  const Transition timed =
      Transition::opacity().animation(Animation::ease_in());
  ASSERT_TRUE(timed.attached_animation().has_value());
  EXPECT_EQ(*timed.attached_animation(), Animation::ease_in());
  EXPECT_NE(timed, Transition::opacity());
  Transaction plain;
  EXPECT_EQ(Presentation::carried(timed, plain), Animation::ease_in());
  plain.set_disables_animations(true);
  EXPECT_FALSE(Presentation::carried(timed, plain).has_value());
  const Transition still =
      Transition::scale(0.5F).with_properties(TransitionProperties{false});
  EXPECT_FALSE(still.properties().has_motion());
  EXPECT_EQ(still.reduced(), still);
  EXPECT_EQ(Transition::scale(0.5F).reduced().insertion().opacity, 0.0F);
  const Transition symbol = Transition::symbol_effect();
  EXPECT_FLOAT_EQ(symbol.insertion().scale_x, Transition::symbol_effect_scale);
  EXPECT_TRUE(symbol.attached_animation().has_value());
}

TEST_F(MotionParityTest, CustomTransitionsExpressColourAndDepthModifiers) {
  const Transition developed = Transition::custom(Develop{});
  EXPECT_FALSE(developed.properties().has_motion());
  const Transition::Effect& hidden = developed.insertion();
  EXPECT_TRUE(hidden.recolours());
  EXPECT_TRUE(hidden.moves());
  EXPECT_FLOAT_EQ(hidden.grayscale, 1.0F);
  EXPECT_FLOAT_EQ(hidden.brightness, 0.2F);
  EXPECT_FLOAT_EQ(hidden.colour_multiply.green, 0.5F);
  const Transition::Effect half = hidden.at(0.5);
  EXPECT_FLOAT_EQ(half.grayscale, 0.5F);
  EXPECT_FLOAT_EQ(half.contrast, 0.75F);
  EXPECT_FLOAT_EQ(half.colour_multiply.green, 0.75F);
  EXPECT_NEAR(half.tilt, Angle::degrees(30.0).radians(), 1.0e-6);
  EXPECT_FALSE(hidden.at(1.0).recolours());
  const Transition modified = Transition::modifier(
      TransitionContent{}.opacity(0.0F).blur(8.0F), TransitionContent{});
  EXPECT_FLOAT_EQ(modified.insertion().opacity, 0.0F);
  EXPECT_FLOAT_EQ(modified.removal().blur, 8.0F);
  const Transition relative =
      Transition::modifier(TransitionContent{}.scale_effect(0.5F).offset(10.0F),
                           TransitionContent{}.scale_effect(0.8F).offset(4.0F));
  EXPECT_FLOAT_EQ(relative.insertion().scale_x, 0.625F);
  EXPECT_FLOAT_EQ(relative.insertion().offset_x, 6.0F);
}

TEST_F(MotionParityTest, TrackedVelocitySmoothsAndContinuousIsCarried) {
  Transaction tracked;
  tracked.set_disables_animations(true);
  tracked.set_tracks_velocity(true);
  tracked.set_is_continuous(true);
  EXPECT_TRUE(tracked.is_continuous());
  EXPECT_TRUE(tracked.tracks_velocity());
  EXPECT_FALSE(Transaction{}.is_continuous());
  EXPECT_FALSE(Transaction{}.tracks_velocity());
  Transaction plain;
  plain.set_disables_animations(true);
  const auto moment = [](const Transaction& transaction,
                         std::chrono::nanoseconds now) {
    return Presentation::Moment{.transaction = transaction, .now = now};
  };
  Presentation smoothed;
  Presentation raw;
  const std::array<double, 4> path{0.0, 10.0, 12.0, 22.0};
  for (std::size_t index = 0; index < path.size(); ++index) {
    const auto now =
        std::chrono::nanoseconds{static_cast<std::int64_t>(index) * 16'666'667};
    smoothed.animate(Presentation::Property::offset,
                     AnimatableVector{path[index]}, moment(tracked, now));
    raw.animate(Presentation::Property::offset, AnimatableVector{path[index]},
                moment(plain, now));
  }
  const double jittery = raw.velocity(Presentation::Property::offset)[0];
  const double steady = smoothed.velocity(Presentation::Property::offset)[0];
  EXPECT_NEAR(jittery, 600.0, 1.0);
  EXPECT_GT(steady, 300.0);
  EXPECT_LT(steady, 600.0);
}

}
