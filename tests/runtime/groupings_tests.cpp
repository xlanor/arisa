#include <arisa/components/button.hpp>
#include <arisa/components/button_border_shape.hpp>
#include <arisa/components/button_repeat_behavior.hpp>
#include <arisa/components/button_sizing.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/content_unavailable_view.hpp>
#include <arisa/components/control_size.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/label.hpp>
#include <arisa/components/labeled_content.hpp>
#include <arisa/components/primitive_button_style.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/command_options.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/control_environment.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/list_item_tint.hpp>
#include <arisa/composition/node.hpp>
#include <arisa/composition/pasteboard.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/composition/tint.hpp>
#include <arisa/composition/transfer_item.hpp>
#include <arisa/controls/calendar_month.hpp>
#include <arisa/controls/closed_range.hpp>
#include <arisa/controls/color_picker.hpp>
#include <arisa/controls/date_format.hpp>
#include <arisa/controls/date_picker.hpp>
#include <arisa/controls/edit_button.hpp>
#include <arisa/controls/gauge.hpp>
#include <arisa/controls/menu.hpp>
#include <arisa/controls/multi_date_picker.hpp>
#include <arisa/controls/paste_button.hpp>
#include <arisa/controls/picker.hpp>
#include <arisa/controls/picker_style.hpp>
#include <arisa/controls/rename_button.hpp>
#include <arisa/controls/stepper.hpp>
#include <arisa/controls/text_editor.hpp>
#include <arisa/controls/toggle.hpp>
#include <arisa/controls/toggle_style.hpp>
#include <arisa/graphics/gradient.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/structure/control_group.hpp>
#include <arisa/structure/disclosure_group.hpp>
#include <arisa/structure/form.hpp>
#include <arisa/structure/form_style.hpp>
#include <arisa/structure/group_box.hpp>
#include <arisa/structure/list.hpp>
#include <arisa/structure/section.hpp>
#include <arisa/structure/tab.hpp>
#include <arisa/structure/tab_view.hpp>
#include <arisa/structure/tab_view_customization.hpp>
#include <arisa/structure/tab_view_style.hpp>
#include <arisa/text/locale.hpp>
#include <arisa/theme/color.hpp>
#include <arisa/theme/colour.hpp>

#include <focus_fixture.hpp>

#include <gtest/gtest.h>

#include <chrono>
#include <cmath>
#include <format>
#include <functional>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::runtime {

using composition::Element;
using provider::Button;

class GroupingsTest : public FocusFixture {
 protected:
  enum class Shade : std::uint8_t {
    red,
    green,
    blue,
  };

  struct Probe {
    std::optional<composition::StateHandle<bool>> flag;
    std::optional<composition::StateHandle<int>> count;
    std::optional<composition::StateHandle<Shade>> shade;
    std::optional<composition::StateHandle<std::chrono::sys_seconds>> when;
    std::optional<
        composition::StateHandle<std::set<std::chrono::year_month_day>>>
        days;
    std::optional<composition::StateHandle<theme::Colour>> colour;
    std::optional<composition::StateHandle<components::EditMode>> mode;
    std::optional<composition::StateHandle<structure::TabViewCustomization>>
        customization;
    std::vector<std::string> pasted;
    int actions{0};
    int configurations{0};
    std::vector<std::optional<graphics::ShapeStyle>> tints;
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

  class TintProbe final : public composition::Component {
   public:
    explicit TintProbe(Probe* probe) noexcept : probe_(probe) {}

    auto operator==(const TintProbe&) const -> bool = default;

    auto body(composition::Context& context) const -> Element override {
      probe_->tints.push_back(context.environment(composition::Tint::key));
      return components::Text("Tinted");
    }

   private:
    Probe* probe_;
  };

  struct HoldStyle {
    auto make_body(
        components::PrimitiveButtonStyle::Configuration configuration) const
        -> Element {
      return components::VStack(std::move(configuration.label))
          .on_long_press_gesture(configuration.trigger)
          .on_command(Button::a, "Hold", configuration.trigger,
                      composition::CommandOptions{
                          .trigger = composition::CommandTrigger::long_press,
                          .hidden = false});
    }

    friend auto operator==(const HoldStyle&, const HoldStyle&)
        -> bool = default;
  };

  struct CountingGroupBoxStyle {
    Probe* probe{nullptr};

    auto make_body(structure::GroupBoxStyle::Configuration configuration) const
        -> Element {
      ++probe->configurations;
      return components::VStack(std::move(configuration.label),
                                std::move(configuration.content))
          .id("custom-box");
    }

    friend auto operator==(const CountingGroupBoxStyle&,
                           const CountingGroupBoxStyle&) -> bool = default;
  };

