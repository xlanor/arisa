#include <arisa/motion/animatable_pair.hpp>
#include <arisa/motion/animatable_vector.hpp>
#include <arisa/motion/animated_value.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/animation_completion.hpp>
#include <arisa/motion/animation_context.hpp>
#include <arisa/motion/animation_track.hpp>
#include <arisa/motion/completion_queue.hpp>
#include <arisa/motion/custom_animation.hpp>
#include <arisa/motion/presentation.hpp>
#include <arisa/motion/spring.hpp>
#include <arisa/motion/transaction.hpp>
#include <arisa/motion/transaction_key.hpp>
#include <arisa/motion/transition.hpp>
#include <arisa/motion/transition_content.hpp>

#include <gtest/gtest.h>

#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <vector>

namespace arisa::motion {

using namespace std::chrono_literals;

class CustomAnimationTest : public ::testing::Test {
 protected:
  class Stepped final : public CustomAnimation {
   public:
    auto animate(const AnimatableVector& value, double time,
                 AnimationContext& context) const
        -> std::optional<AnimatableVector> override {
      if (time >= 1.0) {
        return std::nullopt;
      }
      context.state() = context.state() + AnimatableVector{1.0};
      if (time >= 0.5) {
        context.set_logically_complete(true);
      }
      return value * (std::floor(time * 4.0) / 4.0);
    }

    auto should_merge(const Animation&, const AnimatableVector&, double,
                      AnimationContext&) const -> bool override {
      return true;
    }
  };

  static auto reference() -> const std::map<std::string, double>& {
    static const std::map<std::string, double> rows = [] {
      std::map<std::string, double> found;
      std::ifstream stream{ARISA_SWIFTUI_ANIMATORS};
      std::string line;
      while (std::getline(stream, line)) {
        if (line.empty() || line.front() == '#') {
          continue;
        }
        std::istringstream fields{line};
        std::string key;
        std::string light;
        std::getline(fields, key, '\t');
        std::getline(fields, light, '\t');
        if (key.starts_with("animators.")) {
          found[key] = std::stod(light);
        }
      }
      return found;
    }();
    return rows;
  }

  static auto measured(const std::string& key) -> double {
    const auto found = reference().find(key);
    EXPECT_NE(found, reference().end()) << key;
    return found == reference().end() ? 0.0 : found->second;
  }

