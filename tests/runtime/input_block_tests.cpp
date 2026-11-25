#include "focus_fixture.hpp"

#include <arisa/components/button.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/exclusive_frame.hpp>
#include <arisa/composition/overlay_layer.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/provider/threading/thread_role.hpp>
#include <arisa/provider/threading/thread_spec.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/runtime/button_capture.hpp>
#include <arisa/runtime/input_block.hpp>
#include <arisa/runtime/input_passthrough.hpp>
#include <arisa/runtime/overlay_behaviour.hpp>
#include <arisa/runtime/overlay_handle.hpp>
#include <arisa/runtime/overlays.hpp>
#include <arisa/runtime/passed_input.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <stop_token>
#include <utility>

namespace arisa::runtime {

class InputBlockTest : public FocusFixture {
 protected:
  using Button = provider::Button;

  static auto counter(const std::shared_ptr<int>& presses)
      -> composition::Element {
    return components::VStack(
        components::Button("Go", [presses] { ++*presses; })
            .frame({.width = 120.0F, .height = 50.0F})
            .id("go"),
        components::Button("Other", [] {})
            .frame({.width = 120.0F, .height = 50.0F})
            .id("other"));
  }

  static auto panel() -> composition::Element {
    return components::VStack(button("panel.first"), button("panel.second"));
  }

  static void settle(Application& application) { frames(application, 30); }

  auto tappable(Application& application, bool dismiss) -> OverlayHandle {
    return application.root().overlays().show(
        composition::OverlayLayer::dialog,
        components::Button("Tap", [presses = presses_] { ++*presses; })
            .frame({.width = 120.0F, .height = 50.0F})
            .id("panel.tap"),
        OverlayBehaviour{.input = composition::OverlayInput::modal,
                         .alignment = composition::Alignment::center,
                         .dismiss_on_tap_outside = dismiss});
  }

  static auto key(provider::Key which, bool down) -> provider::InputFrame {
    provider::InputFrame frame;
    frame.keys.push_back(provider::KeyEvent{.key = which, .down = down});
    return frame;
  }

  auto dock(Application& application) -> OverlayHandle {
    return application.root().overlays().show(
        composition::OverlayLayer::dialog, panel(),
        OverlayBehaviour{.input = composition::OverlayInput::modal,
                         .alignment = composition::Alignment::bottom});
  }