  struct SwappedLabeledContentStyle {
    auto make_body(components::LabeledContentStyle::Configuration configuration)
        const -> Element {
      return components::HStack(std::move(configuration.content),
                                std::move(configuration.label))
          .id("swapped");
    }

    friend auto operator==(const SwappedLabeledContentStyle&,
                           const SwappedLabeledContentStyle&) -> bool = default;
  };

  struct StackedControlGroupStyle {
    auto make_body(structure::ControlGroupStyle::Configuration configuration)
        const -> Element {
      return components::VStack(configuration.content()).id("stacked-group");
    }

    friend auto operator==(const StackedControlGroupStyle&,
                           const StackedControlGroupStyle&) -> bool = default;
  };

  struct OpenDisclosureStyle {
    auto make_body(structure::DisclosureGroupStyle::Configuration configuration)
        const -> Element {
      return components::VStack(std::move(configuration.label),
                                configuration.content())
          .id("always-open");
    }

    friend auto operator==(const OpenDisclosureStyle&,
                           const OpenDisclosureStyle&) -> bool = default;
  };

  struct PercentGaugeStyle {
    auto make_body(controls::GaugeStyle::Configuration configuration) const
        -> Element {
      return components::Text(std::format(
          "{}%", static_cast<int>(configuration.value * 100.0 + 0.5)));
    }

    friend auto operator==(const PercentGaugeStyle&, const PercentGaugeStyle&)
        -> bool = default;
  };

  struct FramedFormStyle {
    auto make_body(structure::FormStyle::Configuration configuration) const
        -> Element {
      return components::VStack(components::Text("Framed form"),
                                std::move(configuration.content));
    }

    friend auto operator==(const FramedFormStyle&, const FramedFormStyle&)
        -> bool = default;
  };

  struct MarkedEditorStyle {
    auto make_body(controls::TextEditorStyle::Configuration configuration) const
        -> Element {
      return components::VStack(components::Text("Notes editor"),
                                std::move(configuration.content));
    }

    friend auto operator==(const MarkedEditorStyle&, const MarkedEditorStyle&)
        -> bool = default;
  };

  struct LinkMenuStyle {
    auto make_body(controls::MenuStyle::Configuration configuration) const
        -> Element {
      return components::Button([open = configuration.open] { open(); },
                                std::move(configuration.label))
          .button_style(components::ButtonStyle::plain())
          .id("link-menu");
    }

    friend auto operator==(const LinkMenuStyle&, const LinkMenuStyle&)
        -> bool = default;
  };

  static constexpr composition::StateKey<bool> flag_key{"flag", true};
  static constexpr composition::StateKey<int> count_key{"count", 0};
  static constexpr composition::StateKey<Shade> shade_key{"shade",
                                                          Shade::green};
  static constexpr std::chrono::sys_seconds start_instant{
      std::chrono::seconds{1'763'287'200}};
  static constexpr composition::StateKey<std::chrono::sys_seconds> when_key{
      "when", start_instant};
  static const composition::StateKey<std::set<std::chrono::year_month_day>>
      days_key;
  static constexpr composition::StateKey<theme::Colour> colour_key{
      "colour", theme::Colour::hex(0xFF0000)};
  static constexpr composition::StateKey<components::EditMode> mode_key{
      "mode", components::EditMode::inactive};
  static const composition::StateKey<structure::TabViewCustomization>
      customization_key;

  static auto shades() -> Element {
    return components::Group(components::Text("Red").tag(Shade::red),
                             components::Text("Green").tag(Shade::green),
                             components::Text("Blue").tag(Shade::blue));
  }

  static auto frame_of(Application& application, std::string_view key)
      -> composition::Rect {
    const auto node = node_named(application, key);
    return node.has_value() ? node->get().frame() : composition::Rect{};
  }

  static void settle(provider::headless::HeadlessProviderFactory& providers,
                     Application& application) {
    for (int step = 0; step < 40; ++step) {
      ASSERT_TRUE(providers.clock().advance(frame_step).has_value());
      frames(application, 1);
    }
  }

  static auto first_frame_of(const composition::Node& node,
                             composition::ElementType type)
      -> std::optional<composition::Rect> {
    if (node.content().type() == type) {
      return node.frame();
    }
    for (const composition::Node& child : node.children()) {
      if (const auto found = first_frame_of(child, type)) {
        return found;
      }
    }
    return std::nullopt;
  }
};

const composition::StateKey<std::set<std::chrono::year_month_day>>
    GroupingsTest::days_key{"days", {}};
const composition::StateKey<structure::TabViewCustomization>
    GroupingsTest::customization_key{"customization",
                                     structure::TabViewCustomization{}};

TEST_F(GroupingsTest, LabelsHiddenLeavesJustTheControls) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context& context) {
    const auto flag = context.state(flag_key);
    const auto count = context.state(count_key);
    const auto shade = context.state(shade_key);
    seen.flag = flag;
    return components::VStack(
               components::HStack(controls::Toggle("Wi-Fi", flag).id("toggle")),
               controls::Picker("Shade", shade, [] { return shades(); }),
               controls::Stepper("Count", count,
                                 controls::ClosedRange<int>{.lower_bound = 0,
                                                            .upper_bound = 9}),
               components::LabeledContent("Console", "Switch"))
        .labels_hidden();
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_FALSE(shows(application, "Wi-Fi"));
  EXPECT_FALSE(shows(application, "Shade"));
  EXPECT_FALSE(shows(application, "Count"));
  EXPECT_FALSE(shows(application, "Console"));
  EXPECT_TRUE(shows(application, "Switch"));
  EXPECT_FLOAT_EQ(frame_of(application, "toggle").width, 54.0F);
  EXPECT_FLOAT_EQ(frame_of(application, "toggle").height, 24.0F);
  press(providers, application, Button::a);
  EXPECT_FALSE(probe.flag->get());
}

TEST_F(GroupingsTest, APrimitiveButtonStyleDecidesWhenToTrigger) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context&) {
    Probe* target = &seen;
    return components::VStack(
        components::Button("Hold me", [target] { ++target->actions; })
            .button_style(HoldStyle{})
            .id("held"),
        components::Button("Plain", [target] {
          target->actions += 10;
        }).id("plain"));
  };
  Application application = start(providers, Host{&probe, &build});
  press(providers, application, Button::a);
  EXPECT_EQ(probe.actions, 0);
  hold(providers, application, pad({Button::a}),
       std::chrono::milliseconds{800});
  EXPECT_EQ(probe.actions, 1);
  press(providers, application, Button::dpad_down);
  EXPECT_EQ(focused_key(application), "plain");
  press(providers, application, Button::a);
  EXPECT_EQ(probe.actions, 11);
}

