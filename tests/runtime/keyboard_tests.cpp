#include <arisa/components/button.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/event_modifiers.hpp>
#include <arisa/composition/key_equivalent.hpp>
#include <arisa/composition/key_press.hpp>
#include <arisa/composition/keyboard_shortcut.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/provider/headless/headless_factory.hpp>

#include <focus_fixture.hpp>

#include <gtest/gtest.h>

#include <format>
#include <memory>
#include <string>
#include <vector>

namespace arisa::runtime {

using composition::Element;
using composition::KeyEquivalent;
using composition::KeyPress;

class KeyboardTest : public FocusFixture {
 protected:
  struct Log {
    std::vector<std::string> pressed;
    int activations{0};
    int deleted{0};
    int bubbled{0};
  };

  static void key(provider::headless::HeadlessProviderFactory& providers,
                  Application& application, provider::Key which, bool down) {
    provider::InputFrame frame;
    frame.keys.push_back(provider::KeyEvent{.key = which, .down = down});
    providers.input().push(frame);
    frames(application, 2);
  }

  static void type(provider::headless::HeadlessProviderFactory& providers,
                   Application& application, provider::Key which) {
    key(providers, application, which, true);
    key(providers, application, which, false);
  }

  class Pager final : public composition::Component {
   public:
    static constexpr composition::StateKey<int> page{"page", 2};

    auto operator==(const Pager&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto shown = context.state(page);
      return components::VStack(button("first"), components::Text(std::format(
                                                     "page {}", shown.get())))
          .page_command(composition::Binding<int>{shown}, 1, 3);
    }
  };
};

TEST_F(KeyboardTest, KeyPressReachesTheFocusedViewThenItsAncestors) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application =
      start(providers,
            components::VStack(button("target").on_key_press(
                                   'k', KeyPress::Phases::down,
                                   [log](const KeyPress& press) {
                                     log->pressed.push_back(press.characters);
                                     return KeyPress::Result::ignored;
                                   }))
                .on_key_press(KeyPress::Phases::down, [log](const KeyPress&) {
                  ++log->bubbled;
                  return KeyPress::Result::handled;
                }));
  type(providers, application, provider::Key::k);
  ASSERT_EQ(log->pressed.size(), 1U);
  EXPECT_EQ(log->pressed.front(), "k");
  EXPECT_EQ(log->bubbled, 1);
}

TEST_F(KeyboardTest, HandledKeysDoNotNavigateOrActivate) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application = start(
      providers, components::VStack(
                     components::Button("target", [log] { ++log->activations; })
                         .frame({.width = 100.0F, .height = 50.0F})
                         .id("target")
                         .on_key_press(KeyEquivalent::return_key,
                                       [log] {
                                         log->pressed.emplace_back("return");
                                         return KeyPress::Result::handled;
                                       }),
                     button("below")));
  type(providers, application, provider::Key::enter);
  EXPECT_EQ(log->activations, 0);
  EXPECT_EQ(log->pressed.size(), 1U);
  EXPECT_EQ(focused_key(application), "target");
}

TEST_F(KeyboardTest, ShiftUppercasesTheCharacters) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application = start(
      providers, components::VStack(button("target").on_key_press(
                     KeyPress::Phases::down, [log](const KeyPress& press) {
                       log->pressed.push_back(press.characters);
                       return KeyPress::Result::handled;
                     })));
  key(providers, application, provider::Key::left_shift, true);
  type(providers, application, provider::Key::q);
  key(providers, application, provider::Key::left_shift, false);
  ASSERT_EQ(log->pressed.size(), 1U);
  EXPECT_EQ(log->pressed.front(), "Q");
}

TEST_F(KeyboardTest, KeyboardShortcutActivatesAButtonAnywhere) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application = start(
      providers, components::VStack(
                     button("first"),
                     components::Button("save", [log] { ++log->activations; })
                         .frame({.width = 100.0F, .height = 50.0F})
                         .id("save")
                         .keyboard_shortcut('s')));
  type(providers, application, provider::Key::s);
  EXPECT_EQ(log->activations, 0);
  key(providers, application, provider::Key::left_super, true);
  type(providers, application, provider::Key::s);
  key(providers, application, provider::Key::left_super, false);
  EXPECT_EQ(log->activations, 1);
  EXPECT_EQ(focused_key(application), "first");
}

