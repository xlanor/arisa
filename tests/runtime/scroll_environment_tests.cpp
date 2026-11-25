#include <arisa/components/button.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/shape_view.hpp>
#include <arisa/components/stack_options.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/alignment.hpp>
#include <arisa/composition/axes.hpp>
#include <arisa/composition/bar_placement.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/content_margin_guide.hpp>
#include <arisa/composition/content_margin_placement.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/decoration.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/geometry_proxy.hpp>
#include <arisa/composition/scroll_bounce_behavior.hpp>
#include <arisa/composition/scroll_dismisses_keyboard_mode.hpp>
#include <arisa/composition/scroll_edge_effect_style.hpp>
#include <arisa/composition/scroll_environment.hpp>
#include <arisa/composition/scroll_indicator_visibility.hpp>
#include <arisa/composition/scroll_input_behavior.hpp>
#include <arisa/composition/scroll_input_kind.hpp>
#include <arisa/controls/text_field.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/structure/scaffold.hpp>
#include <arisa/theme/space.hpp>

#include <scroll_fixture.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <format>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace arisa::runtime {

using composition::Axes;
using composition::Element;
using composition::ScrollBounceBehavior;
using composition::ScrollDismissesKeyboardMode;
using composition::ScrollIndicatorVisibility;
using composition::ScrollInputBehavior;
using composition::ScrollInputKind;

class ScrollEnvironmentTest : public ScrollFixture {
 protected:
  struct Seen {
    bool enabled{false};
    ScrollIndicatorVisibility horizontal_indicators{};
    ScrollIndicatorVisibility vertical_indicators{};
    ScrollBounceBehavior horizontal_bounce{};
    ScrollBounceBehavior vertical_bounce{};
    ScrollDismissesKeyboardMode keyboard{};
  };

  using Log = std::map<std::string, Seen>;

  class Reader final : public composition::Component {
   public:
    Reader(std::string name, std::shared_ptr<Log> log)
        : name_(std::move(name)), log_(std::move(log)) {}

    auto operator==(const Reader& other) const -> bool {
      return name_ == other.name_ && log_ == other.log_;
    }

    auto body(composition::Context& context) const -> Element override {
      using components::Environment;
      (*log_)[name_] = Seen{
          .enabled = context.environment(Environment::is_scroll_enabled),
          .horizontal_indicators = context.environment(
              Environment::horizontal_scroll_indicator_visibility),
          .vertical_indicators = context.environment(
              Environment::vertical_scroll_indicator_visibility),
          .horizontal_bounce = context.environment(
              Environment::horizontal_scroll_bounce_behavior),
          .vertical_bounce =
              context.environment(Environment::vertical_scroll_bounce_behavior),
          .keyboard =
              context.environment(Environment::scroll_dismisses_keyboard_mode)};
      return components::VStack().frame({.width = 10.0F, .height = 10.0F});
    }

   private:
    std::string name_;
    std::shared_ptr<Log> log_;
  };

  static auto rows(int count) -> std::vector<Element> {
    std::vector<Element> made;
    for (int index = 0; index < count; ++index) {
      made.push_back(components::Button(std::format("Row {}", index), [] {})
                         .frame({.width = 280.0F, .height = 50.0F})
                         .id(std::format("b{}", index)));
    }
    return made;
  }

  static auto scroller(Element content) -> Element {
    return components::ScrollView(std::move(content))
        .frame({.width = 300.0F, .height = 400.0F})
        .id("scroll");
  }

  static auto list(int count) -> Element {
    return components::VStack(components::VStackOptions{.spacing = 0.0F},
                              rows(count));
  }

  static void pull(provider::headless::HeadlessProviderFactory& providers,
                   Application& application, float from_y, float to_y) {
    const composition::Rect viewport = frame_of(application, "scroll");
    drag(providers, application,
         provider::PixelPoint{.x = viewport.x + 150.0F,
                              .y = viewport.y + from_y},
         provider::PixelPoint{.x = viewport.x + 150.0F, .y = viewport.y + to_y},
         6);
    settle(providers, application);
  }