TEST_F(GroupingsTest, MenusHonourStyleIndicatorAndDismissBehaviour) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context&) {
    Probe* target = &seen;
    const auto entries = [target] {
      return Element{components::Group(
          components::Button("Duplicate", [target] { ++target->actions; }),
          components::Button("Delete", [target] { target->actions += 10; }))};
    };
    return components::VStack(
        components::HStack(controls::Menu("Actions", entries).id("shown")),
        components::HStack(controls::Menu("Actions", entries)
                               .menu_indicator(composition::Visibility::hidden)
                               .id("bare")),
        controls::Menu("Stays", entries)
            .menu_action_dismiss_behavior(
                composition::MenuActionDismissBehavior::disabled),
        controls::Menu("Linked", entries).menu_style(LinkMenuStyle{}));
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_GT(frame_of(application, "shown").width,
            frame_of(application, "bare").width);
  EXPECT_TRUE(node_of(application, "link-menu").has_value());
  press(providers, application, Button::dpad_down);
  press(providers, application, Button::dpad_down);
  press(providers, application, Button::a);
  settle(providers, application);
  ASSERT_TRUE(shows(application, "Duplicate"));
  press(providers, application, Button::a);
  settle(providers, application);
  EXPECT_EQ(probe.actions, 1);
  EXPECT_TRUE(shows(application, "Duplicate"));
}

TEST_F(GroupingsTest, PriorityOrderPutsTheFirstItemNearestAnUpwardLabel) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe&, composition::Context&) {
    return components::VStack(
        components::Spacer(),
        controls::Menu("Sort",
                       [] {
                         return Element{components::Group(
                             components::Button("First", [] {}).id("first"),
                             components::Button("Last", [] {}).id("last"))};
                       })
            .menu_order(composition::MenuOrder::priority)
            .environment(composition::ControlEnvironment::menu_opens_upward,
                         true));
  };
  Application application = start(providers, Host{&probe, &build});
  press(providers, application, Button::a);
  settle(providers, application);
  ASSERT_TRUE(shows(application, "First"));
  EXPECT_GT(frame_of(application, "first").y, frame_of(application, "last").y);
}

TEST_F(GroupingsTest, APanelFlippedUpwardForRoomReversesUnderPriority) {
  const auto opened = [](std::optional<composition::MenuOrder> order,
                         bool bottom) {
    provider::headless::HeadlessProviderFactory providers;
    Probe probe;
    const auto build = [order, bottom](Probe&, composition::Context&) {
      Element menu = controls::Menu("Sort", [] {
        return Element{
            components::Group(components::Button("First", [] {}).id("first"),
                              components::Button("Last", [] {}).id("last"))};
      });
      if (order.has_value()) {
        menu = std::move(menu).menu_order(*order);
      }
      return bottom ? components::VStack(components::Spacer(), std::move(menu))
                    : components::VStack(std::move(menu), components::Spacer());
    };
    const Build built = build;
    Application application = start(providers, Host{&probe, &built});
    press(providers, application, Button::a);
    settle(providers, application);
    EXPECT_TRUE(shows(application, "First"));
    return frame_of(application, "first").y - frame_of(application, "last").y;
  };
  EXPECT_GT(opened(composition::MenuOrder::priority, true), 0.0F);
  EXPECT_LT(opened(composition::MenuOrder::fixed, true), 0.0F);
  EXPECT_LT(opened(std::nullopt, true), 0.0F);
  EXPECT_LT(opened(composition::MenuOrder::priority, false), 0.0F);
}

