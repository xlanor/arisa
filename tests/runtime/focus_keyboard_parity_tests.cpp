#include <arisa/components/button.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/focus_state.hpp>
#include <arisa/composition/focused_value_key.hpp>
#include <arisa/composition/keyboard_environment.hpp>
#include <arisa/composition/search_environment.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/controls/searchable.hpp>
#include <arisa/provider/headless/headless_factory.hpp>

#include <focus_fixture.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace arisa::runtime {

class Rows {
 public:
  static auto button(std::string key, float width = 100.0F,
                     float height = 50.0F) -> composition::Element {
    return components::Button(key, [] {})
        .frame({.width = width, .height = height})
        .id(std::move(key));
  }
};

using composition::Element;
using composition::EventModifiers;

class FocusKeyboardParityTest : public FocusFixture {
 protected:
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
};

class PriorityProbe final : public composition::Component {
 public:
  static constexpr composition::FocusState<int> chosen{"parity-priority"};

  explicit PriorityProbe(composition::DefaultFocusEvaluationPriority priority)
      : priority_(priority) {}

  auto operator==(const PriorityProbe&) const -> bool = default;

  auto body(composition::Context& context) const -> Element override {
    const auto focus = context.state(chosen);
    return components::HStack(
        Rows::button("outside"),
        components::VStack(Rows::button("one").focused(focus, 1),
                           Rows::button("two").focused(focus, 2),
                           Rows::button("three").focused(focus, 3))
            .focus_section()
            .default_focus(focus, 3, priority_));
  }

 private:
  composition::DefaultFocusEvaluationPriority priority_;
};

TEST_F(FocusKeyboardParityTest, UserInitiatedDefaultFocusBeatsMemory) {
  for (const auto priority :
       {composition::DefaultFocusEvaluationPriority::automatic,
        composition::DefaultFocusEvaluationPriority::user_initiated}) {
    provider::headless::HeadlessProviderFactory providers;
    Application application = start(providers, PriorityProbe{priority});
    press(providers, application, provider::Button::dpad_left);
    EXPECT_EQ(focused_key(application), "outside");
    press(providers, application, provider::Button::dpad_right);
    EXPECT_EQ(focused_key(application), "three");
    press(providers, application, provider::Button::dpad_up);
    EXPECT_EQ(focused_key(application), "two");
    press(providers, application, provider::Button::dpad_left);
    press(providers, application, provider::Button::dpad_right);
    EXPECT_EQ(focused_key(application),
              priority == composition::DefaultFocusEvaluationPriority::automatic
                  ? "two"
                  : "three");
  }
}

class FocusedBindingProbe final : public composition::Component {
 public:
  static constexpr composition::StateKey<int> counted{"parity-counted", 1};
  static const composition::FocusedValueKey<composition::Binding<int>> editing;

  auto operator==(const FocusedBindingProbe&) const -> bool = default;

  auto body(composition::Context& context) const -> Element override {
    const auto count = context.state(counted);
    const composition::FocusedBinding<int> focused =
        context.focused_binding(editing);
    const composition::FocusedValues values = context.focused_values();
    return components::VStack(
        Rows::button("editor").focused_value(editing,
                                             composition::Binding<int>{count}),
        components::Button("bump",
                           [focused] {
                             focused.set(focused.wrapped_value().value_or(0) +
                                         10);
                           })
            .frame({.width = 100.0F, .height = 50.0F})
            .id("bump"),
        components::Text(std::to_string(count.get())),
        components::Text(values[editing].has_value() ? "bound" : "unbound"));
  }
};

const composition::FocusedValueKey<composition::Binding<int>>
    FocusedBindingProbe::editing{"parity-editing"};

TEST_F(FocusKeyboardParityTest, FocusedBindingWritesTheFocusedValue) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, FocusedBindingProbe{});
  EXPECT_EQ(focused_key(application), "editor");
  frames(application, 2);
  EXPECT_TRUE(shows(application, "bound"));
  press(providers, application, provider::Button::dpad_down);
  EXPECT_EQ(focused_key(application), "bump");
  frames(application, 2);
  EXPECT_TRUE(shows(application, "unbound"));
}

class SearchProbe final : public composition::Component {
 public:
  enum class Field : std::uint8_t {
    search,
    other,
  };

  static constexpr composition::FocusState<Field> field{"parity-search-field"};
  static constexpr composition::StateKey<std::string> query{"parity-query", ""};

  auto operator==(const SearchProbe&) const -> bool = default;

  auto body(composition::Context& context) const -> Element override {
    const auto focus = context.state(field);
    const auto text = context.state(query);
    return components::VStack(
               components::Button("seek", [focus] { focus.set(Field::search); })
                   .frame({.width = 100.0F, .height = 50.0F})
                   .id("seek"),
               components::Text(focus.get() == Field::search ? "searching"
                                                             : "idle"))
        .searchable(composition::Binding<std::string>{text})
        .search_focused(focus, Field::search);
  }
};

TEST_F(FocusKeyboardParityTest, SearchFocusedBindsAnEnumValue) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, SearchProbe{});
  EXPECT_TRUE(shows(application, "searching"));
  press(providers, application, provider::Button::dpad_down);
  frames(application, 2);
  EXPECT_TRUE(shows(application, "idle"));
  tap(providers, application, centre(application, "seek"));
  frames(application, 3);
  EXPECT_TRUE(shows(application, "searching"));
}