  static auto editing(std::optional<ScrollDismissesKeyboardMode> mode)
      -> Element {
    std::vector<Element> made;
    made.push_back(controls::TextField(
                       "Name", composition::Binding<std::string>::constant(""))
                       .frame({.width = 280.0F})
                       .id("name"));
    for (Element& row : rows(30)) {
      made.push_back(std::move(row));
    }
    Element view = scroller(components::VStack(
        components::VStackOptions{.spacing = 0.0F}, std::move(made)));
    if (mode.has_value()) {
      view = std::move(view).scroll_dismisses_keyboard(*mode);
    }
    return view;
  }

  static auto wheel_over(provider::headless::HeadlessProviderFactory& providers,
                         Application& application) -> float {
    const composition::Rect viewport = frame_of(application, "scroll");
    const provider::PixelPoint over{.x = viewport.x + 150.0F,
                                    .y = viewport.y + 200.0F};
    provider::InputFrame pointed;
    pointed.pointer = provider::PointerState{.position = over, .left = false};
    providers.input().push(pointed);
    frames(application, 2);
    provider::InputFrame wheel = pointed;
    wheel.pointer->wheel_y = -3.0F;
    providers.input().push(wheel);
    settle(providers, application);
    return offset(application, "scroll").y;
  }

  static auto shaped(const composition::Node& node) -> bool {
    if (node.content_as<components::ShapeView::Content>().has_value()) {
      return true;
    }
    return std::ranges::any_of(node.children(),
                               [](const auto& child) { return shaped(child); });
  }

  static auto backing(Application& application) -> int {
    const auto header =
        node_named(application, structure::Scaffold::header_key);
    if (!header.has_value()) {
      return -1;
    }
    const composition::Tree& tree = application.root().tree();
    std::optional<composition::NodeId> current = header->get().parent();
    std::optional<std::reference_wrapper<const composition::Node>> node;
    for (int depth = 0; depth < 2 && current.has_value(); ++depth) {
      node = tree.node(*current);
      if (!node.has_value()) {
        return -1;
      }
      current = node->get().parent();
    }
    if (!node.has_value() || node->get().children().empty()) {
      return -1;
    }
    for (const composition::Node& first : node->get().children()) {
      if (!first.content_as<composition::Decoration>().has_value()) {
        return 0;
      }
      return shaped(first) ? 1 : 2;
    }
    return -1;
  }

  static auto barred(Element content) -> Element {
    return structure::Scaffold(
        structure::ScaffoldOptions{
            .shows_status = true,
            .shows_hints = true,
            .header = composition::BarPlacement::overlay,
            .footer = composition::BarPlacement::overlay},
        std::move(content).navigation_title("Scroll"));
  }
};