  static auto at(double seconds) -> std::chrono::nanoseconds {
    return std::chrono::nanoseconds{std::llround(seconds * 1.0e9)};
  }
};

TEST_F(CustomAnimationTest, LogicalDurationMatchesSwiftUI) {
  const std::vector<std::pair<std::string, Animation>> cases{
      {"default", Animation::default_animation()},
      {"smooth", Animation::smooth()},
      {"bouncy", Animation::bouncy()},
      {"spring_duration_1_bounce_0_5",
       Animation::spring(
           Spring{Spring::Duration{.duration = 1.0, .bounce = 0.5}})},
      {"spring_response_0_3_damping_0_6", Animation::spring(0.3, 0.6)},
      {"ease_in_out", Animation::ease_in_out()},
      {"linear_0_5", Animation::linear(0.5)},
      {"ease_out_delay_0_2", Animation::ease_out(0.3).delay(0.2)},
      {"linear_0_2_speed_2", Animation::linear(0.2).speed(2.0)},
      {"linear_0_2_repeat_3", Animation::linear(0.2).repeat_count(3)},
  };
  for (const auto& [name, animation] : cases) {
    const double logical = measured("animators.logical." + name);
    const double removed = measured("animators.removed." + name);
    const double expected = logical >= 0.0 ? logical : removed;
    EXPECT_NEAR(animation.logical_duration(), expected, 0.002) << name;
  }
}

TEST_F(CustomAnimationTest, CustomCurveDrivesATrackAndCarriesItsContext) {
  AnimationTrack track{Animation::custom(Stepped{}),
                       Animation::Span{.from = AnimatableVector{10.0},
                                       .to = AnimatableVector{20.0},
                                       .velocity = {},
                                       .precision = 1.0e-3},
                       0ns, nullptr};
  EXPECT_DOUBLE_EQ(track.step(at(0.3)).value[0], 12.5);
  EXPECT_DOUBLE_EQ(track.step(at(0.6)).value[0], 15.0);
  EXPECT_FALSE(track.step(at(0.8)).finished);
  EXPECT_TRUE(track.step(at(1.0)).finished);
  EXPECT_DOUBLE_EQ(track.sample(at(0.3)).value[0], 12.5);
}

TEST_F(CustomAnimationTest, ShouldMergeKeepsTheRunningTrack) {
  AnimationTrack track{Animation::custom(Stepped{}),
                       Animation::Span{.from = AnimatableVector{0.0},
                                       .to = AnimatableVector{4.0},
                                       .velocity = {},
                                       .precision = 1.0e-3},
                       0ns, nullptr};
  EXPECT_TRUE(track.merge(Animation::custom(Stepped{}), AnimatableVector{8.0},
                          at(0.5)));
  EXPECT_DOUBLE_EQ(track.step(at(0.5)).value[0], 4.0);
  EXPECT_FALSE(
      track.merge(Animation::linear(1.0), AnimatableVector{8.0}, at(0.5)));
}

TEST_F(CustomAnimationTest, LogicalCompletionFiresBeforeTheSpringSettles) {
  const auto queue = std::make_shared<CompletionQueue>();
  int logical = 0;
  int removed = 0;
  Transaction transaction{Animation::spring(0.3, 0.6)};
  transaction.add_animation_completion(
      AnimationCompletionCriteria::logically_complete,
      [&logical] { ++logical; });
  transaction.add_animation_completion(AnimationCompletionCriteria::removed,
                                       [&removed] { ++removed; });
  transaction.attach(queue);
  Presentation presentation;
  const Transaction none;
  const Presentation::Moment start{.transaction = none, .now = 0ns};
  static_cast<void>(presentation.animate(Presentation::Property::opacity,
                                         AnimatableVector{0.0}, start));
  const Presentation::Moment animated{.transaction = transaction, .now = 0ns};
  static_cast<void>(presentation.animate(Presentation::Property::opacity,
                                         AnimatableVector{1.0}, animated));
  transaction = Transaction{};
  const auto step = [&](double seconds) {
    static_cast<void>(presentation.animate(
        Presentation::Property::opacity, AnimatableVector{1.0},
        Presentation::Moment{.transaction = none, .now = at(seconds)}));
    queue->drain();
  };
  step(0.25);
  EXPECT_EQ(logical, 0);
  step(0.31);
  EXPECT_EQ(logical, 1);
  EXPECT_EQ(removed, 0);
  step(2.0);
  EXPECT_EQ(removed, 1);
}

TEST_F(CustomAnimationTest, AnimatablePairPacksBothHalves) {
  using Pair = AnimatablePair<double, AnimatablePair<float, Angle>>;
  const Pair value{.first = 2.0,
                   .second = {.first = 3.0F, .second = Angle::degrees(90.0)}};
  const AnimatableVector packed = AnimatableTraits<Pair>::vector(value);
  EXPECT_DOUBLE_EQ(packed[0], 2.0);
  EXPECT_DOUBLE_EQ(packed[1], 3.0);
  EXPECT_NEAR(packed[2], 1.5707963, 1.0e-6);
  const Pair back = AnimatableTraits<Pair>::value(packed * 0.5);
  EXPECT_DOUBLE_EQ(back.first, 1.0);
  EXPECT_FLOAT_EQ(back.second.first, 1.5F);
  EXPECT_NEAR(back.second.second.degrees(), 45.0, 1.0e-9);
}

TEST_F(CustomAnimationTest, TransactionKeysReadDefaultsAndCopyOnWrite) {
  static constexpr TransactionKey<int> bounce{"bounce", 3};
  Transaction first;
  EXPECT_EQ(first[bounce], 3);
  first.set(bounce, 7);
  Transaction second = first;
  second.set(bounce, 9);
  EXPECT_EQ(first[bounce], 7);
  EXPECT_EQ(second[bounce], 9);
}

TEST_F(CustomAnimationTest, AnimatedValueRetargetsFromWhatItShows) {
  AnimatedValue value;
  const Transaction none;
  const Transaction linear{Animation::linear(1.0)};
  EXPECT_DOUBLE_EQ(value.animate(AnimatableVector{0.0},
                                 {.transaction = none, .now = 0ns})[0],
                   0.0);
  EXPECT_DOUBLE_EQ(value.animate(AnimatableVector{10.0},
                                 {.transaction = linear, .now = 0ns})[0],
                   0.0);
  EXPECT_NEAR(value.animate(AnimatableVector{10.0},
                            {.transaction = none, .now = at(0.5)})[0],
              5.0, 1.0e-6);
  EXPECT_TRUE(value.animating());
  EXPECT_DOUBLE_EQ(value.animate(AnimatableVector{10.0},
                                 {.transaction = none, .now = at(1.0)})[0],
                   10.0);
  EXPECT_FALSE(value.animating());
}

TEST_F(CustomAnimationTest, MixesInterpolateAndSnapWhenReshaped) {
  Presentation presentation;
  const Transaction none;
  const Transaction linear{Animation::linear(1.0)};
  const std::array<float, 3> first{0.0F, 0.0F, 0.0F};
  const std::array<float, 3> second{2.0F, 4.0F, 6.0F};
  static_cast<void>(presentation.mix(Presentation::Mix::colour, first,
                                     {.transaction = none, .now = 0ns}));
  static_cast<void>(presentation.mix(Presentation::Mix::colour, second,
                                     {.transaction = linear, .now = 0ns}));
  const std::span<const float> halfway = presentation.mix(
      Presentation::Mix::colour, second, {.transaction = none, .now = at(0.5)});
  EXPECT_NEAR(halfway[1], 2.0F, 1.0e-4F);
  const auto blend = presentation.blend(Presentation::Mix::colour);
  ASSERT_TRUE(blend.has_value());
  EXPECT_NEAR(blend->progress, 0.5F, 1.0e-4F);
  const std::span<const float> reshaped =
      presentation.mix(Presentation::Mix::colour, second,
                       {.transaction = linear, .now = at(0.5)}, 7);
  EXPECT_FLOAT_EQ(reshaped[2], 6.0F);
  EXPECT_FALSE(presentation.in_flight(Presentation::Mix::colour));
}

TEST_F(CustomAnimationTest,
       CustomTransitionsAndBlurReplaceDescribeTheirEffects) {
  struct Spin {
    auto body(TransitionContent content, TransitionPhase phase) const
        -> TransitionContent {
      return content.opacity(phase.is_identity() ? 1.0F : 0.0F)
          .rotation_effect(Angle::degrees(phase.value() * 90.0))
          .blur(phase.is_identity() ? 0.0F : 6.0F);
    }
  };
  const Transition spin = Transition::custom(Spin{});
  EXPECT_FLOAT_EQ(spin.insertion().opacity, 0.0F);
  EXPECT_NEAR(spin.insertion().rotation, -1.5707963F, 1.0e-6F);
  EXPECT_NEAR(spin.removal().rotation, 1.5707963F, 1.0e-6F);
  EXPECT_FLOAT_EQ(spin.removal().blur, 6.0F);
  EXPECT_FLOAT_EQ(spin.insertion().at(0.5).blur, 3.0F);
  const Transition replace = Transition::blur_replace();
  EXPECT_FLOAT_EQ(replace.insertion().opacity, 0.0F);
  EXPECT_NEAR(replace.insertion().scale_x,
              static_cast<float>(1.0 - ((100.0 -
                                         measured("animators.blur_replace."
                                                  "down_up.insert.50."
                                                  "half_width") -
                                         1.0) *
                                        2.0 / 100.0)),
              1.0e-5F);
  EXPECT_FLOAT_EQ(Transition::blur_replace(Transition::BlurReplace::up_up)
                      .removal()
                      .scale_x,
                  Transition::blur_replace_grow);
  EXPECT_NEAR(replace.insertion().at(0.5).opacity,
              measured("animators.blur_replace.down_up.insert.50.centre_alpha"),
              0.01);
}

}