TEST_F(GroupingsTest, EveryNewStyleProtocolReachesItsView) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context& context) {
    const auto flag = context.state(flag_key);
    return components::VStack(
        structure::GroupBox("Box", components::Text("Inside"))
            .group_box_style(CountingGroupBoxStyle{&seen}),
        components::LabeledContent("Name", "Arisa")
            .labeled_content_style(SwappedLabeledContentStyle{}),
        structure::ControlGroup([] {
          return Element{components::Group(components::Button("A", [] {}),
                                           components::Button("B", [] {}))};
        }).control_group_style(StackedControlGroupStyle{}),
        structure::DisclosureGroup(
            "More", [] { return Element{components::Text("Hidden detail")}; })
            .disclosure_group_style(OpenDisclosureStyle{}),
        controls::Gauge(0.25, components::Text("Fuel"))
            .gauge_style(PercentGaugeStyle{}),
        controls::TextEditor(composition::Binding<std::string>::constant("x"))
            .text_editor_style(MarkedEditorStyle{})
            .frame({.height = 120.0F}),
        controls::Toggle("Flag", flag));
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_GE(probe.configurations, 1);
  EXPECT_TRUE(node_of(application, "custom-box").has_value());
  EXPECT_TRUE(node_of(application, "swapped").has_value());
  EXPECT_TRUE(node_of(application, "stacked-group").has_value());
  EXPECT_TRUE(shows(application, "Hidden detail"));
  EXPECT_TRUE(shows(application, "25%"));
  EXPECT_TRUE(shows(application, "Notes editor"));
}

TEST_F(GroupingsTest, ACustomFormStyleWrapsTheFormContent) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe&, composition::Context&) {
    return structure::Form(structure::Section(components::Text("Row")))
        .form_style(FramedFormStyle{});
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_TRUE(shows(application, "Framed form"));
  EXPECT_TRUE(shows(application, "Row"));
}

TEST_F(GroupingsTest, ColumnsFormsLineLabelsUpInOneColumn) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe&, composition::Context& context) {
    const auto flag = context.state(flag_key);
    return structure::Form(
               structure::Section(
                   components::LabeledContent(
                       components::ZStack()
                           .frame({.width = 60.0F, .height = 20.0F})
                           .id("one-value"),
                       components::ZStack()
                           .frame({.width = 40.0F, .height = 20.0F})
                           .id("one-label")),
                   components::LabeledContent(
                       components::ZStack()
                           .frame({.width = 60.0F, .height = 20.0F})
                           .id("two-value"),
                       components::ZStack()
                           .frame({.width = 120.0F, .height = 20.0F})
                           .id("two-label")),
                   controls::Toggle("Online", flag).id("online")))
        .form_style(structure::FormStyle::columns())
        .frame({.width = 400.0F, .height = 300.0F});
  };
  Application application = start(providers, Host{&probe, &build});
  frames(application, 4);
  const composition::Rect one_label = frame_of(application, "one-label");
  const composition::Rect two_label = frame_of(application, "two-label");
  const composition::Rect one_value = frame_of(application, "one-value");
  const composition::Rect two_value = frame_of(application, "two-value");
  EXPECT_NEAR(one_label.x + one_label.width, two_label.x + two_label.width,
              0.5F);
  EXPECT_NEAR(one_value.x, two_value.x, 0.5F);
  EXPECT_NEAR(two_value.x - (two_label.x + two_label.width),
              components::LabelColumn::gap, 0.5F);
  EXPECT_NEAR(one_label.width, 40.0F, 0.5F);
  EXPECT_TRUE(shows(application, "Online"));
}