  provider::headless::HeadlessProviderFactory providers_;
  std::shared_ptr<int> presses_ = std::make_shared<int>(0);
};

TEST_F(InputBlockTest, InputIsBlockedExactlyWhileAGuardLives) {
  Application application = start(providers_, counter(presses_));
  press(providers_, application, Button::a);
  ASSERT_EQ(*presses_, 1);
  const std::string focused = focused_key(application);
  {
    InputBlock block = application.block_input();
    EXPECT_TRUE(block.active());
    EXPECT_TRUE(application.root().input_blocked());
    press(providers_, application, Button::a);
    press(providers_, application, Button::dpad_down);
    EXPECT_EQ(*presses_, 1);
    EXPECT_EQ(focused_key(application), focused);
  }
  EXPECT_FALSE(application.root().input_blocked());
  press(providers_, application, Button::a);
  EXPECT_EQ(*presses_, 2);
}

TEST_F(InputBlockTest, NestedGuardsBlockUntilTheLastOneEnds) {
  Application application = start(providers_, counter(presses_));
  press(providers_, application, Button::a);
  ASSERT_EQ(*presses_, 1);
  std::optional<InputBlock> outer = application.block_input();
  std::optional<InputBlock> inner = application.block_input();
  press(providers_, application, Button::a);
  outer.reset();
  press(providers_, application, Button::a);
  EXPECT_EQ(*presses_, 1);
  EXPECT_TRUE(application.root().input_blocked());
  inner.reset();
  press(providers_, application, Button::a);
  EXPECT_EQ(*presses_, 2);
}

TEST_F(InputBlockTest, ReleasingEarlyEndsTheScopeOnce) {
  Application application = start(providers_, counter(presses_));
  press(providers_, application, Button::a);
  InputBlock block = application.block_input();
  block.release();
  EXPECT_FALSE(block.active());
  block.release();
  press(providers_, application, Button::a);
  EXPECT_EQ(*presses_, 2);
}

TEST_F(InputBlockTest, AGuardDestroyedOnAnotherThreadUnblocks) {
  Application application = start(providers_, counter(presses_));
  press(providers_, application, Button::a);
  auto block =
      std::make_shared<std::optional<InputBlock>>(application.block_input());
  press(providers_, application, Button::a);
  EXPECT_EQ(*presses_, 1);
  {
    auto worker = providers_.threading().start(
        provider::ThreadSpec{.name = "input-owner",
                             .role = provider::ThreadRole::background},
        [block](const std::stop_token&) { block->reset(); });
    ASSERT_TRUE(worker.has_value());
  }
  frames(application, 1);
  EXPECT_FALSE(application.root().input_blocked());
  press(providers_, application, Button::a);
  EXPECT_EQ(*presses_, 2);
}

TEST_F(InputBlockTest, AGuardTakenOnAnotherThreadBlocksTheNextFrame) {
  Application application = start(providers_, counter(presses_));
  press(providers_, application, Button::a);
  auto block = std::make_shared<std::optional<InputBlock>>();
  Root& root = application.root();
  {
    auto worker = providers_.threading().start(
        provider::ThreadSpec{.name = "input-owner",
                             .role = provider::ThreadRole::background},
        [block, &root](const std::stop_token&) {
          block->emplace(root.block_input());
        });
    ASSERT_TRUE(worker.has_value());
  }
  press(providers_, application, Button::a);
  EXPECT_EQ(*presses_, 1);
  block->reset();
  press(providers_, application, Button::a);
  EXPECT_EQ(*presses_, 2);
}

TEST_F(InputBlockTest, PassthroughButtonsStillReachTheInterface) {
  Application application = start(providers_, counter(presses_));
  press(providers_, application, Button::a);
  InputBlock block = application.block_input();
  {
    InputPassthrough pass = application.pass_input({Button::a});
    EXPECT_TRUE(pass.buttons().contains(Button::a));
    press(providers_, application, Button::a);
    EXPECT_EQ(*presses_, 2);
    const std::string focused = focused_key(application);
    press(providers_, application, Button::dpad_down);
    EXPECT_EQ(focused_key(application), focused);
  }
  press(providers_, application, Button::a);
  EXPECT_EQ(*presses_, 2);
}

TEST_F(InputBlockTest, AButtonHeldAcrossTheReleaseDoesNotActivate) {
  Application application = start(providers_, counter(presses_));
  press(providers_, application, Button::a);
  std::optional<InputBlock> block = application.block_input();
  for (int frame = 0; frame < 3; ++frame) {
    providers_.input().push(pad({Button::a}));
    frames(application, 1);
  }
  block.reset();
  for (int frame = 0; frame < 3; ++frame) {
    providers_.input().push(pad({Button::a}));
    frames(application, 1);
  }
  providers_.input().push(pad({}));
  frames(application, 2);
  EXPECT_EQ(*presses_, 1);
  press(providers_, application, Button::a);
  EXPECT_EQ(*presses_, 2);
}

TEST_F(InputBlockTest, ABlockStartedWhileAButtonIsHeldCancelsThePress) {
  Application application = start(providers_, counter(presses_));
  press(providers_, application, Button::a);
  ASSERT_EQ(*presses_, 1);
  providers_.input().push(pad({Button::a}));
  frames(application, 1);
  std::optional<InputBlock> block = application.block_input();
  providers_.input().push(pad({Button::a}));
  frames(application, 1);
  providers_.input().push(pad({}));
  frames(application, 2);
  EXPECT_EQ(*presses_, 1);
  block.reset();
  frames(application, 2);
  EXPECT_EQ(*presses_, 1);
  press(providers_, application, Button::a);
  EXPECT_EQ(*presses_, 2);
}

TEST_F(InputBlockTest, ABlockStartedDuringATouchCancelsTheTap) {
  Application application = start(providers_, counter(presses_));
  const provider::PixelPoint point = centre(application, "go");
  provider::InputFrame down;
  down.touches.push_back(provider::Touch{.id = provider::TouchId{5},
                                         .position = point,
                                         .phase = provider::TouchPhase::began});
  providers_.input().push(down);
  frames(application, 1);
  std::optional<InputBlock> block = application.block_input();
  frames(application, 2);
  block.reset();
  provider::InputFrame up;
  up.touches.push_back(provider::Touch{.id = provider::TouchId{5},
                                       .position = point,
                                       .phase = provider::TouchPhase::ended});
  providers_.input().push(up);
  frames(application, 3);
  EXPECT_EQ(*presses_, 0);
  tap(providers_, application, point);
  EXPECT_EQ(*presses_, 1);
}

TEST_F(InputBlockTest, AGuardThatOutlivesItsApplicationIsHarmless) {
  std::optional<InputBlock> block;
  {
    Application application = start(providers_, counter(presses_));
    block.emplace(application.block_input());
  }
  EXPECT_TRUE(block->active());
  block.reset();
}

TEST_F(InputBlockTest, AnimationsKeepRunningWhileBlocked) {
  Application application = start(providers_, counter(presses_));
  InputBlock block = application.block_input();
  const std::uint64_t before = application.root().frames();
  frames(application, 3);
  EXPECT_EQ(application.root().frames(), before + 3);
}

TEST_F(InputBlockTest, PassedDpadMovesFocusInsideAModalOverlay) {
  Application application = start(providers_, counter(presses_));
  composition::ExclusiveFrame exclusive =
      application.exclusive_frame(composition::ExclusiveFrame::Options{
          .keep = {composition::OverlayLayer::dialog,
                   composition::OverlayLayer::toast},
      });
  InputBlock block = application.block_input();
  InputPassthrough pass = application.pass_input(
      {Button::a, Button::b, Button::dpad_up, Button::dpad_down});
  OverlayHandle shown = dock(application);
  settle(application);
  EXPECT_EQ(focused_key(application), "panel.first");
  press(providers_, application, Button::dpad_down);
  EXPECT_EQ(focused_key(application), "panel.second");
  press(providers_, application, Button::dpad_up);
  EXPECT_EQ(focused_key(application), "panel.first");
}

TEST_F(InputBlockTest, PassedDpadMovesFocusInsideAnOverlayShownBeforeTheBlock) {
  Application application = start(providers_, counter(presses_));
  composition::ExclusiveFrame exclusive =
      application.exclusive_frame(composition::ExclusiveFrame::Options{
          .keep = {composition::OverlayLayer::dialog},
      });
  OverlayHandle shown = dock(application);
  settle(application);
  InputBlock block = application.block_input();
  InputPassthrough pass =
      application.pass_input({Button::dpad_up, Button::dpad_down});
  settle(application);
  EXPECT_EQ(focused_key(application), "panel.first");
  press(providers_, application, Button::dpad_down);
  EXPECT_EQ(focused_key(application), "panel.second");
}

TEST_F(InputBlockTest, PassedInputNavigateUnderABlock) {
  Application application = start(providers_, counter(presses_));
  InputBlock block = application.block_input();
  InputPassthrough pass =
      application.pass_input({}, PassedInput{.left_stick = true});
  EXPECT_TRUE(pass.passed().left_stick);
  EXPECT_EQ(focused_key(application), "go");
  hold(providers_, application, pad({}, provider::Stick{.x = 0.0F, .y = -1.0F}),
       std::chrono::milliseconds{48});
  EXPECT_EQ(focused_key(application), "other");
}

TEST_F(InputBlockTest, SticksStayBlockedUnlessPassed) {
  Application application = start(providers_, counter(presses_));
  InputBlock block = application.block_input();
  InputPassthrough pass = application.pass_input({Button::dpad_down});
  hold(providers_, application, pad({}, provider::Stick{.x = 0.0F, .y = -1.0F}),
       std::chrono::milliseconds{48});
  EXPECT_EQ(focused_key(application), "go");
}

TEST_F(InputBlockTest, ALaterBlockClosesAnEarlierPassthrough) {
  Application application = start(providers_, counter(presses_));
  press(providers_, application, Button::a);
  ASSERT_EQ(*presses_, 1);
  InputBlock outer = application.block_input();
  InputPassthrough pass = application.pass_input({Button::a});
  press(providers_, application, Button::a);
  ASSERT_EQ(*presses_, 2);
  std::optional<InputBlock> capture = application.block_input();
  press(providers_, application, Button::a);
  EXPECT_EQ(*presses_, 2);
  capture.reset();
  press(providers_, application, Button::a);
  EXPECT_EQ(*presses_, 3);
}

TEST_F(InputBlockTest, APassthroughTakenAfterTheNewestBlockOpensIt) {
  Application application = start(providers_, counter(presses_));
  press(providers_, application, Button::a);
  InputBlock outer = application.block_input();
  std::optional<InputPassthrough> early = application.pass_input({Button::a});
  InputBlock inner = application.block_input();
  InputPassthrough late = application.pass_input({Button::a});
  press(providers_, application, Button::a);
  EXPECT_EQ(*presses_, 2);
  early.reset();
  press(providers_, application, Button::a);
  EXPECT_EQ(*presses_, 3);
}

TEST_F(InputBlockTest, ACaptureReadsTheHeldMaskOfEachConnectedPad) {
  Application application = start(providers_, counter(presses_));
  ButtonCapture capture = application.capture_buttons();
  provider::InputFrame frame;
  frame.pads[0] = provider::PadState{.connected = true,
                                     .buttons = {Button::a},
                                     .left = {},
                                     .right = {}};
  frame.pads[2] = provider::PadState{.connected = true,
                                     .buttons = {Button::l, Button::r},
                                     .left = {},
                                     .right = {}};
  frame.pads[5] = provider::PadState{.connected = true,
                                     .buttons = {Button::zl, Button::zr},
                                     .left = {},
                                     .right = {}};
  providers_.input().push(frame);
  frames(application, 1);
  EXPECT_EQ(capture.held(0), (provider::Buttons{Button::a}));
  EXPECT_EQ(capture.held(2), (provider::Buttons{Button::l, Button::r}));
  EXPECT_EQ(capture.held(5), (provider::Buttons{Button::zl, Button::zr}));
  EXPECT_TRUE(capture.held(1).empty());
  EXPECT_TRUE(capture.held(provider::max_pads).empty());
  EXPECT_EQ(capture.held(), (provider::Buttons{Button::a, Button::l, Button::r,
                                               Button::zl, Button::zr}));
  EXPECT_TRUE(capture.connected(0));
  EXPECT_FALSE(capture.connected(1));
  EXPECT_TRUE(capture.connected(2));
  EXPECT_TRUE(capture.connected(5));
  EXPECT_FALSE(capture.connected(provider::max_pads));
  EXPECT_EQ(*presses_, 0);
  capture.release();
  EXPECT_FALSE(capture.connected(2));
  EXPECT_TRUE(capture.held(2).empty());
}

TEST_F(InputBlockTest, ACaptureReadsSideButtonsAndRawSticks) {
  Application application = start(providers_, counter(presses_));
  ButtonCapture capture = application.capture_buttons();
  provider::InputFrame frame;
  frame.pads[1] = provider::PadState{.connected = true,
                                     .buttons = {Button::sl, Button::sr},
                                     .left = {.x = -0.9F, .y = 0.1F},
                                     .right = {.x = 0.0F, .y = 0.8F}};
  providers_.input().push(frame);
  frames(application, 1);
  EXPECT_EQ(capture.held(1), (provider::Buttons{Button::sl, Button::sr}));
  const provider::PadState raw = capture.pad(1);
  EXPECT_TRUE(raw.connected);
  EXPECT_LT(raw.left.x, -0.5F);
  EXPECT_GT(raw.right.y, 0.5F);
  EXPECT_FALSE(capture.pad(provider::max_pads).connected);
  EXPECT_EQ(*presses_, 0);
  capture.release();
  EXPECT_FALSE(capture.pad(1).connected);
}

TEST_F(InputBlockTest, ACaptureSeesHeldButtonsWhileTheInterfaceSeesNone) {
  Application application = start(providers_, counter(presses_));
  press(providers_, application, Button::a);
  ASSERT_EQ(*presses_, 1);
  InputPassthrough pass = application.pass_input({Button::a});
  ButtonCapture capture = application.capture_buttons();
  EXPECT_TRUE(capture.active());
  EXPECT_TRUE(capture.held().empty());
  providers_.input().push(pad({Button::a, Button::zl}));
  frames(application, 1);
  EXPECT_EQ(capture.held(), (provider::Buttons{Button::a, Button::zl}));
  EXPECT_EQ(capture.held(0), (provider::Buttons{Button::a, Button::zl}));
  EXPECT_TRUE(capture.held(1).empty());
  providers_.input().push(pad({}));
  frames(application, 2);
  EXPECT_TRUE(capture.held().empty());
  EXPECT_EQ(*presses_, 1);
  capture.release();
  EXPECT_FALSE(capture.active());
  press(providers_, application, Button::a);
  EXPECT_EQ(*presses_, 2);
}

TEST_F(InputBlockTest, PassedTouchReachesAModalOverlayAsUnblockedTouchDoes) {
  Application application = start(providers_, counter(presses_));
  InputBlock block = application.block_input();
  OverlayHandle shown = tappable(application, false);
  settle(application);
  const provider::PixelPoint point = centre(application, "panel.tap");
  tap(providers_, application, point);
  EXPECT_EQ(*presses_, 0);
  InputPassthrough pass =
      application.pass_input({}, PassedInput{.touch = true});
  EXPECT_TRUE(pass.passed().touch);
  tap(providers_, application, point);
  EXPECT_EQ(*presses_, 1);
}

TEST_F(InputBlockTest, PassedTouchOutsideAModalOverlayDismissesItAsUnblocked) {
  Application application = start(providers_, counter(presses_));
  InputBlock block = application.block_input();
  OverlayHandle shown = tappable(application, true);
  settle(application);
  const provider::PixelPoint outside{.x = 4.0F, .y = 4.0F};
  tap(providers_, application, outside);
  settle(application);
  ASSERT_TRUE(node_of(application, "panel.tap").has_value());
  InputPassthrough pass =
      application.pass_input({}, PassedInput{.touch = true});
  tap(providers_, application, outside);
  settle(application);
  EXPECT_FALSE(node_of(application, "panel.tap").has_value());
  EXPECT_EQ(*presses_, 0);
}

TEST_F(InputBlockTest, ALaterBlockClosingTouchCancelsATapInFlight) {
  Application application = start(providers_, counter(presses_));
  InputBlock outer = application.block_input();
  InputPassthrough pass =
      application.pass_input({}, PassedInput{.touch = true});
  OverlayHandle shown = tappable(application, false);
  settle(application);
  const provider::PixelPoint point = centre(application, "panel.tap");
  provider::InputFrame down;
  down.touches.push_back(provider::Touch{.id = provider::TouchId{9},
                                         .position = point,
                                         .phase = provider::TouchPhase::began});
  providers_.input().push(down);
  frames(application, 1);
  std::optional<InputBlock> capture = application.block_input();
  frames(application, 1);
  capture.reset();
  provider::InputFrame up;
  up.touches.push_back(provider::Touch{.id = provider::TouchId{9},
                                       .position = point,
                                       .phase = provider::TouchPhase::ended});
  providers_.input().push(up);
  frames(application, 3);
  EXPECT_EQ(*presses_, 0);
  tap(providers_, application, point);
  EXPECT_EQ(*presses_, 1);
}

TEST_F(InputBlockTest, KeyboardEquivalentsOfPassedButtonsNavigate) {
  Application application = start(providers_, counter(presses_));
  InputBlock block = application.block_input();
  InputPassthrough pass = application.pass_input({Button::dpad_down});
  providers_.input().push(key(provider::Key::enter, true));
  providers_.input().push(key(provider::Key::enter, false));
  frames(application, 3);
  EXPECT_EQ(*presses_, 0);
  providers_.input().push(key(provider::Key::down, true));
  providers_.input().push(key(provider::Key::down, false));
  frames(application, 3);
  EXPECT_EQ(focused_key(application), "other");
}

}
