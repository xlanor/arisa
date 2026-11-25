#include <groupings_page.hpp>

#include <arisa/components/button.hpp>
#include <arisa/components/button_border_shape.hpp>
#include <arisa/components/button_repeat_behavior.hpp>
#include <arisa/components/button_sizing.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/control_size.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/for_each.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/index_set.hpp>
#include <arisa/components/label.hpp>
#include <arisa/components/labeled_content.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/toolbar_item.hpp>
#include <arisa/controls/closed_range.hpp>
#include <arisa/controls/color_picker.hpp>
#include <arisa/controls/date_picker.hpp>
#include <arisa/controls/edit_button.hpp>
#include <arisa/controls/gauge.hpp>
#include <arisa/controls/menu.hpp>
#include <arisa/controls/multi_date_picker.hpp>
#include <arisa/controls/paste_button.hpp>
#include <arisa/controls/picker.hpp>
#include <arisa/controls/picker_style.hpp>
#include <arisa/controls/rename_button.hpp>
#include <arisa/controls/slider.hpp>
#include <arisa/controls/stepper.hpp>
#include <arisa/controls/text_field.hpp>
#include <arisa/controls/toggle.hpp>
#include <arisa/presentation/navigation_stack.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/presentation/presented_destination.hpp>
#include <arisa/structure/control_group.hpp>
#include <arisa/structure/form.hpp>
#include <arisa/structure/form_style.hpp>
#include <arisa/structure/group_box.hpp>
#include <arisa/structure/list.hpp>
#include <arisa/structure/section.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <array>
#include <format>
#include <string>
#include <utility>
#include <vector>