TEST_F(GroupingsTest, BorderShapeSizingAndControlSizeShapeButtons) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe&, composition::Context&) {
    return components::VStack(
        components::HStack(
            components::Button("Button", [] {})
                .button_style(components::ButtonStyle::bordered())
                .control_size(components::ControlSize::mini)
                .id("mini"),
            components::Button("Button", [] {})
                .button_style(components::ButtonStyle::bordered())
                .id("regular"),
            components::Button("Button", [] {})
                .button_style(components::ButtonStyle::bordered())
                .control_size(components::ControlSize::extra_large)
                .id("huge")),
        components::HStack(
            components::Button("Wide", [] {})
                .button_style(components::ButtonStyle::bordered())
                .button_sizing(components::ButtonSizing::flexible)
                .id("wide"))
            .frame({.width = 300.0F}),
        components::HStack(
            controls::Toggle("Mini", composition::Binding<bool>::constant(true))
                .labels_hidden()
                .control_size(components::ControlSize::mini)
                .id("mini-toggle")));
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_LT(frame_of(application, "mini").height,
            frame_of(application, "regular").height);
  EXPECT_LT(frame_of(application, "regular").height,
            frame_of(application, "huge").height);
  EXPECT_FLOAT_EQ(frame_of(application, "wide").width, 300.0F);
  EXPECT_FLOAT_EQ(frame_of(application, "mini-toggle").width, 36.0F);
  EXPECT_FLOAT_EQ(frame_of(application, "mini-toggle").height, 16.0F);
  EXPECT_EQ(components::ButtonBorderShape::automatic().shape(
                components::ControlSize::large),
            theme::Shape::capsule());
  EXPECT_EQ(components::ButtonBorderShape::rounded_rectangle(3.0F).shape(
                components::ControlSize::regular),
            theme::Shape::rounded(3.0F, true));
}

TEST_F(GroupingsTest, RepeatingButtonsFireWhileHeld) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context&) {
    Probe* target = &seen;
    return components::VStack(
        components::Button("More", [target] { ++target->actions; })
            .button_style(components::ButtonStyle::bordered())
            .button_repeat_behavior(components::ButtonRepeatBehavior::enabled),
        components::Button("Once", [target] {
          target->actions += 1000;
        }).id("once"));
  };
  Application application = start(providers, Host{&probe, &build});
  hold(providers, application, pad({Button::a}),
       std::chrono::milliseconds{1500});
  EXPECT_GT(probe.actions, 2);
  EXPECT_LT(probe.actions, 1000);
}

TEST_F(GroupingsTest, EditPasteAndRenameButtonsActOnTheirEnvironment) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  composition::Pasteboard board;
  board.set_items({composition::TransferItem::of(std::string{"copied"})});
  const Build build = [board](Probe& seen, composition::Context& context) {
    const auto mode = context.state(mode_key);
    seen.mode = mode;
    Probe* target = &seen;
    return components::VStack(
               Element{controls::EditButton{}}.id("edit"),
               Element{controls::PasteButton(
                           std::type_identity<std::string>{},
                           [target](std::vector<std::string> items) {
                             target->pasted = std::move(items);
                           })}
                   .id("paste"),
               Element{controls::RenameButton{}}
                   .rename_action([target] { target->actions += 1; })
                   .id("rename"),
               Element{controls::RenameButton{}}.id("unbound"))
        .button_style(components::ButtonStyle::bordered())
        .edit_mode(mode)
        .environment(components::Environment::pasteboard,
                     std::optional<composition::Pasteboard>{board});
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_TRUE(shows(application, "Edit"));
  press(providers, application, Button::a);
  EXPECT_EQ(probe.mode->get(), components::EditMode::active);
  EXPECT_TRUE(shows(application, "Done"));
  press(providers, application, Button::dpad_down);
  press(providers, application, Button::a);
  ASSERT_EQ(probe.pasted.size(), 1U);
  EXPECT_EQ(probe.pasted.front(), "copied");
  press(providers, application, Button::dpad_down);
  press(providers, application, Button::a);
  EXPECT_EQ(probe.actions, 1);
  const auto renaming = application.interaction().focused();
  press(providers, application, Button::dpad_down);
  EXPECT_EQ(application.interaction().focused(), renaming);
}

TEST_F(GroupingsTest, DateFieldsFollowTheLocaleOrder) {
  using std::chrono::November;
  const std::chrono::year_month_day day{std::chrono::year{2026} / November /
                                        std::chrono::day{16}};
  EXPECT_EQ(controls::DateFormat::date_text(day, text::Locale::undetermined()),
            "Nov 16, 2026");
  EXPECT_EQ(
      controls::DateFormat::date_text(day, *text::Locale::create("de-DE")),
      "16.11.2026");
  EXPECT_EQ(
      controls::DateFormat::date_text(day, *text::Locale::create("ja-JP")),
      "2026/11/16");
  EXPECT_EQ(controls::DateFormat::time_text(std::chrono::minutes{(13 * 60) + 5},
                                            text::Locale::undetermined()),
            "1:05\u202fPM");
  EXPECT_EQ(controls::DateFormat::time_text(std::chrono::minutes{(13 * 60) + 5},
                                            *text::Locale::create("en-GB")),
            "13:05");
  const controls::CalendarMonth month{std::chrono::year{2026} / November,
                                      std::chrono::Sunday};
  EXPECT_EQ(month.leading(), 0U);
  EXPECT_EQ(month.days(), 30U);
  EXPECT_EQ(month.rows(), 5U);
  const controls::CalendarMonth monday{std::chrono::year{2026} / November,
                                       std::chrono::Monday};
  EXPECT_EQ(monday.leading(), 6U);
  EXPECT_EQ(monday.rows(), 6U);
  EXPECT_EQ(monday.day(6),
            std::optional<std::chrono::year_month_day>{
                std::chrono::year{2026} / November / std::chrono::day{1}});
}

