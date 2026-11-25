#include <arisa/appearance/appearance_policy.hpp>
#include <arisa/components/button.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/command_options.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/focus_state.hpp>
#include <arisa/composition/sensory_feedback.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/controls/closed_range.hpp>
#include <arisa/controls/picker.hpp>
#include <arisa/controls/picker_style.hpp>
#include <arisa/controls/slider.hpp>
#include <arisa/controls/stepper.hpp>
#include <arisa/controls/toggle.hpp>
#include <arisa/provider/audio/provider.hpp>
#include <arisa/provider/haptics/haptic_strength.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/runtime/application_options.hpp>
#include <arisa/structure/haptic_strength_slider.hpp>

#include <focus_fixture.hpp>
#include <runtime/feedback_player.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <format>
#include <functional>
#include <optional>
#include <print>
#include <string>
#include <utility>
#include <vector>

namespace arisa::runtime {

using composition::Element;
using composition::SensoryFeedback;
using provider::Button;

class SensoryFeedbackTest : public FocusFixture {
 protected:
  static constexpr std::chrono::milliseconds pause{300};

  static constexpr composition::StateKey<int> count_key{"feedback-count", 0};
  static constexpr composition::StateKey<int> other_key{"feedback-other", 0};
  static constexpr composition::StateKey<bool> flag_key{"feedback-flag", false};
  static constexpr composition::StateKey<double> level_key{"feedback-level",
                                                           0.5};
  static constexpr composition::StateKey<int> amount_key{"feedback-amount", 3};
  static constexpr composition::StateKey<int> shade_key{"feedback-shade", 1};
  static constexpr composition::FocusState<int> cell_key{"feedback-cell"};

  struct Probe {
    std::optional<composition::StateHandle<int>> count;
    std::optional<composition::StateHandle<int>> other;
    std::optional<composition::StateHandle<bool>> flag;
    std::optional<composition::StateHandle<double>> level;
  };

  using Build = std::function<Element(Probe&, composition::Context&)>;

  class Host final : public composition::Component {
   public:
    Host(Probe* probe, const Build* build) noexcept
        : probe_(probe), build_(build) {}

    auto operator==(const Host&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      return (*build_)(*probe_, context);
    }

   private:
    Probe* probe_;
    const Build* build_;
  };

  static auto played(provider::headless::HeadlessProviderFactory& providers)
      -> std::vector<SensoryFeedback> {
    return providers.haptics().played();
  }

  static auto count_of(provider::headless::HeadlessProviderFactory& providers,
                       const SensoryFeedback& feedback) -> std::size_t {
    const std::vector<SensoryFeedback> all = played(providers);
    return static_cast<std::size_t>(std::ranges::count(all, feedback));
  }

  static void rest(provider::headless::HeadlessProviderFactory& providers,
                   Application& application) {
    ASSERT_TRUE(providers.clock().advance(pause).has_value());
    frames(application, 1);
  }

  static void bump(provider::headless::HeadlessProviderFactory& providers,
                   Application& application, std::string_view key) {
    ASSERT_TRUE(application.root().activate(composition::ElementKey{key}));
    frames(application, 2);
    rest(providers, application);
  }

  static auto configured(provider::headless::HeadlessProviderFactory& providers,
                         Element content, appearance::AppearancePolicy policy)
      -> Application {
    auto created = Application::create(
        providers, std::move(content),
        ApplicationOptions{
            .executors = std::nullopt,
            .fonts = {},
            .atlas = {},
            .interaction = {},
            .appearance = AppearanceOptions{.policy = std::move(policy),
                                            .storage = std::nullopt}});
    EXPECT_TRUE(created.has_value());
    Application started = std::move(*created);
    frames(started, 2);
    return started;
  }

  static auto strength_of(const Application& application)
      -> provider::HapticStrength {
    return application.appearance()->settings().get().haptic_strength;
  }