namespace arisa::examples {

using composition::Element;
using composition::infinity;
using composition::ToolbarItem;
using composition::ToolbarItemPlacement;
using theme::ColourRole;

const composition::StateKey<std::set<std::chrono::year_month_day>>
    GroupingsPage::days{"groupings-days", {}};

const composition::StateKey<std::vector<std::string>> GroupingsPage::notes{
    "groupings-notes",
    {"Pack the dock", "Charge the controllers", "Update the console",
     "Back up saves"}};

const composition::StateKey<std::string> GroupingsPage::name{"groupings-name",
                                                             "Arisa"};

auto GroupingsPage::HoldStyle::make_body(
    components::PrimitiveButtonStyle::Configuration configuration) const
    -> Element {
  return components::VStack(std::move(configuration.label))
      .padding(components::Button::label_insets)
      .background(ColourRole::system_fill,
                  theme::Shape::rounded(theme::radius::control, true))
      .on_long_press_gesture(configuration.trigger)
      .on_command(provider::Button::a, "Hold", configuration.trigger,
                  composition::CommandOptions{
                      .trigger = composition::CommandTrigger::long_press,
                      .hidden = false});
}

auto GroupingsPage::title_of(Demo chosen) -> std::string_view {
  switch (chosen) {
    case Demo::dates:
      return "Dates";
    case Demo::colour:
      return "Colour";
    case Demo::gauges:
      return "Gauges";
    case Demo::sizes:
      return "Control sizes";
    case Demo::group_box:
      return "Group box";
    case Demo::editing:
      return "Edit and paste";
    case Demo::menus:
      return "Menus and groups";
    case Demo::wheel:
      return "Wheel";
  }
  return "Dates";
}

auto GroupingsPage::tile_key(Demo chosen) -> std::string {
  return std::format("groupings-demo-{}", static_cast<int>(chosen));
}

auto GroupingsPage::flavours() -> Element {
  return components::Group(
      components::Text("Vanilla").tag(Flavour::vanilla),
      components::Text("Chocolate").tag(Flavour::chocolate),
      components::Text("Strawberry").tag(Flavour::strawberry),
      components::Text("Mint").tag(Flavour::mint),
      components::Text("Caramel").tag(Flavour::caramel));
}

auto GroupingsPage::tile(Demo each, std::string_view symbol,
                         composition::StateHandle<std::optional<Demo>> chosen)
    -> Element {
  return components::Button(
             [chosen, each] { chosen.set(each); },
             components::VStack(
                 components::VStackOptions{
                     .alignment = components::HorizontalAlignment::leading,
                     .spacing = theme::space::sm},
                 components::Image::system_name(std::string{symbol})
                     .font(theme::Font::title2),
                 components::Text(std::string{title_of(each)})
                     .font(theme::Font::headline))
                 .frame({.width = 180.0F,
                         .height = 96.0F,
                         .alignment = composition::Alignment::top_leading}))
      .id(tile_key(each));
}

auto GroupingsPage::tiles(composition::StateHandle<std::optional<Demo>> chosen)
    -> Element {
  return components::ScrollView(
             components::VStack(
                 components::VStackOptions{
                     .alignment = components::HorizontalAlignment::leading,
                     .spacing = theme::space::lg},
                 components::HStack(
                     components::HStackOptions{.spacing = theme::space::lg},
                     tile(Demo::dates, "calendar_today", chosen),
                     tile(Demo::colour, "palette", chosen),
                     tile(Demo::gauges, "speaker", chosen),
                     tile(Demo::sizes, "zoom_in", chosen)),
                 components::HStack(
                     components::HStackOptions{.spacing = theme::space::lg},
                     tile(Demo::group_box, "inventory_2", chosen),
                     tile(Demo::editing, "edit", chosen),
                     tile(Demo::menus, "more_horiz", chosen),
                     tile(Demo::wheel, "unfold_more", chosen)))
                 .padding(theme::space::xl)
                 .frame({.max_width = infinity,
                         .alignment = composition::Alignment::leading}))
      .frame({.max_width = infinity, .max_height = infinity});
}

auto GroupingsPage::page(std::vector<Element> content) -> Element {
  return components::ScrollView(
             components::VStack(
                 components::VStackOptions{
                     .alignment = components::HorizontalAlignment::leading,
                     .spacing = theme::space::lg},
                 std::move(content))
                 .padding(theme::space::xl)
                 .frame({.width = 560.0F,
                         .alignment = composition::Alignment::leading}))
      .frame({.max_width = infinity, .max_height = infinity});
}

auto GroupingsPage::dates(composition::Context& context) -> Element {
  const auto picked = context.state(when);
  const auto chosen_days = context.state(days);
  std::vector<Element> content;
  content.push_back(Element{controls::DatePicker("Start", picked)}.id(
      std::string{date_picker}));
  content.push_back(Element{controls::DatePicker(
      "Time", picked, controls::DatePickerComponents::hour_and_minute)});
  content.push_back(
      Element{controls::DatePicker("Calendar", picked,
                                   controls::DatePickerComponents::date)}
          .date_picker_style(controls::DatePickerStyle::graphical())
          .id(std::string{graphical_picker}));
  content.push_back(Element{controls::MultiDatePicker("Trips", chosen_days)}.id(
      std::string{multi_picker}));
  content.push_back(
      components::Text(std::format("{} days chosen", chosen_days.get().size()))
          .font(theme::Font::footnote)
          .foreground_style(ColourRole::text_secondary));
  return page(std::move(content));
}

auto GroupingsPage::colours(composition::Context& context) -> Element {
  const auto picked = context.state(colour);
  const auto on = context.state(wifi);
  const auto amount = context.state(level);
  std::vector<Element> content;
  content.push_back(Element{controls::ColorPicker("Accent", picked)}.id(
      std::string{colour_picker}));
  content.push_back(
      components::VStack(
          components::VStackOptions{
              .alignment = components::HorizontalAlignment::leading,
              .spacing = theme::space::md},
          controls::Toggle("Wi-Fi", on), controls::Slider(amount),
          components::Button("Tinted", [] {})
              .button_style(components::ButtonStyle::bordered_prominent()))
          .tint(picked.get())
          .id(std::string{tinted_preview}));
  return page(std::move(content));
}

auto GroupingsPage::gauges(composition::Context& context) -> Element {
  const auto amount = context.state(level);
  const double value = amount.get();
  const auto labels = [value] {
    return controls::Gauge::Labels{
        .label = components::Text("Battery"),
        .current_value = components::Text(
            std::format("{}", static_cast<int>(value * 100.0))),
        .minimum_value = components::Text("0"),
        .maximum_value = components::Text("100")};
  };
  const controls::ClosedRange<double> unit{.lower_bound = 0.0,
                                           .upper_bound = 1.0};
  std::vector<Element> content;
  content.push_back(controls::Slider(amount));
  content.push_back(Element{controls::Gauge(value, unit, labels())});
  content.push_back(Element{controls::Gauge(value, unit, labels())}.gauge_style(
      controls::GaugeStyle::accessory_linear()));
  content.push_back(Element{controls::Gauge(value, unit, labels())}.gauge_style(
      controls::GaugeStyle::accessory_linear_capacity()));
  content.push_back(components::HStack(
      components::HStackOptions{.spacing = theme::space::xl},
      Element{controls::Gauge(value, unit, labels())}.gauge_style(
          controls::GaugeStyle::accessory_circular()),
      Element{controls::Gauge(value, unit, labels())}.gauge_style(
          controls::GaugeStyle::accessory_circular_capacity())));
  return page(std::move(content));
}

auto GroupingsPage::sizes(composition::Context& context) -> Element {
  const auto on = context.state(wifi);
  const auto counted = context.state(count);
  const auto taste = context.state(flavour);
  constexpr std::array<std::pair<components::ControlSize, std::string_view>, 4>
      ladder{std::pair{components::ControlSize::mini, "Mini"},
             std::pair{components::ControlSize::small, "Small"},
             std::pair{components::ControlSize::regular, "Regular"},
             std::pair{components::ControlSize::large, "Large"}};
  std::vector<Element> rows;
  for (const auto& [size, title] : ladder) {
    rows.push_back(
        components::HStack(
            components::HStackOptions{
                .alignment = components::VerticalAlignment::center,
                .spacing = theme::space::md},
            components::Text(std::string{title}).frame({.width = 72.0F}),
            components::Button("Button", [] {})
                .button_style(components::ButtonStyle::bordered()),
            controls::Toggle("Wi-Fi", on).labels_hidden(),
            controls::Picker(
                "Flavour", taste,
                [] {
                  return Element{components::Group(
                      components::Text("A").tag(Flavour::vanilla),
                      components::Text("B").tag(Flavour::chocolate),
                      components::Text("C").tag(Flavour::strawberry))};
                })
                .picker_style(controls::PickerStyle::segmented())
                .labels_hidden()
                .frame({.width = 120.0F}),
            controls::Stepper(
                "Count", counted,
                controls::ClosedRange<int>{.lower_bound = 0, .upper_bound = 9})
                .labels_hidden())
            .control_size(size));
  }
  std::vector<Element> content;
  content.push_back(
      components::VStack(
          components::VStackOptions{
              .alignment = components::HorizontalAlignment::leading,
              .spacing = theme::space::md},
          std::move(rows))
          .id(std::string{sizes_grid}));
  return page(std::move(content));
}

auto GroupingsPage::group_box(composition::Context& context) -> Element {
  const auto on = context.state(wifi);
  const auto counted = context.state(count);
  const auto taste = context.state(flavour);
  const auto typed = context.state(name);
  std::vector<Element> content;
  content.push_back(structure::GroupBox(
      "Profile",
      structure::Form(
          structure::Section("Player",
                             components::LabeledContent("Console", "Switch"),
                             controls::TextField("Name", typed),
                             controls::Toggle("Online play", on)),
          structure::Section(
              "Snacks",
              controls::Picker("Flavour", taste, [] { return flavours(); }),
              controls::Stepper("Scoops", counted,
                                controls::ClosedRange<int>{.lower_bound = 0,
                                                           .upper_bound = 9})))
          .form_style(structure::FormStyle::columns())
          .frame({.height = 320.0F})
          .id(std::string{columns_form})));
  content.push_back(structure::GroupBox(components::Text(
      "A group box without a title groups its content on a fill.")));
  return page(std::move(content));
}

auto GroupingsPage::editing(composition::Context& context) -> Element {
  const auto editing_mode = context.state(mode);
  const auto all = context.state(notes);
  const auto typed = context.state(name);
  std::vector<Element> content;
  content.push_back(components::HStack(
                        components::HStackOptions{.spacing = theme::space::md},
                        Element{controls::EditButton{}},
                        controls::PasteButton(
                            std::type_identity<std::string>{},
                            [all](std::vector<std::string> pasted) {
                              all.update(
                                  [&pasted](std::vector<std::string>& held) {
                                    for (std::string& each : pasted) {
                                      held.push_back(std::move(each));
                                    }
                                  });
                            }),
                        Element{controls::RenameButton{}}.rename_action(
                            [typed] { typed.set(typed.get() + " (renamed)"); }))
                        .button_style(components::ButtonStyle::bordered())
                        .edit_mode(editing_mode));
  content.push_back(components::Text(typed.get())
                        .font(theme::Font::footnote)
                        .foreground_style(ColourRole::text_secondary));
  content.push_back(
      structure::List(
          structure::Section(
              "Notes",
              components::ForEach(
                  all.get(), [](const std::string& note) { return note; },
                  [](const std::string& note) {
                    return Element{components::Text(note)};
                  })
                  .on_delete([all](const components::IndexSet& offsets) {
                    all.update([&offsets](std::vector<std::string>& held) {
                      offsets.remove_from(held);
                    });
                  })))
          .edit_mode(editing_mode)
          .frame({.height = 260.0F})
          .id(std::string{edit_list}));
  return page(std::move(content));
}

auto GroupingsPage::menus(composition::Context& context) -> Element {
  const auto last = context.state(last_action);
  const auto pressed = context.state(repeats);
  const auto held = context.state(holds);
  const auto entries = [last] {
    return Element{components::Group(
        components::Button("Duplicate", [last] { last.set("duplicated"); }),
        components::Button("Rename", [last] { last.set("renamed"); }),
        components::Button("Delete", [last] { last.set("deleted"); }))};
  };
  std::vector<Element> content;
  content.push_back(components::HStack(
      components::HStackOptions{.spacing = theme::space::md},
      controls::Menu("Automatic", entries),
      controls::Menu("Borderless", entries)
          .menu_style(controls::MenuStyle::borderless_button()),
      controls::Menu("No indicator", entries)
          .menu_indicator(composition::Visibility::hidden),
      controls::Menu("Stays open", entries)
          .menu_action_dismiss_behavior(
              composition::MenuActionDismissBehavior::disabled)));
  content.push_back(components::Text(std::format("Last: {}", last.get()))
                        .font(theme::Font::footnote)
                        .foreground_style(ColourRole::text_secondary));
  const auto group_entries = [last] {
    return Element{components::Group(
        components::Button("Cut", [last] { last.set("cut"); }),
        components::Button("Copy", [last] { last.set("copied"); }),
        components::Button("Paste", [last] { last.set("pasted"); }))};
  };
  content.push_back(components::HStack(
      components::HStackOptions{.spacing = theme::space::md},
      structure::ControlGroup(group_entries),
      structure::ControlGroup("Edit", group_entries)
          .control_group_style(structure::ControlGroupStyle::menu()),
      structure::ControlGroup(group_entries)
          .control_group_style(structure::ControlGroupStyle::palette())));
  content.push_back(components::HStack(
      components::HStackOptions{.spacing = theme::space::md},
      components::Button("Capsule", [] {})
          .button_style(components::ButtonStyle::bordered())
          .button_border_shape(components::ButtonBorderShape::capsule()),
      components::Button("Square", [] {})
          .button_style(components::ButtonStyle::bordered())
          .button_border_shape(
              components::ButtonBorderShape::rounded_rectangle(2.0F)),
      components::Button(std::format("Repeat {}", pressed.get()),
                         [pressed] { pressed.set(pressed.get() + 1); })
          .button_style(components::ButtonStyle::bordered())
          .button_repeat_behavior(components::ButtonRepeatBehavior::enabled)
          .id(std::string{repeat_button}),
      components::Button(std::format("Held {}", held.get()),
                         [held] { held.set(held.get() + 1); })
          .button_style(HoldStyle{})
          .id(std::string{hold_button})));
  content.push_back(
      components::Button("Flexible", [] {})
          .button_style(components::ButtonStyle::bordered_prominent())
          .button_sizing(components::ButtonSizing::flexible));
  return page(std::move(content));
}

auto GroupingsPage::wheel(composition::Context& context) -> Element {
  const auto taste = context.state(flavour);
  std::vector<Element> content;
  content.push_back(
      controls::Picker("Flavour", taste, [] { return flavours(); })
          .picker_style(controls::PickerStyle::wheel())
          .id(std::string{wheel_picker}));
  content.push_back(components::Button("After", [] {
                    }).button_style(components::ButtonStyle::bordered()));
  return page(std::move(content));
}

auto GroupingsPage::Shown::body(composition::Context& context) const
    -> Element {
  Element shown = [&] {
    switch (chosen_) {
      case Demo::dates:
        return dates(context);
      case Demo::colour:
        return colours(context);
      case Demo::gauges:
        return gauges(context);
      case Demo::sizes:
        return sizes(context);
      case Demo::group_box:
        return group_box(context);
      case Demo::editing:
        return editing(context);
      case Demo::menus:
        return menus(context);
      case Demo::wheel:
        return wheel(context);
    }
    return dates(context);
  }();
  return std::move(shown).navigation_title(std::string{title_of(chosen_)});
}

auto GroupingsPage::body(composition::Context& context) const -> Element {
  const auto chosen_demo = context.state(demo);
  const auto shown_flag = shown_;
  return presentation::NavigationStack([chosen_demo, shown_flag] {
    return tiles(chosen_demo)
        .navigation_title("Controls and groupings")
        .toolbar(ToolbarItem(
            ToolbarItemPlacement::cancellation_action,
            components::Button("Close", [shown_flag] { shown_flag.set(false); })
                .id(std::string{close_button})))
        .navigation_destination(
            chosen_demo, [](const Demo& each) { return Element{Shown{each}}; });
  });
}

auto GroupingsPage::Opener::body(composition::Context& context) const
    -> Element {
  const auto shown = context.state(shown_key);
  return components::Button("Controls and groupings",
                            [shown] { shown.set(true); })
      .id(std::string{button})
      .full_screen_cover(shown,
                         [shown] { return Element{GroupingsPage{shown}}; });
}

}