TEST_F(ScrollEnvironmentTest, ScrollModifiersWriteTheEnvironmentLikeSwiftUI) {
  provider::headless::HeadlessProviderFactory providers;
  auto log = std::make_shared<Log>();
  const auto read = [&log](std::string name) {
    return Element{Reader{std::move(name), log}};
  };
  Application application = start(
      providers,
      components::VStack(
          read("plain"), read("disabled").scroll_disabled(true),
          read("reenabled").scroll_disabled(false).scroll_disabled(true),
          read("indicators")
              .scroll_indicators(ScrollIndicatorVisibility::hidden,
                                 Axes::horizontal),
          read("indicators_all")
              .scroll_indicators(ScrollIndicatorVisibility::never),
          read("bounce").scroll_bounce_behavior(
              ScrollBounceBehavior::based_on_size),
          read("bounce_both")
              .scroll_bounce_behavior(ScrollBounceBehavior::always, Axes::all),
          read("keyboard")
              .scroll_dismisses_keyboard(
                  ScrollDismissesKeyboardMode::interactively),
          components::ScrollView(read("in_scroll"))
              .frame({.width = 50.0F, .height = 20.0F}),
          components::ScrollView(read("in_disabled_scroll"))
              .scroll_disabled(true)
              .frame({.width = 50.0F, .height = 20.0F}),
          components::ScrollView(read("in_hidden_scroll"))
              .scroll_indicators(ScrollIndicatorVisibility::hidden)
              .frame({.width = 50.0F, .height = 20.0F})));
  ASSERT_EQ(log->size(), 11U);
  const Seen& plain = log->at("plain");
  EXPECT_TRUE(plain.enabled);
  EXPECT_EQ(plain.horizontal_indicators, ScrollIndicatorVisibility::automatic);
  EXPECT_EQ(plain.vertical_indicators, ScrollIndicatorVisibility::automatic);
  EXPECT_EQ(plain.horizontal_bounce, ScrollBounceBehavior::automatic);
  EXPECT_EQ(plain.vertical_bounce, ScrollBounceBehavior::automatic);
  EXPECT_EQ(plain.keyboard, ScrollDismissesKeyboardMode::automatic);
  EXPECT_FALSE(log->at("disabled").enabled);
  EXPECT_FALSE(log->at("reenabled").enabled);
  EXPECT_EQ(log->at("indicators").horizontal_indicators,
            ScrollIndicatorVisibility::hidden);
  EXPECT_EQ(log->at("indicators").vertical_indicators,
            ScrollIndicatorVisibility::automatic);
  EXPECT_EQ(log->at("indicators_all").horizontal_indicators,
            ScrollIndicatorVisibility::never);
  EXPECT_EQ(log->at("indicators_all").vertical_indicators,
            ScrollIndicatorVisibility::never);
  EXPECT_EQ(log->at("bounce").horizontal_bounce,
            ScrollBounceBehavior::automatic);
  EXPECT_EQ(log->at("bounce").vertical_bounce,
            ScrollBounceBehavior::based_on_size);
  EXPECT_EQ(log->at("bounce_both").horizontal_bounce,
            ScrollBounceBehavior::always);
  EXPECT_EQ(log->at("bounce_both").vertical_bounce,
            ScrollBounceBehavior::always);
  EXPECT_EQ(log->at("keyboard").keyboard,
            ScrollDismissesKeyboardMode::interactively);
  EXPECT_TRUE(log->at("in_scroll").enabled);
  EXPECT_TRUE(log->at("in_disabled_scroll").enabled);
  EXPECT_EQ(log->at("in_hidden_scroll").horizontal_indicators,
            ScrollIndicatorVisibility::hidden);
  EXPECT_EQ(log->at("in_hidden_scroll").vertical_indicators,
            ScrollIndicatorVisibility::automatic);
}

TEST_F(ScrollEnvironmentTest, AnInnerScrollEnabledCannotUndoAnOuterDisable) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers, components::VStack(scroller(list(30)).scroll_disabled(false))
                     .scroll_disabled(true));
  pull(providers, application, 300.0F, 100.0F);
  EXPECT_FLOAT_EQ(offset(application, "scroll").y, 0.0F);
}

TEST_F(ScrollEnvironmentTest, AScrollViewConsumesScrollDisabledForItsContent) {
  provider::headless::HeadlessProviderFactory providers;
  std::vector<Element> wide;
  for (int index = 0; index < 10; ++index) {
    wide.push_back(components::VStack()
                       .frame({.width = 100.0F, .height = 40.0F})
                       .id(std::format("w{}", index)));
  }
  Application application =
      start(providers,
            components::ScrollView(
                components::VStack(
                    components::ScrollView(Axes::horizontal,
                                           components::HStack(std::move(wide)))
                        .frame({.width = 300.0F, .height = 40.0F})
                        .id("inner"),
                    list(30)))
                .scroll_disabled(true)
                .frame({.width = 300.0F, .height = 400.0F})
                .id("outer"));
  const interaction::ScrollCoordinator& scrolls =
      application.interaction().scrolls();
  const auto outer = node_of(application, "outer");
  const auto inner = node_of(application, "inner");
  ASSERT_TRUE(outer.has_value());
  ASSERT_TRUE(inner.has_value());
  EXPECT_FALSE(scrolls.scrollable(*outer).has_value());
  EXPECT_TRUE(scrolls.scrollable(*inner).has_value());
}