  static auto light() -> SensoryFeedback {
    return SensoryFeedback::impact(SensoryFeedback::Weight::light);
  }

  static auto shades() -> Element {
    return components::Group(components::Text("Red").tag(0),
                             components::Text("Green").tag(1),
                             components::Text("Blue").tag(2));
  }

  static auto counter(composition::StateHandle<int> value, std::string key)
      -> Element {
    return components::Button(key, [value] { value.set(value.get() + 1); })
        .id(std::move(key));
  }
};

TEST_F(SensoryFeedbackTest, ModifierPlaysOnChangeAndNotOnAppear) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context& context) {
    const auto count = context.state(count_key);
    seen.count = count;
    return components::VStack(counter(count, "bump"))
        .sensory_feedback(SensoryFeedback::success, count.get());
  };
  Application application = start(providers, Host{&probe, &build});
  frames(application, 3);
  EXPECT_TRUE(played(providers).empty());
  bump(providers, application, "bump");
  EXPECT_EQ(played(providers), std::vector{SensoryFeedback::success});
  bump(providers, application, "bump");
  EXPECT_EQ(played(providers),
            (std::vector{SensoryFeedback::success, SensoryFeedback::success}));
  frames(application, 5);
  EXPECT_EQ(played(providers).size(), 2U);
}

TEST_F(SensoryFeedbackTest, PressReleaseAndSelectionVariantsPlayAndCoalesce) {
  using Press = SensoryFeedback::PressFeedback;
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context& context) {
    const auto count = context.state(count_key);
    seen.count = count;
    return components::VStack(counter(count, "bump"))
        .sensory_feedback(SensoryFeedback::press(Press::button), count.get())
        .sensory_feedback(SensoryFeedback::selection_feedback(
                              SensoryFeedback::SelectionFeedback::on),
                          count.get())
        .sensory_feedback(
            SensoryFeedback::release(SensoryFeedback::ReleaseFeedback::slider),
            count.get());
  };
  Application application = start(providers, Host{&probe, &build});
  bump(providers, application, "bump");
  const std::vector<SensoryFeedback> heard = played(providers);
  EXPECT_FALSE(heard.empty());
  EXPECT_TRUE(std::ranges::any_of(heard, [](const SensoryFeedback& each) {
    return each.kind() == SensoryFeedback::Kind::press ||
           each.kind() == SensoryFeedback::Kind::release ||
           each.selection_variant().has_value();
  }));
  EXPECT_EQ(runtime::FeedbackPlayer::gap(SensoryFeedback::Kind::press),
            std::chrono::milliseconds{40});
  EXPECT_EQ(runtime::FeedbackPlayer::gap(SensoryFeedback::Kind::release),
            std::chrono::milliseconds{40});
}

TEST_F(SensoryFeedbackTest, ClosureFormChoosesTheFeedbackForEachChange) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context& context) {
    const auto count = context.state(count_key);
    seen.count = count;
    return components::VStack(
               counter(count, "up"),
               components::Button("down",
                                  [count] { count.set(count.get() - 1); })
                   .id("down"))
        .sensory_feedback(count.get(),
                          [](const int& before,
                             const int& now) -> std::optional<SensoryFeedback> {
                            if (now == 2) {
                              return std::nullopt;
                            }
                            return now > before ? SensoryFeedback::increase
                                                : SensoryFeedback::decrease;
                          });
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_TRUE(played(providers).empty());
  bump(providers, application, "up");
  bump(providers, application, "down");
  bump(providers, application, "up");
  bump(providers, application, "up");
  bump(providers, application, "up");
  EXPECT_EQ(probe.count->get(), 3);
  EXPECT_EQ(
      played(providers),
      (std::vector{SensoryFeedback::increase, SensoryFeedback::decrease,
                   SensoryFeedback::increase, SensoryFeedback::increase}));
}