TEST_F(GroupingsTest, GraphicalDatePickersPickDaysAndKeepTheTime) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context& context) {
    const auto when = context.state(when_key);
    seen.when = when;
    const std::chrono::sys_seconds start = start_instant;
    return controls::DatePicker(
               "Day", when,
               controls::DatePicker::Range{
                   .lower_bound = start - std::chrono::days{3},
                   .upper_bound = start + std::chrono::days{3}},
               controls::DatePickerComponents::date |
                   controls::DatePickerComponents::hour_and_minute)
        .date_picker_style(controls::DatePickerStyle::graphical());
  };
  Application application = start(providers, Host{&probe, &build});
  const std::chrono::sys_seconds start = start_instant;
  using std::chrono::November;
  EXPECT_TRUE(shows(application, "November 2025"));
  const std::chrono::year_month_day picked{std::chrono::year{2025} / November /
                                           std::chrono::day{17}};
  tap(providers, application,
      centre(application, controls::DatePickerStyle::day_key(picked)));
  EXPECT_EQ(probe.when->get(), start + std::chrono::days{1});
  const std::chrono::year_month_day far{std::chrono::year{2025} / November /
                                        std::chrono::day{25}};
  tap(providers, application,
      centre(application, controls::DatePickerStyle::day_key(far)));
  EXPECT_EQ(probe.when->get(), start + std::chrono::days{1});
  tap(providers, application,
      centre(application, controls::DatePickerStyle::next_key));
  EXPECT_TRUE(shows(application, "December 2025"));
}

TEST_F(GroupingsTest, CompactDatePickersOpenACalendarAndTimeSteppers) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context& context) {
    const auto when = context.state(when_key);
    seen.when = when;
    return controls::DatePicker("When", when).frame({.width = 400.0F});
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_TRUE(shows(application, "Nov 16, 2025"));
  EXPECT_TRUE(shows(application, "10:00\u202fAM"));
  press(providers, application, Button::dpad_right);
  press(providers, application, Button::a);
  settle(providers, application);
  EXPECT_TRUE(shows(application, "Hour 10"));
  ASSERT_TRUE(node_of(application, "arisa.date_picker.hour").has_value());
  tap(providers, application,
      centre(application, controls::Stepper::raise_key));
  EXPECT_EQ(probe.when->get(), start_instant + std::chrono::hours{1});
}

TEST_F(GroupingsTest, MultiDatePickersToggleDays) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  using std::chrono::November;
  const std::chrono::year_month_day first{std::chrono::year{2026} / November /
                                          std::chrono::day{2}};
  const Build build = [first](Probe& seen, composition::Context& context) {
    const auto days = context.state(days_key);
    seen.days = days;
    return controls::MultiDatePicker(
        days,
        controls::MultiDatePicker::Range{.lower_bound = first,
                                         .upper_bound = first},
        components::Text("Trips"));
  };
  Application application = start(providers, Host{&probe, &build});
  tap(providers, application,
      centre(application, controls::DatePickerStyle::day_key(first)));
  EXPECT_TRUE(probe.days->get().contains(first));
  tap(providers, application,
      centre(application, controls::DatePickerStyle::day_key(first)));
  EXPECT_TRUE(probe.days->get().empty());
}

TEST_F(GroupingsTest, ColorPickersEditTheBoundColourFromAPanel) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context& context) {
    const auto colour = context.state(colour_key);
    seen.colour = colour;
    return controls::ColorPicker("Accent", colour).frame({.width = 300.0F});
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_TRUE(shows(application, "Accent"));
  press(providers, application, Button::a);
  settle(providers, application);
  ASSERT_TRUE(shows(application, "Hue"));
  press(providers, application, Button::dpad_down);
  press(providers, application, Button::dpad_right);
  settle(providers, application);
  EXPECT_NE(probe.colour->get(), theme::Colour::hex(0xFF0000));
}