TEST_F(ScrollEnvironmentTest, ScrollingDismissesTheKeyboardImmediately) {
  for (const std::optional<ScrollDismissesKeyboardMode> mode :
       {std::optional<ScrollDismissesKeyboardMode>{},
        std::optional{ScrollDismissesKeyboardMode::immediately}}) {
    provider::headless::HeadlessProviderFactory providers;
    Application application = start(providers, editing(mode));
    press(providers, application, provider::Button::a);
    frames(application, 2);
    ASSERT_EQ(providers.text_input().requests().size(), 1U);
    ASSERT_EQ(providers.text_input().live_sessions(), 1U);
    pull(providers, application, 350.0F, 150.0F);
    EXPECT_EQ(providers.text_input().live_sessions(), 0U);
    EXPECT_EQ(providers.text_input().cancelled_sessions(), 1U);
  }
}

TEST_F(ScrollEnvironmentTest, NeverKeepsTheKeyboardWhileScrolling) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers, editing(ScrollDismissesKeyboardMode::never));
  press(providers, application, provider::Button::a);
  frames(application, 2);
  ASSERT_EQ(providers.text_input().live_sessions(), 1U);
  pull(providers, application, 350.0F, 150.0F);
  EXPECT_GT(offset(application, "scroll").y, 0.0F);
  EXPECT_EQ(providers.text_input().live_sessions(), 1U);
}

TEST_F(ScrollEnvironmentTest, InteractivelyDismissesOnlyWhenDraggedDown) {
  provider::headless::HeadlessProviderFactory providers;
  Application application =
      start(providers, editing(ScrollDismissesKeyboardMode::interactively));
  press(providers, application, provider::Button::a);
  frames(application, 2);
  ASSERT_EQ(providers.text_input().live_sessions(), 1U);
  pull(providers, application, 350.0F, 150.0F);
  EXPECT_EQ(providers.text_input().live_sessions(), 1U);
  pull(providers, application, 150.0F, 300.0F);
  EXPECT_EQ(providers.text_input().live_sessions(), 0U);
  EXPECT_EQ(providers.text_input().cancelled_sessions(), 1U);
}

TEST_F(ScrollEnvironmentTest, DisabledInputKindsDoNotScroll) {
  provider::headless::HeadlessProviderFactory providers;
  Application touch_off = start(
      providers, scroller(list(30)).scroll_input_behavior(
                     ScrollInputBehavior::disabled, ScrollInputKind::touch));
  pull(providers, touch_off, 300.0F, 100.0F);
  EXPECT_FLOAT_EQ(offset(touch_off, "scroll").y, 0.0F);
  EXPECT_GT(wheel_over(providers, touch_off), 10.0F);

  provider::headless::HeadlessProviderFactory pointing;
  Application pointer_off = start(
      pointing, scroller(list(30)).scroll_input_behavior(
                    ScrollInputBehavior::disabled, ScrollInputKind::pointer));
  EXPECT_FLOAT_EQ(wheel_over(pointing, pointer_off), 0.0F);
  pull(pointing, pointer_off, 300.0F, 100.0F);
  EXPECT_GT(offset(pointer_off, "scroll").y, 10.0F);

  provider::headless::HeadlessProviderFactory padded;
  Application controller_off = start(
      padded, scroller(list(30)).scroll_input_behavior(
                  ScrollInputBehavior::disabled, ScrollInputKind::controller));
  press(padded, controller_off, provider::Button::zr);
  settle(padded, controller_off);
  EXPECT_FLOAT_EQ(offset(controller_off, "scroll").y, 0.0F);

  provider::headless::HeadlessProviderFactory enabled;
  Application controller_on =
      start(enabled, scroller(list(30))
                         .scroll_input_behavior(ScrollInputBehavior::disabled,
                                                ScrollInputKind::controller)
                         .scroll_input_behavior(ScrollInputBehavior::enabled,
                                                ScrollInputKind::controller));
  press(enabled, controller_on, provider::Button::zr);
  settle(enabled, controller_on);
  EXPECT_GT(offset(controller_on, "scroll").y, 10.0F);
}

