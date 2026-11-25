#include <arisa/components/button.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/focus_interactions.hpp>
#include <arisa/composition/focused_value_key.hpp>
#include <arisa/composition/namespace.hpp>
#include <arisa/composition/navigation_split_view_visibility.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/composition/reset_focus_action.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/structure/navigation_split_view.hpp>
#include <arisa/structure/tab.hpp>
#include <arisa/structure/tab_view.hpp>
#include <arisa/structure/tab_view_style.hpp>

#include <scroll_fixture.hpp>

#include <gtest/gtest.h>

#include <format>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace arisa::runtime {

using composition::Element;

class FocusExtrasTest : public ScrollFixture {
 protected:
  static inline const composition::FocusedValueKey<std::string> selection{
      "selection"};
  static inline const composition::FocusedValueKey<int> layer{"layer"};

  class Reader final : public composition::Component {
   public:
    auto operator==(const Reader&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const std::optional<std::string> chosen =
          context.focused_value(selection);
      const std::optional<int> scene = context.focused_value(layer);
      return components::Text(
                 std::format(
                     "chosen {} scene {}", chosen.value_or("none"),
                     scene.has_value() ? std::to_string(*scene) : "none"))
          .id("reader");
    }
  };

  class Reloading final : public composition::Component {
   public:
    static constexpr composition::StateKey<int> loads{"loads", 0};
    static constexpr composition::Namespace space{"reloading"};

    auto operator==(const Reloading&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto count = context.state(loads);
      const composition::Namespace::ID scope = context.namespace_id(space);
      const composition::ResetFocusAction refocus =
          context.environment(components::Environment::reset_focus);
      return components::HStack(
          components::VStack(button(std::format("first-{}", count.get()))
                                 .prefers_default_focus(scope),
                             button(std::format("second-{}", count.get())),
                             button(std::format("third-{}", count.get())))
              .focus_scope(scope),
          components::Button("reload",
                             [count, refocus, scope] {
                               count.set(count.get() + 1);
                               refocus(scope);
                             })
              .frame({.width = 100.0F, .height = 50.0F})
              .id("reload"));
    }
  };
};

TEST_F(FocusExtrasTest, FocusedValuesFollowFocusAndSceneValuesStay) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers,
            components::VStack(
                button("alpha").focused_value(selection, std::string{"alpha"}),
                button("beta").focused_value(selection, std::string{"beta"}),
                button("plain"), Element{Reader{}})
                .focused_scene_value(layer, 3));
  frames(application, 2);
  EXPECT_TRUE(shows(application, "chosen alpha scene 3"));
  press(providers, application, provider::Button::dpad_down);
  frames(application, 2);
  EXPECT_TRUE(shows(application, "chosen beta scene 3"));
  press(providers, application, provider::Button::dpad_down);
  frames(application, 2);
  EXPECT_TRUE(shows(application, "chosen none scene 3"));
}

TEST_F(FocusExtrasTest, FocusedObjectIsFoundThroughTheFocusedChain) {
  provider::headless::HeadlessProviderFactory providers;
  composition::Observable<int> model{7};
  std::optional<int> seen;
  class Peek final : public composition::Component {
   public:
    explicit Peek(std::optional<int>* seen) noexcept : seen_(seen) {}

    auto operator==(const Peek&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto object = context.focused_object<int>();
      *seen_ = object.has_value() ? std::optional<int>{object->get().get()}
                                  : std::nullopt;
      return components::Text("peek");
    }

   private:
    std::optional<int>* seen_;
  };
  Application application =
      start(providers, components::VStack(button("owner").focused_object(model),
                                          Element{Peek{&seen}}));
  frames(application, 3);
  EXPECT_EQ(seen, std::optional<int>{7});
}

TEST_F(FocusExtrasTest, FocusedSceneObjectStaysWhateverIsFocused) {
  provider::headless::HeadlessProviderFactory providers;
  composition::Observable<int> model{11};
  auto seen = std::make_shared<std::vector<std::optional<int>>>();
  class Peek final : public composition::Component {
   public:
    explicit Peek(std::shared_ptr<std::vector<std::optional<int>>> seen)
        : seen_(std::move(seen)) {}

    auto operator==(const Peek&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      const auto object = context.focused_object<int>();
      seen_->push_back(object.has_value()
                           ? std::optional<int>{object->get().get()}
                           : std::nullopt);
      return components::Text("peek");
    }

   private:
    std::shared_ptr<std::vector<std::optional<int>>> seen_;
  };
  Application application = start(
      providers,
      components::VStack(button("one"), button("two"), Element{Peek{seen}})
          .focused_scene_object(model));
  frames(application, 3);
  ASSERT_FALSE(seen->empty());
  EXPECT_EQ(seen->back(), std::optional<int>{11});
  press(providers, application, provider::Button::dpad_down);
  frames(application, 3);
  EXPECT_EQ(seen->back(), std::optional<int>{11});
}