class ShortcutProbe final : public composition::Component {
 public:
  static constexpr composition::StateKey<std::string> fired{"parity-fired",
                                                            "none"};

  auto operator==(const ShortcutProbe&) const -> bool = default;

  auto body(composition::Context& context) const -> Element override {
    const auto last = context.state(fired);
    const std::optional<composition::KeyboardShortcut>& read =
        context.environment(composition::KeyboardEnvironment::shortcut);
    return components::VStack(
               components::Button("back", [last] { last.set("back"); })
                   .keyboard_shortcut(
                       composition::KeyEquivalent::left_arrow,
                       EventModifiers::command,
                       composition::KeyboardShortcut::Localization::automatic),
               components::Button("fixed", [last] { last.set("fixed"); })
                   .keyboard_shortcut(
                       '[', EventModifiers::command,
                       composition::KeyboardShortcut::Localization::custom),
               components::Text(last.get()),
               components::Text(read.has_value() ? "outer" : "plain"))
        .environment(components::Environment::layout_direction,
                     components::LayoutDirection::right_to_left);
  }
};

TEST_F(FocusKeyboardParityTest, ShortcutsMirrorInRightToLeftUnlessCustom) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, ShortcutProbe{});
  key(providers, application, provider::Key::left_super, true);
  type(providers, application, provider::Key::right);
  key(providers, application, provider::Key::left_super, false);
  frames(application, 2);
  EXPECT_TRUE(shows(application, "back"));
  key(providers, application, provider::Key::left_super, true);
  type(providers, application, provider::Key::left_bracket);
  key(providers, application, provider::Key::left_super, false);
  frames(application, 2);
  EXPECT_TRUE(shows(application, "fixed"));
  EXPECT_TRUE(shows(application, "plain"));
}

class ShortcutReader final : public composition::Component {
 public:
  auto operator==(const ShortcutReader&) const -> bool = default;

  auto body(composition::Context& context) const -> Element override {
    const std::optional<composition::KeyboardShortcut>& read =
        context.environment(composition::KeyboardEnvironment::shortcut);
    return components::Text(read.has_value() && read->key == 's' ? "save"
                                                                 : "none");
  }
};

TEST_F(FocusKeyboardParityTest, KeyboardShortcutIsReadableFromTheEnvironment) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers, components::VStack(Element{ShortcutReader{}}.keyboard_shortcut(
                     's', EventModifiers::command)));
  EXPECT_TRUE(shows(application, "save"));
}

TEST_F(FocusKeyboardParityTest, ModifierKeysReportChangesAndAlternatesSwap) {
  provider::headless::HeadlessProviderFactory providers;
  auto seen = std::make_shared<std::vector<std::pair<int, int>>>();
  const auto bits = [](EventModifiers modifiers) {
    int value = 0;
    value |= modifiers.contains(EventModifiers::shift) ? 1 : 0;
    value |= modifiers.contains(EventModifiers::option) ? 2 : 0;
    return value;
  };
  Application application = start(
      providers,
      components::VStack(
          components::Text("primary").id("primary").modifier_key_alternate(
              EventModifiers::option,
              components::Text("alternate").id("alternate")),
          components::Text("watcher").on_modifier_keys_changed(
              EventModifiers::shift | EventModifiers::option, true,
              [seen, bits](EventModifiers old, EventModifiers next) {
                seen->emplace_back(bits(old), bits(next));
              })));
  frames(application, 2);
  ASSERT_FALSE(seen->empty());
  EXPECT_EQ(seen->front(), (std::pair{0, 0}));
  const auto live = [&application](std::string_view key) {
    const auto node = node_named(application, key);
    const auto parent = node.has_value() ? node->get().parent() : std::nullopt;
    const auto wrapper = parent.has_value()
                             ? application.root().tree().node(*parent)
                             : std::nullopt;
    return wrapper.has_value() && wrapper->get().interactions().hit_testing;
  };
  EXPECT_TRUE(live("primary"));
  EXPECT_FALSE(live("alternate"));
  key(providers, application, provider::Key::left_alt, true);
  frames(application, 2);
  EXPECT_EQ(seen->back(), (std::pair{0, 2}));
  EXPECT_TRUE(live("alternate"));
  EXPECT_FALSE(live("primary"));
  key(providers, application, provider::Key::left_super, true);
  const std::size_t before = seen->size();
  key(providers, application, provider::Key::left_super, false);
  EXPECT_EQ(seen->size(), before);
  key(providers, application, provider::Key::left_alt, false);
  frames(application, 2);
  EXPECT_EQ(seen->back(), (std::pair{2, 0}));
  EXPECT_TRUE(live("primary"));
}

TEST_F(FocusKeyboardParityTest, TypeSelectJumpsToTheMatchingRow) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers, components::VStack(
                     button("apple").type_select_equivalent("Apple"),
                     button("banana").type_select_equivalent("Banana"),
                     button("blueberry").type_select_equivalent("Blueberry"),
                     button("cherry").type_select_equivalent(std::nullopt)));
  EXPECT_EQ(focused_key(application), "apple");
  type(providers, application, provider::Key::b);
  EXPECT_EQ(focused_key(application), "banana");
  type(providers, application, provider::Key::l);
  EXPECT_EQ(focused_key(application), "blueberry");
  type(providers, application, provider::Key::c);
  EXPECT_EQ(focused_key(application), "blueberry");
}

}