TEST_F(KeyboardTest, ShiftedPunctuationShortcutsNeedShift) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application = start(
      providers, components::VStack(
                     button("first"),
                     components::Button("zoom", [log] { ++log->activations; })
                         .frame({.width = 100.0F, .height = 50.0F})
                         .keyboard_shortcut('+'),
                     components::Button("help", [log] { log->deleted += 1; })
                         .frame({.width = 100.0F, .height = 50.0F})
                         .keyboard_shortcut('?')));
  key(providers, application, provider::Key::left_super, true);
  type(providers, application, provider::Key::equals);
  type(providers, application, provider::Key::space);
  EXPECT_EQ(log->activations, 0);
  key(providers, application, provider::Key::left_shift, true);
  type(providers, application, provider::Key::equals);
  type(providers, application, provider::Key::slash);
  key(providers, application, provider::Key::left_shift, false);
  key(providers, application, provider::Key::left_super, false);
  EXPECT_EQ(log->activations, 1);
  EXPECT_EQ(log->deleted, 1);
}

TEST_F(KeyboardTest, KeyEquivalentsNeverSubstituteAnotherKey) {
  EXPECT_EQ(KeyEquivalent{'+'}.key(), provider::Key::equals);
  EXPECT_TRUE(KeyEquivalent{'+'}.shifted());
  EXPECT_EQ(KeyEquivalent{'?'}.unshifted(), KeyEquivalent{'/'});
  EXPECT_EQ(KeyEquivalent{U'\u00E9'}.key(), std::nullopt);
  EXPECT_NE(KeyEquivalent{U'\u00E9'}, KeyEquivalent::space);
  EXPECT_EQ(KeyEquivalent{U'\u00E9'}.character(), U'\u00E9');
  EXPECT_EQ(KeyEquivalent::up_arrow.key(), provider::Key::up);
  EXPECT_EQ(KeyEquivalent{provider::Key::page_down}, KeyEquivalent::page_down);
  EXPECT_EQ(KeyEquivalent::pressed(provider::Key::digit1, true),
            KeyEquivalent{'!'});
  EXPECT_EQ(KeyEquivalent::pressed(provider::Key::a, true), KeyEquivalent{'a'});
}

TEST_F(KeyboardTest, DefaultAndCancelActionsUseReturnAndEscape) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application = start(
      providers,
      components::HStack(
          components::Button("other", [] {})
              .frame({.width = 100.0F, .height = 50.0F})
              .id("other")
              .focusable(false),
          components::Button("ok", [log] { log->activations += 1; })
              .frame({.width = 100.0F, .height = 50.0F})
              .keyboard_shortcut(composition::KeyboardShortcut::default_action),
          components::Button("cancel", [log] { log->activations += 10; })
              .frame({.width = 100.0F, .height = 50.0F})
              .keyboard_shortcut(composition::KeyboardShortcut::cancel_action)
              .focusable(false)));
  type(providers, application, provider::Key::escape);
  EXPECT_EQ(log->activations, 10);
}

TEST_F(KeyboardTest, DeleteCommandRunsFromTheKeyAndMinus) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  Application application =
      start(providers, components::VStack(button("row").on_delete_command(
                           [log] { ++log->deleted; })));
  type(providers, application, provider::Key::delete_forward);
  EXPECT_EQ(log->deleted, 1);
  type(providers, application, provider::Key::backspace);
  EXPECT_EQ(log->deleted, 2);
  press(providers, application, provider::Button::minus);
  EXPECT_EQ(log->deleted, 3);
  const auto& hints = application.interaction().hints();
  EXPECT_TRUE(
      std::ranges::any_of(hints, [](const composition::CommandHint& hint) {
        return hint.button == provider::Button::minus && hint.label == "Delete";
      }));
}

TEST_F(KeyboardTest, PageCommandStepsWithinTheBounds) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, Pager{});
  press(providers, application, provider::Button::zr);
  EXPECT_TRUE(shows(application, "page 3"));
  press(providers, application, provider::Button::zr);
  EXPECT_TRUE(shows(application, "page 3"));
  type(providers, application, provider::Key::page_up);
  type(providers, application, provider::Key::page_up);
  EXPECT_TRUE(shows(application, "page 1"));
  press(providers, application, provider::Button::zl);
  EXPECT_TRUE(shows(application, "page 1"));
}

}