TEST_F(GroupingsTest, GaugesClampAndDrawEveryStyle) {
  EXPECT_DOUBLE_EQ(controls::Gauge::fraction(
                       15.0, {.lower_bound = 0.0, .upper_bound = 10.0}),
                   1.0);
  EXPECT_DOUBLE_EQ(
      controls::Gauge::fraction(5.0, {.lower_bound = 0.0, .upper_bound = 10.0}),
      0.5);
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe&, composition::Context&) {
    return components::VStack(
        controls::Gauge(0.5, components::Text("Linear")).id("linear"),
        controls::Gauge(0.5, components::Text("Line"))
            .gauge_style(controls::GaugeStyle::accessory_linear())
            .id("accessory"),
        controls::Gauge(0.5, components::Text("Ring"))
            .gauge_style(controls::GaugeStyle::accessory_circular_capacity())
            .id("ring"),
        controls::Gauge(0.5, components::Text("Dial"))
            .gauge_style(controls::GaugeStyle::accessory_circular())
            .id("dial"));
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_FLOAT_EQ(frame_of(application, "ring").width,
                  controls::GaugeStyle::circular_extent);
  EXPECT_FLOAT_EQ(frame_of(application, "dial").height,
                  controls::GaugeStyle::circular_extent);
  EXPECT_TRUE(shows(application, "Linear"));
  EXPECT_TRUE(shows(application, "Line"));
}

TEST_F(GroupingsTest, GroupBoxesPadTheirContentUnderTheLabel) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe&, composition::Context&) {
    return components::VStack(
        structure::GroupBox(components::Text("Inside").id("inside"),
                            components::Text("Title").id("title"))
            .id("box"));
  };
  Application application = start(providers, Host{&probe, &build});
  const composition::Rect box = frame_of(application, "box");
  const composition::Rect title = frame_of(application, "title");
  const composition::Rect inside = frame_of(application, "inside");
  EXPECT_FLOAT_EQ(title.x - box.x, structure::GroupBoxStyle::padding);
  EXPECT_FLOAT_EQ(title.y - box.y, structure::GroupBoxStyle::padding);
  EXPECT_FLOAT_EQ(inside.y - (title.y + title.height),
                  structure::GroupBoxStyle::label_spacing);
}

TEST_F(GroupingsTest, ControlGroupsJoinButtonsAndCollapseIntoMenus) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context&) {
    Probe* target = &seen;
    const auto entries = [target] {
      return Element{components::Group(
          components::Button("Cut", [target] { ++target->actions; }).id("cut"),
          components::Button("Copy", [target] {
            target->actions += 10;
          }).id("copy"))};
    };
    return components::VStack(
        structure::ControlGroup(entries),
        structure::ControlGroup("Edit", entries)
            .control_group_style(structure::ControlGroupStyle::menu()));
  };
  Application application = start(providers, Host{&probe, &build});
  EXPECT_EQ(focused_key(application), "cut");
  press(providers, application, Button::dpad_right);
  EXPECT_EQ(focused_key(application), "copy");
  press(providers, application, Button::a);
  EXPECT_EQ(probe.actions, 10);
  EXPECT_TRUE(shows(application, "Edit"));
  EXPECT_FLOAT_EQ(frame_of(application, "cut").y,
                  frame_of(application, "copy").y);
}

TEST_F(GroupingsTest, WheelPickersEngageStepAndRelease) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context& context) {
    const auto shade = context.state(shade_key);
    seen.shade = shade;
    return components::VStack(
        controls::Picker("Shade", shade, [] { return shades(); })
            .picker_style(controls::PickerStyle::wheel())
            .frame({.width = 300.0F}),
        components::Button("After", [] {}).id("after"));
  };
  Application application = start(providers, Host{&probe, &build});
  press(providers, application, Button::a);
  press(providers, application, Button::dpad_down);
  EXPECT_EQ(probe.shade->get(), Shade::blue);
  press(providers, application, Button::dpad_up);
  press(providers, application, Button::dpad_up);
  EXPECT_EQ(probe.shade->get(), Shade::red);
  press(providers, application, Button::b);
  press(providers, application, Button::dpad_down);
  EXPECT_EQ(probe.shade->get(), Shade::red);
  EXPECT_EQ(focused_key(application), "after");
}

TEST_F(GroupingsTest, ListItemTintModesResolveAgainstTheEnvironment) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const theme::Colour pink = theme::Colour::hex(0xFF2D55);
  const theme::Colour blue = theme::Colour::hex(0x007AFF);
  const Build build = [pink, blue](Probe& seen, composition::Context&) {
    Probe* target = &seen;
    return structure::List(structure::Section(
                               Element{TintProbe{target}}.list_item_tint(
                                   composition::ListItemTint::preferred(pink)),
                               Element{TintProbe{target}}.list_item_tint(
                                   composition::ListItemTint::fixed(pink)),
                               Element{TintProbe{target}}.list_item_tint(
                                   composition::ListItemTint::monochrome())))
        .tint(blue);
  };
  Application application = start(providers, Host{&probe, &build});
  ASSERT_GE(probe.tints.size(), 3U);
  EXPECT_EQ(probe.tints[0], std::optional<graphics::ShapeStyle>{blue});
  EXPECT_EQ(probe.tints[1], std::optional<graphics::ShapeStyle>{pink});
  ASSERT_TRUE(probe.tints[2].has_value());
  EXPECT_NE(*probe.tints[2], graphics::ShapeStyle{blue});
  EXPECT_NE(*probe.tints[2], graphics::ShapeStyle{pink});
}