TEST_F(SensoryFeedbackTest, ConditionDecidesWhetherAChangePlays) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context& context) {
    const auto count = context.state(count_key);
    seen.count = count;
    return components::VStack(
               counter(count, "up"),
               components::Button("down",
                                  [count] { count.set(count.get() - 1); })
                   .id("down"))
        .sensory_feedback(
            SensoryFeedback::success, count.get(),
            [](const int& before, const int& now) { return now > before; });
  };
  Application application = start(providers, Host{&probe, &build});
  bump(providers, application, "up");
  bump(providers, application, "down");
  bump(providers, application, "up");
  EXPECT_EQ(probe.count->get(), 1);
  EXPECT_EQ(played(providers),
            (std::vector{SensoryFeedback::success, SensoryFeedback::success}));
}

TEST_F(SensoryFeedbackTest, DisabledEnvironmentSilencesItsSubtree) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context& context) {
    const auto count = context.state(count_key);
    const auto other = context.state(other_key);
    const auto flag = context.state(flag_key);
    seen.count = count;
    seen.other = other;
    seen.flag = flag;
    return components::VStack(
        components::VStack(
            counter(count, "quiet"),
            controls::Toggle("Quiet toggle", flag).id("quiet-toggle"))
            .sensory_feedback(SensoryFeedback::warning, count.get())
            .sensory_feedback_disabled(),
        components::VStack(counter(other, "loud"))
            .sensory_feedback(SensoryFeedback::error, other.get()));
  };
  Application application = start(providers, Host{&probe, &build});
  bump(providers, application, "quiet");
  bump(providers, application, "quiet-toggle");
  EXPECT_EQ(probe.count->get(), 1);
  EXPECT_TRUE(probe.flag->get());
  EXPECT_TRUE(played(providers).empty());
  bump(providers, application, "loud");
  EXPECT_EQ(played(providers), std::vector{SensoryFeedback::error});
}

TEST_F(SensoryFeedbackTest, TheSystemSettingSilencesOnlyTheDefaults) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context& context) {
    const auto count = context.state(count_key);
    const auto flag = context.state(flag_key);
    seen.count = count;
    seen.flag = flag;
    return components::VStack(controls::Toggle("Toggle", flag).id("toggle"),
                              counter(count, "bump"))
        .sensory_feedback(SensoryFeedback::success, count.get());
  };
  appearance::AppearancePolicy policy;
  policy.defaults.sensory_feedback = false;
  auto created = Application::create(
      providers, Host{&probe, &build},
      ApplicationOptions{
          .executors = std::nullopt,
          .fonts = {},
          .atlas = {},
          .interaction = {},
          .appearance = AppearanceOptions{.policy = std::move(policy),
                                          .storage = std::nullopt}});
  ASSERT_TRUE(created.has_value());
  Application application = std::move(*created);
  frames(application, 2);
  bump(providers, application, "toggle");
  EXPECT_TRUE(probe.flag->get());
  EXPECT_TRUE(played(providers).empty());
  bump(providers, application, "bump");
  EXPECT_EQ(played(providers), std::vector{SensoryFeedback::success});
}

TEST_F(SensoryFeedbackTest, AFocusMovePlaysOneSelection) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers, components::VStack(button("a"), button("b"), button("c")));
  EXPECT_TRUE(played(providers).empty());
  press(providers, application, Button::dpad_down);
  EXPECT_EQ(focused_key(application), "b");
  EXPECT_EQ(played(providers), std::vector{SensoryFeedback::selection});
  rest(providers, application);
  press(providers, application, Button::dpad_down);
  EXPECT_EQ(played(providers), (std::vector{SensoryFeedback::selection,
                                            SensoryFeedback::selection}));
}

TEST_F(SensoryFeedbackTest, AFocusBumpAtTheEdgePlaysASoftImpact) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers, components::VStack(button("a"), button("b")));
  press(providers, application, Button::dpad_up);
  EXPECT_EQ(focused_key(application), "a");
  EXPECT_EQ(
      played(providers),
      std::vector{SensoryFeedback::impact(SensoryFeedback::Flexibility::soft)});
}

