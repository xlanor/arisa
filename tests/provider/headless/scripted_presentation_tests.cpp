#include <arisa/provider/headless/scripted_input.hpp>
#include <arisa/provider/headless/scripted_surface.hpp>

#include <gtest/gtest.h>

namespace arisa::provider::headless {

TEST(ScriptedSurfaceTest, ReportsScriptedSizeScaleAndEvents) {
  ScriptedSurface surface;
  EXPECT_EQ(surface.size(), ScriptedSurface::default_size);
  EXPECT_EQ(surface.scale(), SurfaceScale::unit());
  EXPECT_FALSE(surface.poll().has_value());
  surface.resize({.width = SurfaceWidth{1920}, .height = SurfaceHeight{1080}});
  surface.set_scale(*SurfaceScale::create(1.5F));
  surface.push(SurfaceEvent::operation_mode_changed);
  EXPECT_EQ(surface.poll(), SurfaceEvent::resized);
  EXPECT_EQ(surface.poll(), SurfaceEvent::scale_changed);
  EXPECT_EQ(surface.poll(), SurfaceEvent::operation_mode_changed);
  EXPECT_FALSE(surface.poll().has_value());
  EXPECT_EQ(surface.size().width.value(), 1920U);
  EXPECT_FLOAT_EQ(surface.scale().value(), 1.5F);
}

TEST(ScriptedInputTest, ReplaysFramesAndAppliesTheDeadzone) {
  ScriptedInput input;
  InputFrame frame;
  frame.pads[0] = PadState{.connected = true,
                           .buttons = Buttons{Button::a},
                           .left = Stick{.x = 0.05F, .y = 0.0F},
                           .right = Stick{.x = 1.0F, .y = 0.0F}};
  frame.touches.push_back(Touch{.id = TouchId{3},
                                .position = {.x = 10.0F, .y = 20.0F},
                                .phase = TouchPhase::began});
  frame.text = "k";
  input.push(frame);
  EXPECT_EQ(input.pending(), 1U);
  const InputFrame sampled = input.sample();
  EXPECT_TRUE(sampled.pads[0].buttons.contains(Button::a));
  EXPECT_EQ(sampled.pads[0].left, Stick{});
  EXPECT_FLOAT_EQ(sampled.pads[0].right.x, 1.0F);
  EXPECT_EQ(sampled.touches, frame.touches);
  EXPECT_EQ(sampled.text, "k");
  EXPECT_EQ(input.sample(), InputFrame{});
}

}
