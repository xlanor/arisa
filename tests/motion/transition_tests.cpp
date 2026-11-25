#include <arisa/motion/transition.hpp>

#include <gtest/gtest.h>

namespace arisa::motion {

TEST(TransitionTest, EffectsDescribeTheHiddenState) {
  EXPECT_FLOAT_EQ(Transition::opacity().insertion().opacity, 0.0F);
  EXPECT_FLOAT_EQ(Transition::scale().insertion().scale_x, 0.0F);
  EXPECT_EQ(Transition::scale(0.5F, UnitPoint::top).insertion().anchor,
            UnitPoint::top);
  EXPECT_FLOAT_EQ(Transition::move(theme::Edge::leading).insertion().relative_x,
                  -1.0F);
  EXPECT_FLOAT_EQ(Transition::move(theme::Edge::bottom).removal().relative_y,
                  1.0F);
  EXPECT_FLOAT_EQ(Transition::offset(12.0F, -4.0F).insertion().offset_y, -4.0F);
  EXPECT_EQ(Transition::identity().insertion(), Transition::Effect{});
}

TEST(TransitionTest, SlideAndPushAreAsymmetric) {
  const Transition slide = Transition::slide();
  EXPECT_FLOAT_EQ(slide.insertion().relative_x, -1.0F);
  EXPECT_FLOAT_EQ(slide.removal().relative_x, 1.0F);
  const Transition push = Transition::push(theme::Edge::top);
  EXPECT_FLOAT_EQ(push.insertion().relative_y, -1.0F);
  EXPECT_FLOAT_EQ(push.removal().relative_y, 1.0F);
  EXPECT_FLOAT_EQ(push.insertion().opacity, 0.0F);
  const Transition mixed =
      Transition::asymmetric(Transition::opacity(), Transition::scale(0.5F));
  EXPECT_FLOAT_EQ(mixed.insertion().opacity, 0.0F);
  EXPECT_FLOAT_EQ(mixed.insertion().scale_x, 1.0F);
  EXPECT_FLOAT_EQ(mixed.removal().scale_x, 0.5F);
  EXPECT_FLOAT_EQ(mixed.removal().opacity, 1.0F);
}

TEST(TransitionTest, CombinedStacksEffects) {
  const Transition combined =
      Transition::move(theme::Edge::trailing)
          .combined(Transition::opacity())
          .combined(Transition::scale(0.5F, UnitPoint::bottom));
  const Transition::Effect& effect = combined.insertion();
  EXPECT_FLOAT_EQ(effect.relative_x, 1.0F);
  EXPECT_FLOAT_EQ(effect.opacity, 0.0F);
  EXPECT_FLOAT_EQ(effect.scale_y, 0.5F);
  EXPECT_EQ(effect.anchor, UnitPoint::bottom);
}

TEST(TransitionTest, ProgressInterpolatesTowardIdentity) {
  const Transition::Effect effect = Transition::move(theme::Edge::leading)
                                        .combined(Transition::opacity())
                                        .insertion();
  const Transition::Effect half = effect.at(0.5);
  EXPECT_FLOAT_EQ(half.opacity, 0.5F);
  EXPECT_FLOAT_EQ(half.relative_x, -0.5F);
  EXPECT_EQ(effect.at(1.0), Transition::Effect{});
  EXPECT_EQ(effect.at(0.0), effect);
}

TEST(TransitionTest, ReducedMotionTurnsMovementIntoOpacity) {
  const Transition::Effect moved =
      Transition::move(theme::Edge::leading).insertion().reduced();
  EXPECT_FLOAT_EQ(moved.opacity, 0.0F);
  EXPECT_FLOAT_EQ(moved.relative_x, 0.0F);
  EXPECT_FALSE(moved.moves());
  EXPECT_EQ(Transition::identity().insertion().reduced(), Transition::Effect{});
}

}