TEST_F(ScrollEnvironmentTest, MarginPlacementChoosesContentOrIndicators) {
  const auto measured = [](composition::ContentMarginPlacement placement) {
    provider::headless::HeadlessProviderFactory providers;
    Application application =
        start(providers,
              scroller(list(30))
                  .content_margins(theme::Edge::vertical, 20.0F, placement)
                  .content_margins(theme::Edge::trailing, 12.0F, placement));
    const auto node = node_named(application, "scroll");
    if (!node.has_value()) {
      return std::pair<theme::Edges, bool>{};
    }
    return std::pair{
        node->get().content_insets(),
        node->get().interactions().scrolling.indicator_margins.has_value()};
  };
  const auto [automatic, automatic_indicators] =
      measured(composition::ContentMarginPlacement::automatic);
  EXPECT_FLOAT_EQ(automatic.top, 20.0F);
  EXPECT_FLOAT_EQ(automatic.bottom, 20.0F);
  EXPECT_FLOAT_EQ(automatic.trailing, 12.0F);
  EXPECT_TRUE(automatic_indicators);
  const auto [content, content_indicators] =
      measured(composition::ContentMarginPlacement::scroll_content);
  EXPECT_FLOAT_EQ(content.top, 20.0F);
  EXPECT_FLOAT_EQ(content.trailing, 12.0F);
  EXPECT_FALSE(content_indicators);
  const auto [indicators, indicators_only] =
      measured(composition::ContentMarginPlacement::scroll_indicators);
  EXPECT_FLOAT_EQ(indicators.top, 0.0F);
  EXPECT_FLOAT_EQ(indicators.bottom, 0.0F);
  EXPECT_FLOAT_EQ(indicators.trailing, 0.0F);
  EXPECT_TRUE(indicators_only);
}

TEST_F(ScrollEnvironmentTest, AContentMarginGuideInsetsAndAlignsTheView) {
  provider::headless::HeadlessProviderFactory providers;
  Application application = start(
      providers,
      components::VStack(
          components::VStackOptions{.spacing = 0.0F},
          components::ZStack(
              components::ZStack()
                  .frame({.height = 20.0F, .max_width = composition::infinity})
                  .id("filled"))
              .content_margins(composition::ContentMarginGuide::container,
                               theme::Edge::horizontal),
          components::ZStack(components::ZStack()
                                 .frame({.width = 40.0F, .height = 20.0F})
                                 .id("placed"))
              .content_margins(composition::ContentMarginGuide::container,
                               theme::Edge::horizontal,
                               composition::Alignment::trailing))
          .frame({.width = 300.0F})
          .id("column"));
  const composition::Rect column = frame_of(application, "column");
  const composition::Rect filled = frame_of(application, "filled");
  const composition::Rect placed = frame_of(application, "placed");
  EXPECT_FLOAT_EQ(filled.x - column.x, 20.0F);
  EXPECT_FLOAT_EQ(filled.width, 260.0F);
  EXPECT_FLOAT_EQ(placed.x - column.x, 240.0F);
  const theme::Edges read = composition::GeometryProxy{}.content_margins(
      composition::ContentMarginGuide::container, theme::Edge::leading);
  EXPECT_FLOAT_EQ(read.leading, 20.0F);
  EXPECT_FLOAT_EQ(read.trailing, 0.0F);
  EXPECT_FLOAT_EQ(read.top, 0.0F);
  EXPECT_FLOAT_EQ(composition::ContentMarginGuide::container.insets().trailing,
                  20.0F);
}

TEST_F(ScrollEnvironmentTest, TheEdgeEffectStyleReachesTheBarAboveTheContent) {
  const auto backed =
      [](std::optional<composition::ScrollEdgeEffectStyle> style, bool hidden) {
        provider::headless::HeadlessProviderFactory providers;
        Element content{components::ScrollView(list(30))};
        if (style.has_value()) {
          content = std::move(content).scroll_edge_effect_style(
              *style, theme::Edge::top);
        }
        if (hidden) {
          content = std::move(content).scroll_edge_effect_hidden(
              true, theme::Edge::top);
        }
        Application application = start(providers, barred(std::move(content)));
        return backing(application);
      };
  EXPECT_EQ(backed(std::nullopt, false), 1);
  EXPECT_EQ(backed(composition::ScrollEdgeEffectStyle::soft, false), 1);
  EXPECT_EQ(backed(composition::ScrollEdgeEffectStyle::hard, false), 2);
  EXPECT_EQ(backed(std::nullopt, true), 0);
}

}
