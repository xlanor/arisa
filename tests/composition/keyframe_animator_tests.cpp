#include <arisa/components/vstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/keyframe_animator.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/composition/tree.hpp>
#include <arisa/motion/keyframe_track.hpp>
#include <arisa/motion/keyframes.hpp>
#include <arisa/motion/linear_keyframe.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <functional>
#include <string_view>
#include <utility>

namespace arisa::composition {

using namespace std::chrono_literals;

class KeyframeAnimatorTest : public ::testing::Test {
 protected:
  struct Hop {
    double lift{0.0};
  };

  static auto hops(const Hop&) -> motion::Keyframes<Hop> {
    return motion::Keyframes{
        motion::KeyframeTrack(&Hop::lift, motion::LinearKeyframe(10.0, 1.0),
                              motion::LinearKeyframe(0.0, 1.0))};
  }

  class Launcher final : public Component {
   public:
    Launcher(const Observable<int>& trigger, const Observable<int>& noise,
             bool modifier)
        : trigger_(trigger), noise_(noise), modifier_(modifier) {}

    auto operator==(const Launcher&) const -> bool { return false; }

    auto body(Context& context) const -> Element override {
      const int fired = context.read(trigger_.get());
      static_cast<void>(context.read(noise_.get()));
      if (modifier_) {
        return components::VStack()
            .frame({.width = 20.0F, .height = 20.0F})
            .id("ball")
            .keyframe_animator(
                Hop{}, fired,
                [](Element ball, const Hop& hop) {
                  return std::move(ball).offset(0.0F,
                                                static_cast<float>(hop.lift));
                },
                hops);
      }
      return KeyframeAnimator(
          Hop{}, fired,
          [](const Hop& hop) {
            return components::VStack()
                .offset(0.0F, static_cast<float>(hop.lift))
                .id("ball");
          },
          hops);
    }

   private:
    std::reference_wrapper<const Observable<int>> trigger_;
    std::reference_wrapper<const Observable<int>> noise_;
    bool modifier_;
  };

  static void frame(Tree& tree, std::chrono::nanoseconds now) {
    tree.set_frame_time(now);
    tree.reconcile();
    tree.close_transactions();
  }

  static auto lift(const Tree& tree) -> float {
    const auto found = tree.find(ElementKey{"ball"});
    EXPECT_TRUE(found.has_value());
    if (!found.has_value()) {
      return -1.0F;
    }
    return tree.node(*found)->get().modifiers().effects.offset_y;
  }

  static auto placeholder(const Tree& tree) -> const Node& {
    const auto found = tree.node(*tree.find(ElementKey{"ball"}));
    const Node& animator = found->get();
    for (const Node& child : animator.children()) {
      return child;
    }
    return animator;
  }

  Observable<int> trigger_{0};
  Observable<int> noise_{0};
};

TEST_F(KeyframeAnimatorTest, PlaysOnlyWhenTheTriggerChanges) {
  Tree tree{Launcher{trigger_, noise_, false}, [] { return true; }};
  frame(tree, 0ms);
  EXPECT_FLOAT_EQ(lift(tree), 0.0F);
  EXPECT_FALSE(tree.dirty());
  noise_.set(1);
  frame(tree, 100ms);
  EXPECT_FLOAT_EQ(lift(tree), 0.0F);
  EXPECT_FALSE(tree.dirty());
  trigger_.set(1);
  frame(tree, 1s);
  EXPECT_FLOAT_EQ(lift(tree), 0.0F);
  EXPECT_TRUE(tree.dirty());
  frame(tree, 1500ms);
  EXPECT_NEAR(lift(tree), 5.0F, 1.0e-4F);
  noise_.set(2);
  frame(tree, 2s);
  EXPECT_NEAR(lift(tree), 10.0F, 1.0e-4F);
  frame(tree, 2500ms);
  EXPECT_NEAR(lift(tree), 5.0F, 1.0e-4F);
  EXPECT_TRUE(tree.dirty());
  frame(tree, 3s);
  EXPECT_FLOAT_EQ(lift(tree), 0.0F);
  EXPECT_FALSE(tree.dirty());
  frame(tree, 4s);
  EXPECT_FLOAT_EQ(lift(tree), 0.0F);
}

TEST_F(KeyframeAnimatorTest, RetriggeringMidFlightStartsFromTheCurrentValue) {
  Tree tree{Launcher{trigger_, noise_, false}, [] { return true; }};
  frame(tree, 0ms);
  trigger_.set(1);
  frame(tree, 0ms);
  frame(tree, 500ms);
  EXPECT_NEAR(lift(tree), 5.0F, 1.0e-4F);
  trigger_.set(2);
  frame(tree, 500ms);
  EXPECT_NEAR(lift(tree), 5.0F, 1.0e-4F);
  frame(tree, 1s);
  EXPECT_NEAR(lift(tree), 7.5F, 1.0e-4F);
}

TEST_F(KeyframeAnimatorTest, ModifierFormDrivesAPlaceholderAroundTheContent) {
  Tree tree{Launcher{trigger_, noise_, true}, [] { return true; }};
  frame(tree, 0ms);
  const Node& wrapper = placeholder(tree);
  ASSERT_EQ(wrapper.child_count(), 1U);
  const NodeId content = (*wrapper.children().begin()).id();
  EXPECT_FLOAT_EQ(wrapper.modifiers().effects.offset_y, 0.0F);
  trigger_.set(1);
  frame(tree, 0ms);
  frame(tree, 250ms);
  EXPECT_NEAR(placeholder(tree).modifiers().effects.offset_y, 2.5F, 1.0e-4F);
  ASSERT_EQ(placeholder(tree).child_count(), 1U);
  EXPECT_EQ((*placeholder(tree).children().begin()).id(), content);
  EXPECT_TRUE((*placeholder(tree).children().begin())
                  .modifiers()
                  .frame.width.has_value());
}

}