TEST_F(SensoryFeedbackTest, EachSliderStepPlaysOneSelection) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context& context) {
    const auto level = context.state(level_key);
    seen.level = level;
    return components::VStack(
        controls::Slider(level,
                         controls::ClosedRange<double>{.lower_bound = 0.0,
                                                       .upper_bound = 1.0},
                         0.1)
            .frame({.width = 224.0F})
            .id("slider"));
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_EQ(focused_key(application), "slider");
  press(providers, application, Button::dpad_right);
  EXPECT_NEAR(probe.level->get(), 0.6, 1e-9);
  EXPECT_EQ(played(providers), std::vector{SensoryFeedback::selection});
  rest(providers, application);
  press(providers, application, Button::dpad_left);
  rest(providers, application);
  press(providers, application, Button::dpad_left);
  EXPECT_NEAR(probe.level->get(), 0.4, 1e-9);
  EXPECT_EQ(count_of(providers, SensoryFeedback::selection), 3U);
  EXPECT_EQ(played(providers).size(), 3U);
}

TEST_F(SensoryFeedbackTest, AToggleFlipPlaysALightImpact) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context& context) {
    const auto flag = context.state(flag_key);
    seen.flag = flag;
    return components::VStack(
        controls::Toggle("Wi-Fi", flag).frame({.width = 300.0F}).id("toggle"));
  };
  Application application = start(providers, Host{&probe, &build});
  press(providers, application, Button::a);
  EXPECT_TRUE(probe.flag->get());
  EXPECT_EQ(played(providers), std::vector{light()});
  rest(providers, application);
  press(providers, application, Button::a);
  EXPECT_FALSE(probe.flag->get());
  EXPECT_EQ(played(providers), (std::vector{light(), light()}));
}

TEST_F(SensoryFeedbackTest, StepperStepsPlayIncreaseAndDecrease) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context& context) {
    const auto amount = context.state(amount_key);
    seen.count = amount;
    return components::VStack(
        controls::Stepper(
            "Quantity", amount,
            controls::ClosedRange<int>{.lower_bound = 0, .upper_bound = 6})
            .frame({.width = 300.0F}));
  };
  Application application = start(providers, Host{&probe, &build});
  press(providers, application, Button::dpad_right);
  rest(providers, application);
  press(providers, application, Button::a);
  rest(providers, application);
  EXPECT_EQ(probe.count->get(), 4);
  press(providers, application, Button::dpad_left);
  rest(providers, application);
  press(providers, application, Button::a);
  EXPECT_EQ(probe.count->get(), 3);
  EXPECT_EQ(
      played(providers),
      (std::vector{SensoryFeedback::selection, SensoryFeedback::increase,
                   SensoryFeedback::selection, SensoryFeedback::decrease}));
}

TEST_F(SensoryFeedbackTest, APickerChangedByTheDpadPlaysOneSelection) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context& context) {
    const auto shade = context.state(shade_key);
    seen.count = shade;
    return components::VStack(
        controls::Picker("Shade", shade, [] { return shades(); })
            .picker_style(controls::PickerStyle::segmented())
            .frame({.width = 300.0F}));
  };
  Application application = start(providers, Host{&probe, &build});
  press(providers, application, Button::dpad_right);
  EXPECT_EQ(probe.count->get(), 2);
  EXPECT_EQ(played(providers), std::vector{SensoryFeedback::selection});
  rest(providers, application);
  press(providers, application, Button::dpad_right);
  EXPECT_EQ(probe.count->get(), 2);
  EXPECT_EQ(played(providers).size(), 1U);
}