TEST_F(FocusExtrasTest, ResetFocusReturnsToTheScopesDefault) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(providers, Reloading{});
  EXPECT_EQ(focused_key(application), "first-0");
  press(providers, application, provider::Button::dpad_down);
  press(providers, application, provider::Button::dpad_down);
  EXPECT_EQ(focused_key(application), "third-0");
  ASSERT_TRUE(application.root().activate(composition::ElementKey{"reload"}));
  frames(application, 4);
  EXPECT_EQ(focused_key(application), "first-1");
}

TEST_F(FocusExtrasTest, ActivateInteractionsKeepFocusWhereItWas) {
  provider::headless::HeadlessProviderFactory providers;
  int taps = 0;
  Application application =
      start(providers,
            components::VStack(
                button("list"),
                components::Button("tool", [&taps] { ++taps; })
                    .frame({.width = 100.0F, .height = 50.0F})
                    .focusable(true, composition::FocusInteractions::activate)
                    .id("tool")));
  EXPECT_EQ(focused_key(application), "list");
  tap(providers, application, centre(application, "tool"));
  EXPECT_EQ(taps, 1);
  EXPECT_EQ(focused_key(application), "list");
}

TEST_F(FocusExtrasTest, EditInteractionsTakeFocusOnTap) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers, components::VStack(
                     button("list"),
                     components::ZStack()
                         .frame({.width = 200.0F, .height = 80.0F})
                         .focusable(true, composition::FocusInteractions::edit)
                         .id("field")));
  EXPECT_EQ(focused_key(application), "list");
  tap(providers, application, centre(application, "field"));
  EXPECT_EQ(focused_key(application), "field");
}

TEST_F(FocusExtrasTest, ANarrowControlDirectlyBelowBeatsTheRememberedColumn) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers,
      components::VStack(
          components::VStackOptions{
              .alignment = components::HorizontalAlignment::leading,
              .spacing = 8.0F},
          components::HStack(components::HStackOptions{.spacing = 400.0F},
                             button("left"), button("right")),
          button("wide", 600.0F), button("narrow", 80.0F),
          components::ZStack().frame({.height = 300.0F}),
          components::HStack(components::HStackOptions{.spacing = 0.0F},
                             components::ZStack().frame({.width = 400.0F}),
                             button("far"))));
  press(providers, application, provider::Button::dpad_right);
  EXPECT_EQ(focused_key(application), "right");
  press(providers, application, provider::Button::dpad_down);
  EXPECT_EQ(focused_key(application), "wide");
  press(providers, application, provider::Button::dpad_down);
  EXPECT_EQ(focused_key(application), "narrow");
}

TEST_F(FocusExtrasTest, TheDPadScrollsAPageWithNothingFocusable) {
  provider::headless::HeadlessProviderFactory providers;
  std::vector<Element> lines;
  for (int index = 0; index < 40; ++index) {
    lines.push_back(components::Text(std::format("line {}", index))
                        .frame({.height = 40.0F}));
  }
  Application application =
      start(providers,
            components::VStack(
                button("header"),
                components::ScrollView(components::VStack(std::move(lines)))
                    .frame({.width = 400.0F, .height = 300.0F})
                    .id("page")));
  EXPECT_EQ(focused_key(application), "header");
  press(providers, application, provider::Button::dpad_down);
  EXPECT_EQ(focused_key(application), "page");
  press(providers, application, provider::Button::dpad_down);
  settle(providers, application);
  EXPECT_GT(offset(application, "page").y, 50.0F);
  press(providers, application, provider::Button::dpad_up);
  settle(providers, application);
  press(providers, application, provider::Button::dpad_up);
  settle(providers, application);
  press(providers, application, provider::Button::dpad_up);
  settle(providers, application);
  EXPECT_FLOAT_EQ(offset(application, "page").y, 0.0F);
  EXPECT_EQ(focused_key(application), "header");
}

TEST_F(FocusExtrasTest, ASplitViewWithNothingFocusableStillTakesMinus) {
  provider::headless::HeadlessProviderFactory providers;
  composition::Observable<composition::NavigationSplitViewVisibility> shown{
      composition::NavigationSplitViewVisibility::all};
  class Splitter final : public composition::Component {
   public:
    explicit Splitter(
        composition::Observable<composition::NavigationSplitViewVisibility>*
            model) noexcept
        : model_(model) {}

    auto operator==(const Splitter&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      static_cast<void>(context.read(*model_));
      auto* model = model_;
      return structure::NavigationSplitView(
          composition::Binding<composition::NavigationSplitViewVisibility>{
              [model] { return model->get(); },
              [model](composition::NavigationSplitViewVisibility value) {
                model->set(value);
              }},
          components::Text("sidebar"), components::Text("detail"));
    }

   private:
    composition::Observable<composition::NavigationSplitViewVisibility>* model_;
  };
  Application application = start(providers, Splitter{&shown});
  press(providers, application, structure::NavigationSplitView::toggle_button);
  frames(application, 3);
  EXPECT_NE(shown.get(), composition::NavigationSplitViewVisibility::all);
}

}
