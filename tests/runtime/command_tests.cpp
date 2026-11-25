#include <arisa/components/button.hpp>
#include <arisa/components/hint_bar.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/command_hint.hpp>
#include <arisa/composition/command_options.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/move_command_direction.hpp>
#include <arisa/composition/sensory_feedback.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/interaction/confirm_back_layout.hpp>
#include <arisa/interaction/interaction_options.hpp>
#include <arisa/provider/audio/provider.hpp>
#include <arisa/provider/haptics/provider.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>

#include <focus_fixture.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <optional>
#include <vector>

namespace arisa::runtime {

using provider::Button;

class CommandTest : public FocusFixture {
 protected:
  struct Counters {
    int inner{0};
    int outer{0};
    int inner_exit{0};
    int outer_exit{0};
    int play{0};
    int activated{0};
    int moves{0};
    int fast{0};
    int held{0};
    int secret{0};
    std::optional<composition::MoveCommandDirection> last;
  };

  class Bubbling final : public composition::Component {
   public:
    explicit Bubbling(Counters* counters) noexcept : counters_(counters) {}

    auto operator==(const Bubbling&) const -> bool = default;

    auto body(composition::Context&) const -> composition::Element override {
      Counters* counters = counters_;
      return components::VStack(
                 components::HStack(
                     components::Button("a",
                                        [counters] { ++counters->activated; })
                         .frame({.width = 100.0F, .height = 50.0F})
                         .id("a"))
                     .on_command(Button::y, "Inner",
                                 [counters] { ++counters->inner; })
                     .on_command(
                         Button::minus, "Secret",
                         [counters] { ++counters->secret; },
                         composition::CommandOptions{.hidden = true})
                     .on_exit_command("Close",
                                      [counters] { ++counters->inner_exit; }),
                 button("b"), components::HintBar{})
          .on_command(Button::y, "Outer", [counters] { ++counters->outer; })
          .on_exit_command([counters] { ++counters->outer_exit; })
          .on_play_pause_command([counters] { ++counters->play; })
          .on_command(
              Button::zr, "Faster", [counters] { ++counters->fast; },
              composition::CommandOptions{
                  .trigger = composition::CommandTrigger::repeat})
          .on_command(
              Button::zl, "Hold", [counters] { ++counters->held; },
              composition::CommandOptions{
                  .trigger = composition::CommandTrigger::long_press});
    }

   private:
    Counters* counters_;
  };

  class Moving final : public composition::Component {
   public:
    explicit Moving(Counters* counters) noexcept : counters_(counters) {}

    auto operator==(const Moving&) const -> bool = default;

    auto body(composition::Context&) const -> composition::Element override {
      Counters* counters = counters_;
      return components::HStack(button("a"), button("b"))
          .on_move_command(
              [counters](composition::MoveCommandDirection direction) {
                ++counters->moves;
                counters->last = direction;
              });
    }

   private:
    Counters* counters_;
  };

  class Sensing final : public composition::Component {
   public:
    static constexpr composition::StateKey<int> count{"count", 0};

    auto operator==(const Sensing&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      const auto value = context.state(count);
      return components::VStack(
                 components::Button("bump",
                                    [value] { value.set(value.get() + 1); })
                     .id("bump"))
          .sensory_feedback(composition::SensoryFeedback::success, value.get())
          .sensory_feedback(composition::SensoryFeedback::error, value.get());
    }
  };