TEST_F(SensoryFeedbackTest, AFocusStateModifierDoesNotDoubleAFocusMove) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe&, composition::Context& context) {
    const auto cell = context.state(cell_key);
    std::vector<Element> cells;
    for (int index = 0; index < 3; ++index) {
      cells.push_back(
          button(std::format("cell-{}", index)).focused(cell, index));
    }
    return components::VStack(std::move(cells))
        .sensory_feedback(SensoryFeedback::selection, cell.get());
  };
  Application application = start(providers, Host{&probe, &build});
  frames(application, 3);
  rest(providers, application);
  const std::size_t before = played(providers).size();
  press(providers, application, Button::dpad_down);
  frames(application, 3);
  EXPECT_EQ(focused_key(application), "cell-1");
  EXPECT_EQ(played(providers).size(), before + 1U);
}

TEST_F(SensoryFeedbackTest, ALongPressCommandPlaysAMediumImpact) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context& context) {
    const auto count = context.state(count_key);
    seen.count = count;
    return components::VStack(button("a"))
        .on_command(
            Button::x, "Hold", [count] { count.set(count.get() + 1); },
            composition::CommandOptions{
                .trigger = composition::CommandTrigger::long_press,
                .hidden = false});
  };
  Application application = start(providers, Host{&probe, &build});
  hold(providers, application, pad({Button::x}),
       std::chrono::milliseconds{800});
  EXPECT_EQ(probe.count->get(), 1);
  EXPECT_EQ(
      played(providers),
      std::vector{SensoryFeedback::impact(SensoryFeedback::Weight::medium)});
}

TEST_F(SensoryFeedbackTest, HoldingTheDpadIsRateLimited) {
  provider::headless::HeadlessProviderFactory providers;
  std::vector<Element> rows;
  constexpr int row_count = 80;
  for (int index = 0; index < row_count; ++index) {
    rows.push_back(button(std::format("row-{}", index), 200.0F, 40.0F));
  }
  Application application = start(
      providers, components::ScrollView(components::VStack(std::move(rows)))
                     .frame({.width = 400.0F, .height = 500.0F}));
  constexpr std::chrono::milliseconds held{2000};
  hold(providers, application, pad({Button::dpad_down}), held);
  const std::vector<provider::SoundRole> sounds = providers.audio().played();
  const auto moves = static_cast<std::size_t>(
      std::ranges::count(sounds, provider::SoundRole::focus_change));
  const std::size_t ticks = count_of(providers, SensoryFeedback::selection);
  const std::size_t ceiling =
      static_cast<std::size_t>(held / std::chrono::milliseconds{90}) + 1U;
  EXPECT_GT(moves, 20U);
  EXPECT_GE(ticks, 2U);
  EXPECT_LE(ticks, ceiling);
  EXPECT_LT(ticks, moves);
  EXPECT_EQ(played(providers).size(), ticks);
  RecordProperty("moves", static_cast<int>(moves));
  RecordProperty("ticks", static_cast<int>(ticks));
  std::println("held {} ms: {} focus moves, {} selection ticks, ceiling {}",
               held.count(), moves, ticks, ceiling);
}

TEST_F(SensoryFeedbackTest, EveryFeedbackPlaysAtTheChosenStrength) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context& context) {
    const auto count = context.state(count_key);
    const auto flag = context.state(flag_key);
    seen.count = count;
    seen.flag = flag;
    return components::VStack(controls::Toggle("Toggle", flag).id("toggle"),
                              counter(count, "bump"))
        .sensory_feedback(SensoryFeedback::success, count.get());
  };
  appearance::AppearancePolicy policy;
  policy.defaults.haptic_strength = provider::HapticStrength::from_percent(150);
  Application application =
      configured(providers, Host{&probe, &build}, std::move(policy));
  bump(providers, application, "toggle");
  bump(providers, application, "bump");
  EXPECT_EQ(played(providers),
            (std::vector{light(), SensoryFeedback::success}));
  EXPECT_EQ(providers.haptics().strengths(),
            (std::vector{provider::HapticStrength::from_percent(150),
                         provider::HapticStrength::from_percent(150)}));
  application.appearance()->update([](appearance::AppearanceOverrides& o) {
    o.haptic_strength = provider::HapticStrength::from_percent(0);
  });
  frames(application, 1);
  bump(providers, application, "toggle");
  bump(providers, application, "bump");
  EXPECT_EQ(probe.count->get(), 2);
  EXPECT_EQ(played(providers).size(), 2U);
}