TEST_F(GroupingsTest, TintAcceptsAnyShapeStyle) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const graphics::LinearGradient gradient{
      .gradient = graphics::Gradient{std::vector<theme::ColourValue>{
          theme::ColourRole::pink, theme::ColourRole::orange}},
      .start_point = motion::UnitPoint::leading,
      .end_point = motion::UnitPoint::trailing};
  const Build build = [gradient](Probe& seen, composition::Context&) {
    Probe* target = &seen;
    return components::VStack(
        Element{TintProbe{target}}.tint(gradient),
        Element{TintProbe{target}}.tint(theme::Color::mint),
        Element{TintProbe{target}}.tint(std::nullopt));
  };
  Application application = start(providers, Host{&probe, &build});
  ASSERT_GE(probe.tints.size(), 3U);
  ASSERT_TRUE(probe.tints[0].has_value());
  EXPECT_NE(probe.tints[0]->gradient(), nullptr);
  EXPECT_EQ(probe.tints[1],
            std::optional<graphics::ShapeStyle>{theme::Color::mint});
  EXPECT_FALSE(probe.tints[2].has_value());
}

TEST_F(GroupingsTest, UntintedSwipeActionsAreGreyAndTintedOnesKeepTheirTint) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const theme::Colour orange = theme::Colour::hex(0xFF9500);
  const Build build = [orange](Probe& seen, composition::Context&) {
    Probe* target = &seen;
    return structure::List(structure::Section(
        components::Text("Row").swipe_actions([target, orange] {
          return Element{components::Group(
              components::Button([] {}, Element{TintProbe{target}}),
              components::Button([] {}, Element{TintProbe{target}})
                  .tint(orange))};
        })));
  };
  Application application = start(providers, Host{&probe, &build});
  press(providers, application, Button::y);
  settle(providers, application);
  ASSERT_GE(probe.tints.size(), 2U);
  const std::optional<graphics::ShapeStyle> untinted =
      probe.tints[probe.tints.size() - 2];
  const std::optional<graphics::ShapeStyle> tinted = probe.tints.back();
  ASSERT_TRUE(untinted.has_value() && untinted->colour().has_value() &&
              untinted->colour()->literal() != nullptr);
  const theme::Colour grey = *untinted->colour()->literal();
  EXPECT_LT(std::abs(grey.red - grey.blue), 0.1F);
  EXPECT_EQ(tinted, std::optional<graphics::ShapeStyle>{orange});
}

TEST_F(GroupingsTest, ContentUnavailableLabelsStackTheirIconAboveTheTitle) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe&, composition::Context&) {
    return components::ContentUnavailableView(
        components::Label(components::Text("No mail").id("title"),
                          components::Image::system_name("mail").id("icon")),
        std::nullopt, std::nullopt);
  };
  Application application = start(providers, Host{&probe, &build});
  const composition::Rect icon = frame_of(application, "icon");
  const composition::Rect title = frame_of(application, "title");
  EXPECT_LT(icon.y + icon.height, title.y + 0.5F);
}

TEST_F(GroupingsTest, TheSidebarEditButtonHidesAndShowsTabs) {
  provider::headless::HeadlessProviderFactory providers;
  Probe probe;
  const Build build = [](Probe& seen, composition::Context& context) {
    const auto saved = context.state(customization_key);
    seen.customization = saved;
    const auto page = [](std::string text) {
      return structure::Tab::Builder{[text] { return components::Text(text); }};
    };
    return Element{
        structure::TabView(structure::Tab("One", "circle", 0, page("one page"))
                               .customization_id("one"),
                           structure::Tab("Two", "circle", 1, page("two page"))
                               .customization_id("two"))
            .tab_view_customization(saved)}
        .tab_view_style(structure::SidebarAdaptableTabViewStyle{});
  };
  Application application = start(providers, Host{&probe, &build});
  frames(application, 3);
  ASSERT_TRUE(node_of(application, "arisa.tab.sidebar-edit").has_value());
  press(providers, application, Button::dpad_down);
  press(providers, application, Button::dpad_down);
  press(providers, application, Button::a);
  settle(providers, application);
  ASSERT_TRUE(node_of(application, "arisa.tab.visibility.two").has_value());
  press(providers, application, Button::dpad_up);
  press(providers, application, Button::a);
  EXPECT_EQ(probe.customization->get().sidebar_visibility("two"),
            composition::Visibility::hidden);
  press(providers, application, Button::a);
  EXPECT_EQ(probe.customization->get().sidebar_visibility("two"),
            composition::Visibility::visible);
}

}