  static auto hint(Button which, std::string label)
      -> composition::CommandHint {
    return composition::CommandHint{.button = which, .label = std::move(label)};
  }
};

TEST_F(CommandTest, CommandsBubbleAndTheFirstHandlerWins) {
  provider::headless::HeadlessProviderFactory providers;
  Counters counters;
  Application application = start(providers, Bubbling{&counters});
  EXPECT_EQ(focused_key(application), "a");
  press(providers, application, Button::y);
  EXPECT_EQ(counters.inner, 1);
  EXPECT_EQ(counters.outer, 0);
  press(providers, application, Button::b);
  EXPECT_EQ(counters.inner_exit, 1);
  EXPECT_EQ(counters.outer_exit, 0);
  press(providers, application, Button::x);
  EXPECT_EQ(counters.play, 1);
  press(providers, application, Button::a);
  EXPECT_EQ(counters.activated, 1);
  press(providers, application, Button::dpad_down);
  EXPECT_EQ(focused_key(application), "b");
  press(providers, application, Button::y);
  EXPECT_EQ(counters.outer, 1);
  EXPECT_EQ(counters.inner, 1);
  press(providers, application, Button::b);
  EXPECT_EQ(counters.outer_exit, 1);
  const std::vector<provider::SoundRole> sounds = providers.audio().played();
  EXPECT_EQ(std::ranges::count(sounds, provider::SoundRole::back), 2);
  EXPECT_EQ(std::ranges::count(sounds, provider::SoundRole::click), 1);
}

TEST_F(CommandTest, MoveCommandsConsumeDirectionalInput) {
  provider::headless::HeadlessProviderFactory providers;
  Counters counters;
  Application application = start(providers, Moving{&counters});
  press(providers, application, Button::dpad_right);
  EXPECT_EQ(counters.moves, 1);
  EXPECT_EQ(counters.last, composition::MoveCommandDirection::right);
  EXPECT_EQ(focused_key(application), "a");
}

TEST_F(CommandTest, ConfirmBackLayoutSwapsRoles) {
  provider::headless::HeadlessProviderFactory providers;
  Counters counters;
  Application application =
      start(providers, Bubbling{&counters},
            interaction::InteractionOptions{
                .layout = interaction::ConfirmBackLayout::xbox});
  press(providers, application, Button::a);
  EXPECT_EQ(counters.inner_exit, 1);
  EXPECT_EQ(counters.activated, 0);
  press(providers, application, Button::b);
  EXPECT_EQ(counters.activated, 1);
  const auto& hints = application.interaction().hints();
  ASSERT_FALSE(hints.empty());
  EXPECT_EQ(hints.front(), hint(Button::b, "OK"));
  EXPECT_EQ(hints[1], hint(Button::a, "Close"));
}

TEST_F(CommandTest, PlayPauseFollowsTheConfiguredButton) {
  provider::headless::HeadlessProviderFactory providers;
  Counters counters;
  Application application =
      start(providers, Bubbling{&counters},
            interaction::InteractionOptions{.play_pause = Button::plus});
  press(providers, application, Button::x);
  EXPECT_EQ(counters.play, 0);
  press(providers, application, Button::plus);
  EXPECT_EQ(counters.play, 1);
}

TEST_F(CommandTest, RepeatAndLongPressVariants) {
  provider::headless::HeadlessProviderFactory providers;
  Counters counters;
  Application application = start(providers, Bubbling{&counters});
  hold(providers, application, pad({Button::zr}),
       std::chrono::milliseconds{1000});
  EXPECT_EQ(counters.fast, 11);
  hold(providers, application, pad({Button::zl}),
       std::chrono::milliseconds{300});
  EXPECT_EQ(counters.held, 0);
  hold(providers, application, pad({Button::zl}),
       std::chrono::milliseconds{600});
  EXPECT_EQ(counters.held, 1);
}

TEST_F(CommandTest, HintBarListsTheCommandsAtTheFocus) {
  provider::headless::HeadlessProviderFactory providers;
  Counters counters;
  Application application = start(providers, Bubbling{&counters});
  frames(application, 2);
  const std::vector<composition::CommandHint> inner{
      hint(Button::a, "OK"),         hint(Button::b, "Close"),
      hint(Button::x, "Play/Pause"), hint(Button::y, "Inner"),
      hint(Button::zl, "Hold"),      hint(Button::zr, "Faster")};
  EXPECT_EQ(application.interaction().hints(), inner);
  EXPECT_TRUE(shows(application, "Inner"));
  EXPECT_TRUE(shows(application, "Close"));
  EXPECT_TRUE(shows(application, "Y"));
  EXPECT_FALSE(shows(application, "Secret"));
  EXPECT_FALSE(shows(application, "Outer"));
  press(providers, application, Button::dpad_down);
  frames(application, 2);
  const std::vector<composition::CommandHint> outer{
      hint(Button::a, "OK"),         hint(Button::b, "Back"),
      hint(Button::x, "Play/Pause"), hint(Button::y, "Outer"),
      hint(Button::zl, "Hold"),      hint(Button::zr, "Faster")};
  EXPECT_EQ(application.interaction().hints(), outer);
  EXPECT_TRUE(shows(application, "Outer"));
  EXPECT_FALSE(shows(application, "Inner"));
}

TEST_F(CommandTest, SensoryFeedbackPlaysTheLatestRequestOfAFrame) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, Sensing{});
  EXPECT_TRUE(providers.haptics().played().empty());
  ASSERT_TRUE(application.root().activate(composition::ElementKey{"bump"}));
  frames(application, 2);
  const std::vector<provider::SensoryFeedback> expected{
      provider::SensoryFeedback::error};
  EXPECT_EQ(providers.haptics().played(), expected);
}

TEST_F(CommandTest, AHoldDurationMakesALongPressCommandWaitForIt) {
  provider::headless::HeadlessProviderFactory providers;
  int reset = 0;
  Application application = start(
      providers,
      button("binding").on_command(
          Button::x, "Reset", [&reset] { ++reset; },
          composition::CommandOptions{.hold = std::chrono::milliseconds{750}}));
  press(providers, application, Button::x);
  EXPECT_EQ(reset, 0);
  hold(providers, application, pad({Button::x}),
       std::chrono::milliseconds{600});
  EXPECT_EQ(reset, 0);
  hold(providers, application, pad({Button::x}),
       std::chrono::milliseconds{800});
  EXPECT_EQ(reset, 1);
}

}