TEST_F(SensoryFeedbackTest,
       TheStrengthSliderPlaysOneSelectionPerStepAtTheNewStrength) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      configured(providers,
                 components::VStack(structure::HapticStrengthSlider{})
                     .frame({.width = 320.0F}),
                 appearance::AppearancePolicy{});
  EXPECT_EQ(focused_key(application),
            structure::HapticStrengthSlider::slider_key);
  EXPECT_TRUE(played(providers).empty());
  press(providers, application, Button::dpad_right);
  EXPECT_EQ(strength_of(application),
            provider::HapticStrength::from_percent(105));
  EXPECT_EQ(played(providers), std::vector{SensoryFeedback::selection});
  rest(providers, application);
  press(providers, application, Button::dpad_left);
  rest(providers, application);
  press(providers, application, Button::dpad_left);
  rest(providers, application);
  EXPECT_EQ(strength_of(application),
            provider::HapticStrength::from_percent(95));
  EXPECT_EQ(count_of(providers, SensoryFeedback::selection), 3U);
  EXPECT_EQ(played(providers).size(), 3U);
  EXPECT_EQ(providers.haptics().strengths(),
            (std::vector{provider::HapticStrength::from_percent(105),
                         provider::HapticStrength::from_percent(100),
                         provider::HapticStrength::from_percent(95)}));
}

TEST_F(SensoryFeedbackTest, TheStrengthSliderStartsQuietlyAtTheStoredStrength) {
  provider::headless::HeadlessProviderFactory providers;
  appearance::AppearancePolicy policy;
  policy.defaults.haptic_strength = provider::HapticStrength::from_percent(40);
  Application application =
      configured(providers,
                 components::VStack(structure::HapticStrengthSlider{})
                     .frame({.width = 320.0F}),
                 std::move(policy));
  frames(application, 3);
  EXPECT_TRUE(played(providers).empty());
  press(providers, application, Button::dpad_right);
  EXPECT_EQ(strength_of(application),
            provider::HapticStrength::from_percent(45));
  EXPECT_EQ(providers.haptics().strengths(),
            std::vector{provider::HapticStrength::from_percent(45)});
}

TEST_F(SensoryFeedbackTest, TheStrengthSliderIsDisabledWhileFeedbackIsOff) {
  provider::headless::HeadlessProviderFactory providers;
  appearance::AppearancePolicy policy;
  policy.defaults.sensory_feedback = false;
  Application application =
      configured(providers,
                 components::VStack(structure::HapticStrengthSlider{})
                     .frame({.width = 320.0F}),
                 std::move(policy));
  EXPECT_NE(focused_key(application),
            structure::HapticStrengthSlider::slider_key);
  press(providers, application, Button::dpad_right);
  rest(providers, application);
  EXPECT_EQ(strength_of(application), provider::HapticStrength{});
  EXPECT_TRUE(played(providers).empty());
  application.appearance()->update(
      [](appearance::AppearanceOverrides& o) { o.sensory_feedback = true; });
  frames(application, 3);
  EXPECT_EQ(focused_key(application),
            structure::HapticStrengthSlider::slider_key);
  press(providers, application, Button::dpad_right);
  EXPECT_EQ(strength_of(application),
            provider::HapticStrength::from_percent(105));
  EXPECT_EQ(played(providers), std::vector{SensoryFeedback::selection});
}

}
