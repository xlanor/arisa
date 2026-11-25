#include <interaction_page.hpp>

#include <arisa/components/button.hpp>
#include <arisa/components/button_style.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/for_each.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/event_modifiers.hpp>
#include <arisa/composition/gesture.hpp>
#include <arisa/composition/gesture_input_kinds.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/reorder_difference.hpp>
#include <arisa/composition/spring_loading_behavior.hpp>
#include <arisa/composition/toolbar_item.hpp>
#include <arisa/presentation/navigation_stack.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/stroke.hpp>
#include <arisa/theme/typography.hpp>

#include <algorithm>
#include <array>
#include <format>
#include <utility>

namespace arisa::examples {

using composition::Element;
using composition::infinity;
using theme::ColourRole;

const composition::StateKey<std::vector<std::string>> InteractionPage::dropped{
    "interaction-dropped", {}};
const composition::StateKey<std::vector<int>> InteractionPage::selected{
    "interaction-selected", {1, 2}};
const composition::StateKey<std::vector<int>> InteractionPage::left{
    "interaction-left", {1, 2, 3}};
const composition::StateKey<std::vector<int>> InteractionPage::right{
    "interaction-right", {4}};

auto InteractionPage::Tile::encoded() const -> std::string { return name; }

auto InteractionPage::Tile::decoded(std::string_view text)
    -> std::optional<Tile> {
  return Tile{.id = 0, .name = std::string{text}};
}

auto InteractionPage::Tile::content_type() -> std::string {
  return "com.arisa.gallery.tile";
}

auto InteractionPage::tile_key(int id) -> std::string {
  return std::format("{}{}", tile_prefix, id);
}

auto InteractionPage::fruit_key(std::string_view name) -> std::string {
  return std::format("{}{}", fruit_prefix, name);
}

auto InteractionPage::tile_name(int id) -> std::string {
  static constexpr std::array<std::string_view, 4> names{"Red", "Green", "Blue",
                                                         "Gold"};
  return std::string{names[static_cast<std::size_t>((id - 1) % tile_count)]};
}

auto InteractionPage::section(std::string_view title, Element content)
    -> Element {
  return components::VStack(
             components::VStackOptions{
                 .alignment = components::HorizontalAlignment::leading,
                 .spacing = theme::space::md},
             components::Text(std::string{title}).font(theme::Font::headline),
             std::move(content))
      .frame({.max_width = infinity,
              .alignment = composition::Alignment::leading});
}

auto InteractionPage::area(std::string_view key, std::string_view label)
    -> Element {
  return components::Text(std::string{label})
      .frame({.width = area_width, .height = area_height})
      .background(ColourRole::surface_raised,
                  theme::Shape::rounded(theme::radius::control, true))
      .border(ColourRole::separator, theme::Stroke::hairline())
      .id(std::string{key});
}

auto InteractionPage::kinds(composition::StateHandle<int> touched,
                            composition::StateHandle<int> clicked,
                            composition::StateHandle<int> pressed) -> Element {
  return components::HStack(
      components::HStackOptions{.spacing = theme::space::md},
      area(touch_area, std::format("Touch {}", touched.get()))
          .on_tap_gesture(1, composition::CoordinateSpace::local,
                          composition::GestureInputKinds::direct_touch,
                          [touched](composition::Point) {
                            touched.set(touched.get() + 1);
                          }),
      area(pointer_area, std::format("Pointer {}", clicked.get()))
          .gesture(
              composition::TapGesture(1,
                                      composition::GestureInputKinds::pointer)
                  .on_ended([clicked] { clicked.set(clicked.get() + 1); })),
      area(controller_area, std::format("Controller {}", pressed.get()))
          .gesture(
              composition::TapGesture(
                  1, composition::GestureInputKinds::controller)
                  .on_ended([pressed] { pressed.set(pressed.get() + 1); })));
}

auto InteractionPage::mapped(composition::StateHandle<int> travelled)
    -> Element {
  return area(mapped_area, std::format("Moved {}", travelled.get()))
      .gesture(composition::DragGesture(4.0F)
                   .map([](const composition::DragGesture::Value& value) {
                     return static_cast<int>(value.translation.width);
                   })
                   .on_ended([travelled](const int& distance) {
                     travelled.set(distance);
                   }),
               "pan");
}

auto InteractionPage::keys(composition::StateHandle<bool> held) -> Element {
  return components::HStack(
      components::HStackOptions{.spacing = theme::space::md},
      components::Text("Option").modifier_key_alternate(
          composition::EventModifiers::option, components::Text("Held")),
      components::Text(held.get() ? "Shift" : "Released")
          .on_modifier_keys_changed(
              composition::EventModifiers::shift, true,
              [held](composition::EventModifiers,
                     composition::EventModifiers next) {
                held.set(next.contains(composition::EventModifiers::shift));
              }));
}

auto InteractionPage::fruits() -> Element {
  std::vector<Element> rows;
  for (const std::string_view name :
       {std::string_view{"Apple"}, std::string_view{"Banana"},
        std::string_view{"Blueberry"}, std::string_view{"Cherry"}}) {
    rows.push_back(components::Button(std::string{name}, [] {})
                       .type_select_equivalent(std::string{name})
                       .id(fruit_key(name)));
  }
  return components::HStack(
      components::HStackOptions{.spacing = theme::space::sm}, std::move(rows));
}

auto InteractionPage::carrying(
    composition::Namespace::ID space,
    composition::StateHandle<std::vector<int>> chosen,
    composition::StateHandle<std::vector<std::string>> received,
    composition::StateHandle<composition::DragDropPreviewsFormation> shape,
    composition::StateHandle<bool> opened) -> Element {
  std::vector<Element> cards;
  for (int id = 1; id <= tile_count; ++id) {
    const bool picked = std::ranges::contains(chosen.get(), id);
    cards.push_back(
        components::Button(tile_name(id),
                           [chosen, id] {
                             std::vector<int> next = chosen.get();
                             if (std::ranges::contains(next, id)) {
                               std::erase(next, id);
                             } else {
                               next.push_back(id);
                             }
                             chosen.set(std::move(next));
                           })
            .button_style(picked ? components::ButtonStyle::bordered_prominent()
                                 : components::ButtonStyle::bordered())
            .draggable_container_item(id, space)
            .id(tile_key(id)));
  }
  std::string landed;
  for (const std::string& name : received.get()) {
    landed += landed.empty() ? name : " " + name;
  }
  const composition::DragDropPreviewsFormation formed = shape.get();
  return components::VStack(
      components::VStackOptions{
          .alignment = components::HorizontalAlignment::leading,
          .spacing = theme::space::md},
      components::HStack(components::HStackOptions{.spacing = theme::space::sm},
                         std::move(cards))
          .drag_container(std::type_identity<Tile>{}, space,
                          [](std::vector<int> ids) {
                            std::vector<Tile> made;
                            for (const int id : ids) {
                              made.push_back(
                                  Tile{.id = id, .name = tile_name(id)});
                            }
                            return made;
                          })
          .drag_container_selection(chosen.get(), space)
          .drag_previews_formation(formed),
      components::HStack(
          components::HStackOptions{.spacing = theme::space::md},
          components::Button(
              formed == composition::DragDropPreviewsFormation::pile
                  ? "Pile"
                  : (formed == composition::DragDropPreviewsFormation::stack
                         ? "Stack"
                         : "List"),
              [shape] {
                shape.set(
                    shape.get() == composition::DragDropPreviewsFormation::pile
                        ? composition::DragDropPreviewsFormation::stack
                        : (shape.get() ==
                                   composition::DragDropPreviewsFormation::stack
                               ? composition::DragDropPreviewsFormation::list
                               : composition::DragDropPreviewsFormation::pile));
              }),
          components::Button(opened.get() ? "Opened" : "Open",
                             [opened] { opened.set(!opened.get()); })
              .spring_loading_behavior(
                  composition::SpringLoadingBehavior::enabled)
              .id(std::string{spring_button})),
      components::Text(landed.empty() ? "Drop here" : landed)
          .frame({.width = 2.0F * area_width, .height = area_height})
          .background(ColourRole::surface_raised,
                      theme::Shape::rounded(theme::radius::control, true))
          .border(ColourRole::separator, theme::Stroke::hairline())
          .focusable()
          .drop_destination(
              std::type_identity<Tile>{},
              [received](std::vector<Tile> items, composition::Point) {
                std::vector<std::string> names;
                for (const Tile& tile : items) {
                  names.push_back(tile.name);
                }
                received.set(std::move(names));
                return true;
              })
          .id(std::string{tray}));
}

auto InteractionPage::reordering(
    composition::StateHandle<std::vector<int>> first,
    composition::StateHandle<std::vector<int>> second) -> Element {
  const auto row = [](const int& id) {
    return Element{
        components::Button(tile_name(id), [] {}).frame({.width = area_width})};
  };
  const auto self = [](const int& id) { return id; };
  return components::HStack(
             components::HStackOptions{
                 .alignment = components::VerticalAlignment::top,
                 .spacing = theme::space::lg},
             components::VStack(
                 components::VStackOptions{.spacing = theme::space::sm},
                 components::ForEach(first.get(), self, row)
                     .reorderable(composition::ElementKey{"first"})),
             components::VStack(
                 components::VStackOptions{.spacing = theme::space::sm},
                 components::ForEach(second.get(), self, row)
                     .reorderable(composition::ElementKey{"second"})))
      .reorder_container(
          std::type_identity<Tile>{}, &Tile::id,
          std::type_identity<std::string>{}, true,
          [first,
           second](composition::ReorderDifference<int, std::string> move) {
            std::vector<int> a = first.get();
            std::vector<int> b = second.get();
            const auto drop = [&move](std::vector<int>& from) {
              std::erase_if(from, [&move](int id) {
                return std::ranges::contains(move.sources(), id);
              });
            };
            drop(a);
            drop(b);
            std::vector<int>& into =
                move.destination().collection_id() == "first" ? a : b;
            auto at = into.end();
            if (const auto& before = move.destination().position().item()) {
              at = std::ranges::find(into, *before);
            }
            into.insert(at, move.sources().begin(), move.sources().end());
            first.set(std::move(a));
            second.set(std::move(b));
          });
}

auto InteractionPage::repeating(composition::StateHandle<int> counted)
    -> Element {
  return components::Button(std::format("Step {}", counted.get()),
                            [counted] { counted.set(counted.get() + 1); })
      .environment(components::Environment::button_repeat_behavior,
                   components::ButtonRepeatBehavior::enabled)
      .id(std::string{repeat_button});
}

auto InteractionPage::body(composition::Context& context) const -> Element {
  const auto shown_flag = shown_;
  const composition::Namespace::ID space = context.namespace_id(tiles);
  const auto touched = context.state(touches);
  const auto clicked = context.state(clicks);
  const auto pressed = context.state(presses);
  const auto travelled = context.state(moved);
  const auto held = context.state(option);
  const auto chosen = context.state(selected);
  const auto received = context.state(dropped);
  const auto shape = context.state(formation);
  const auto opened = context.state(sprung);
  const auto first = context.state(left);
  const auto second = context.state(right);
  const auto counted = context.state(repeats);
  return presentation::NavigationStack([=] {
    return components::ScrollView(
               components::VStack(
                   components::VStackOptions{
                       .alignment = components::HorizontalAlignment::leading,
                       .spacing = theme::space::xl},
                   section("Input kinds", kinds(touched, clicked, pressed)),
                   section("Mapped gesture", mapped(travelled)),
                   section("Modifier keys", keys(held)),
                   section("Type select", fruits()),
                   section("Drag and drop",
                           carrying(space, chosen, received, shape, opened)),
                   section("Reorder", reordering(first, second)),
                   section("Repeat", repeating(counted)))
                   .padding(theme::space::xl)
                   .frame({.max_width = infinity,
                           .alignment = composition::Alignment::leading}))
        .frame({.max_width = infinity, .max_height = infinity})
        .defers_system_gestures(theme::Edge::bottom)
        .navigation_title("Interaction")
        .toolbar(composition::ToolbarItem(
            composition::ToolbarItemPlacement::cancellation_action,
            components::Button("Close", [shown_flag] {
              shown_flag.set(false);
            }).id(std::string{close_button})));
  });
}

auto InteractionPage::Opener::body(composition::Context& context) const
    -> Element {
  const auto shown = context.state(shown_key);
  return components::Button("Interaction", [shown] { shown.set(true); })
      .id(std::string{button})
      .full_screen_cover(shown,
                         [shown] { return Element{InteractionPage{shown}}; });
}

}
