#include <input_page.hpp>

#include <arisa/components/button.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/coordinate_space.hpp>
#include <arisa/composition/focus_interactions.hpp>
#include <arisa/composition/gesture.hpp>
#include <arisa/composition/key_equivalent.hpp>
#include <arisa/composition/key_press.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/magnify_gesture.hpp>
#include <arisa/composition/rotate_gesture.hpp>
#include <arisa/composition/sensory_feedback.hpp>
#include <arisa/composition/toolbar_item.hpp>
#include <arisa/controls/context_menu.hpp>
#include <arisa/motion/angle.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/presentation/navigation_stack.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/presentation/presented_destination.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/stroke.hpp>
#include <arisa/theme/typography.hpp>

#include <algorithm>
#include <format>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace arisa::examples {

using composition::Element;
using composition::infinity;
using composition::ToolbarItem;
using composition::ToolbarItemPlacement;
using theme::ColourRole;

const composition::StateKey<
    std::array<std::optional<std::string>, InputPage::grid_cells>>
    InputPage::cells{"input-cells", {}};

const composition::FocusedValueKey<std::string> InputPage::picked{
    "input-picked"};

auto InputPage::title_of(Demo chosen) -> std::string_view {
  switch (chosen) {
    case Demo::pinch:
      return "Pinch and rotate";
    case Demo::location:
      return "Tap location";
    case Demo::sequence:
      return "Double tap and drag";
    case Demo::clipboard:
      return "Copy and paste";
    case Demo::home:
      return "Home grid";
    case Demo::focused:
      return "Focused value";
    case Demo::reset:
      return "Reset focus";
    case Demo::keyboard:
      return "Keyboard";
  }
  return "Pinch and rotate";
}

auto InputPage::tile_key(std::string_view prefix, std::size_t index)
    -> std::string {
  return std::format("{}-{}", prefix, index);
}

auto InputPage::tile(Demo each, std::string_view symbol,
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
      .id(tile_key("input-demo", static_cast<std::size_t>(each)));
}

auto InputPage::tiles(composition::StateHandle<std::optional<Demo>> chosen)
    -> Element {
  return components::ScrollView(
             components::VStack(
                 components::VStackOptions{
                     .alignment = components::HorizontalAlignment::leading,
                     .spacing = theme::space::lg},
                 components::HStack(
                     components::HStackOptions{.spacing = theme::space::lg},
                     tile(Demo::pinch, "zoom_in", chosen),
                     tile(Demo::location, "location_on", chosen),
                     tile(Demo::sequence, "drag_indicator", chosen),
                     tile(Demo::clipboard, "content_copy", chosen)),
                 components::HStack(
                     components::HStackOptions{.spacing = theme::space::lg},
                     tile(Demo::home, "grid_view", chosen),
                     tile(Demo::focused, "star", chosen),
                     tile(Demo::reset, "refresh", chosen),
                     tile(Demo::keyboard, "keyboard", chosen)))
                 .padding(theme::space::xl)
                 .frame({.max_width = infinity,
                         .alignment = composition::Alignment::leading}))
      .frame({.max_width = infinity, .max_height = infinity});
}

auto InputPage::card(std::string_view name) -> Element {
  return components::Text(std::string{name})
      .font(theme::Font::headline)
      .frame({.width = 120.0F, .height = 56.0F})
      .background(ColourRole::surface_raised,
                  theme::Shape::rounded(theme::radius::control, true))
      .border(ColourRole::separator, theme::Stroke::hairline());
}

auto InputPage::pinch(composition::Context& context) -> Element {
  const auto committed_scale = context.state(scale);
  const auto committed_angle = context.state(angle);
  const auto live_scale = context.state(magnifying);
  const auto live_angle = context.state(turning);
  const double shown_scale = committed_scale.get() * live_scale.get();
  const double shown_angle = committed_angle.get() + live_angle.get();
  auto gesture =
      composition::MagnifyGesture()
          .updating(live_scale,
                    [](const composition::MagnifyGesture::Value& value,
                       double& held) { held = value.magnification; })
          .on_ended([committed_scale](
                        const composition::MagnifyGesture::Value& value) {
            committed_scale.set(std::clamp(
                committed_scale.get() * value.magnification, 0.5, 4.0));
          })
          .simultaneously(
              composition::RotateGesture()
                  .updating(
                      live_angle,
                      [](const composition::RotateGesture::Value& value,
                         double& held) { held = value.rotation.degrees(); })
                  .on_ended(
                      [committed_angle](
                          const composition::RotateGesture::Value& value) {
                        committed_angle.set(committed_angle.get() +
                                            value.rotation.degrees());
                      }));
  return components::VStack(
             components::VStackOptions{
                 .alignment = components::HorizontalAlignment::center,
                 .spacing = theme::space::xl},
             components::Image::system_name("image")
                 .font(theme::Font::large_title)
                 .frame({.width = 200.0F, .height = 200.0F})
                 .background(ColourRole::accent,
                             theme::Shape::rounded(theme::radius::card, true))
                 .scale_effect(static_cast<float>(shown_scale))
                 .rotation_effect(motion::Angle::degrees(shown_angle))
                 .gesture(std::move(gesture))
                 .id(std::string{pinch_target}),
             components::Text(
                 std::format("{:.2f}x {:.0f}°", shown_scale, shown_angle))
                 .monospaced_digit(),
             components::Button("Reset",
                                [committed_scale, committed_angle] {
                                  committed_scale.set(1.0);
                                  committed_angle.set(0.0);
                                }))
      .frame({.max_width = infinity, .max_height = infinity});
}

auto InputPage::location(composition::Context& context) -> Element {
  const auto last = context.state(tapped);
  std::optional<Element> dot;
  if (last.get().has_value()) {
    dot = components::ZStack()
              .frame({.width = 16.0F, .height = 16.0F})
              .background(ColourRole::accent, theme::Capsule())
              .position(*last.get());
  }
  const std::string readout =
      last.get().has_value()
          ? std::format("{:.0f}, {:.0f}", last.get()->x, last.get()->y)
          : std::string{"—"};
  return components::VStack(
             components::VStackOptions{
                 .alignment = components::HorizontalAlignment::center,
                 .spacing = theme::space::lg},
             components::ZStack(
                 components::ZStackOptions{
                     .alignment = components::Alignment::top_leading},
                 std::move(dot))
                 .frame({.width = 480.0F, .height = 280.0F})
                 .background(ColourRole::surface_raised,
                             theme::Shape::rounded(theme::radius::card, true))
                 .content_shape(theme::Shape::rectangle())
                 .on_tap_gesture(
                     1, composition::CoordinateSpace::local,
                     [last](composition::Point point) { last.set(point); })
                 .id(std::string{location_target}),
             components::Text(readout).monospaced_digit())
      .frame({.max_width = infinity, .max_height = infinity});
}

auto InputPage::sequence(composition::Context& context) -> Element {
  const auto rest = context.state(moved);
  const auto live = context.state(pulling);
  const auto ready = context.state(armed);
  const composition::Point at{.x = rest.get().x + live.get().width,
                              .y = rest.get().y + live.get().height};
  auto gesture =
      composition::TapGesture(2)
          .on_ended([ready] { ready.set(true); })
          .sequenced(
              composition::DragGesture()
                  .updating(
                      live,
                      [](const composition::DragGesture::Value& value,
                         composition::Size& held) { held = value.translation; })
                  .on_ended([rest, ready](
                                const composition::DragGesture::Value& value) {
                    rest.set(composition::Point{
                        .x = rest.get().x + value.translation.width,
                        .y = rest.get().y + value.translation.height});
                    ready.set(false);
                  }));
  return components::VStack(
             components::VStackOptions{
                 .alignment = components::HorizontalAlignment::center,
                 .spacing = theme::space::lg},
             components::ZStack(card(ready.get() ? "Drag" : "Double tap")
                                    .offset(at.x, at.y)
                                    .gesture(std::move(gesture))
                                    .id(std::string{sequence_target}))
                 .frame({.width = 480.0F, .height = 280.0F})
                 .background(ColourRole::surface,
                             theme::Shape::rounded(theme::radius::card, true)))
      .frame({.max_width = infinity, .max_height = infinity});
}

auto InputPage::clipboard(composition::Context& context) -> Element {
  const auto received = context.state(pasted);
  std::vector<Element> left;
  for (std::size_t index = 0; index < palette.size(); ++index) {
    const std::string name{palette[index]};
    left.push_back(components::Button(name, [] {})
                       .copyable(std::vector<GalleryTile>{GalleryTile{name}})
                       .id(tile_key("input-copy", index)));
  }
  std::vector<Element> right;
  for (std::size_t index = 0; index < received.get().size(); ++index) {
    right.push_back(components::Button(received.get()[index], [] {})
                        .on_delete_command([received, index] {
                          std::vector<std::string> next = received.get();
                          if (index < next.size()) {
                            next.erase(next.begin() +
                                       static_cast<std::ptrdiff_t>(index));
                            received.set(std::move(next));
                          }
                        })
                        .id(tile_key("input-pasted", index)));
  }
  if (right.empty()) {
    right.push_back(components::Image::system_name("content_paste")
                        .font(theme::Font::title2)
                        .foreground_style(ColourRole::text_secondary)
                        .frame({.width = 120.0F, .height = 56.0F}));
  }
  const auto column = [](std::vector<Element> rows) {
    return components::VStack(
               components::VStackOptions{
                   .alignment = components::HorizontalAlignment::leading,
                   .spacing = theme::space::sm},
               std::move(rows))
        .padding(theme::space::lg)
        .frame({.width = 240.0F,
                .min_height = 240.0F,
                .alignment = composition::Alignment::top_leading})
        .background(ColourRole::surface_raised,
                    theme::Shape::rounded(theme::radius::card, true));
  };
  return components::HStack(
             components::HStackOptions{
                 .alignment = components::VerticalAlignment::top,
                 .spacing = theme::space::xl},
             column(std::move(left)).focus_section(),
             column(std::move(right))
                 .focusable(true, composition::FocusInteractions::edit)
                 .paste_destination(std::type_identity<GalleryTile>{},
                                    [received](std::vector<GalleryTile> tiles) {
                                      std::vector<std::string> next =
                                          received.get();
                                      for (const GalleryTile& each : tiles) {
                                        next.push_back(each.name());
                                      }
                                      received.set(std::move(next));
                                    })
                 .focus_section()
                 .id(std::string{pasted_list}))
      .frame({.max_width = infinity, .max_height = infinity});
}

auto InputPage::home(composition::Context& context) -> Element {
  const auto grid = context.state(cells);
  const auto over = context.state(targeted);
  const auto count = context.state(placed);
  std::vector<Element> docked;
  for (std::size_t index = 0; index < dock.size(); ++index) {
    const std::string name{dock[index]};
    docked.push_back(card(name)
                         .focusable()
                         .draggable(GalleryTile{name},
                                    [name] { return card(name).opacity(0.9F); })
                         .id(tile_key("input-dock", index)));
  }
  std::vector<Element> rows;
  for (std::size_t row = 0; row < 3; ++row) {
    std::vector<Element> line;
    for (std::size_t column = 0; column < 3; ++column) {
      const std::size_t index = (row * 3) + column;
      const std::optional<std::string>& held = grid.get()[index];
      const bool hovered = over.get() == static_cast<int>(index);
      line.push_back(
          components::ZStack(held.has_value()
                                 ? std::optional<Element>{card(*held)}
                                 : std::nullopt)
              .frame({.width = 140.0F, .height = 72.0F})
              .background(hovered ? ColourRole::accent : ColourRole::surface,
                          theme::Shape::rounded(theme::radius::control, true))
              .border(ColourRole::separator, theme::Stroke::hairline())
              .focusable()
              .drop_destination(
                  std::type_identity<GalleryTile>{},
                  [grid, count, index](std::vector<GalleryTile> tiles,
                                       composition::Point) {
                    auto next = grid.get();
                    next[index] = tiles.front().name();
                    grid.set(next);
                    count.set(count.get() + 1);
                    return true;
                  },
                  [over, index](bool inside) {
                    if (inside) {
                      over.set(static_cast<int>(index));
                    } else if (over.get() == static_cast<int>(index)) {
                      over.set(-1);
                    }
                  })
              .id(tile_key("input-cell", index)));
    }
    rows.push_back(components::HStack(
        components::HStackOptions{.spacing = theme::space::sm},
        std::move(line)));
  }
  return components::VStack(
             components::VStackOptions{
                 .alignment = components::HorizontalAlignment::center,
                 .spacing = theme::space::xl},
             components::VStack(
                 components::VStackOptions{.spacing = theme::space::sm},
                 std::move(rows))
                 .focus_section(),
             components::HStack(
                 components::HStackOptions{.spacing = theme::space::md},
                 std::move(docked))
                 .padding(theme::space::md)
                 .background(ColourRole::surface_raised,
                             theme::Shape::rounded(theme::radius::card, true))
                 .focus_section())
      .sensory_feedback(
          composition::SensoryFeedback::success, count.get(),
          [](const int& before, const int& after) { return after > before; })
      .frame({.max_width = infinity, .max_height = infinity});
}

auto InputPage::focused(composition::Context& context) -> Element {
  const auto starred = context.state(favourites);
  const std::optional<std::string> current = context.focused_value(picked);
  std::vector<Element> rows;
  for (std::size_t index = 0; index < shelf.size(); ++index) {
    const std::string name{shelf[index]};
    const bool favourite = std::ranges::contains(starred.get(), name);
    rows.push_back(
        components::Button(
            [] {},
            components::HStack(
                components::HStackOptions{.spacing = theme::space::sm},
                components::Text(name),
                favourite
                    ? std::optional<Element>{components::Image::system_name(
                          "star")}
                    : std::nullopt))
            .frame(
                {.width = 240.0F, .alignment = composition::Alignment::leading})
            .focused_value(picked, name)
            .id(tile_key("input-shelf", index)));
  }
  const auto toggle = [starred, current] {
    if (!current.has_value()) {
      return;
    }
    std::vector<std::string> next = starred.get();
    if (const auto found = std::ranges::find(next, *current);
        found != next.end()) {
      next.erase(found);
    } else {
      next.push_back(*current);
    }
    starred.set(std::move(next));
  };
  return components::VStack(
             components::VStackOptions{
                 .alignment = components::HorizontalAlignment::leading,
                 .spacing = theme::space::sm},
             std::move(rows))
      .padding(theme::space::xl)
      .frame({.max_width = infinity,
              .max_height = infinity,
              .alignment = composition::Alignment::top_leading})
      .toolbar(ToolbarItem(
          ToolbarItemPlacement::primary_action,
          components::Button("Favourite", toggle)
              .focusable(true, composition::FocusInteractions::activate)
              .disabled(!current.has_value())
              .id(std::string{favourite_button})))
      .on_command(provider::Button::y, "Favourite", toggle);
}

auto InputPage::reset(composition::Context& context) -> Element {
  const auto loads = context.state(generation);
  const composition::Namespace::ID scope = context.namespace_id(list_space);
  const composition::ResetFocusAction refocus =
      context.environment(components::Environment::reset_focus);
  std::vector<Element> rows;
  for (std::size_t index = 0; index < shelf.size(); ++index) {
    Element row = components::Button(
                      std::format("{} {}", shelf[index], loads.get()), [] {})
                      .frame({.width = 240.0F,
                              .alignment = composition::Alignment::leading})
                      .id(composition::ElementKey{static_cast<std::int64_t>(
                          (loads.get() * 10) + static_cast<int>(index))});
    if (index == 0) {
      row = std::move(row).prefers_default_focus(scope);
    }
    rows.push_back(std::move(row));
  }
  return components::HStack(
             components::HStackOptions{
                 .alignment = components::VerticalAlignment::top,
                 .spacing = theme::space::xl},
             components::VStack(
                 components::VStackOptions{
                     .alignment = components::HorizontalAlignment::leading,
                     .spacing = theme::space::sm},
                 std::move(rows))
                 .focus_scope(scope),
             components::Button("Reload",
                                [loads, refocus, scope] {
                                  loads.set(loads.get() + 1);
                                  refocus(scope);
                                })
                 .id(std::string{reload_button}))
      .padding(theme::space::xl)
      .frame({.max_width = infinity,
              .max_height = infinity,
              .alignment = composition::Alignment::top_leading});
}

auto InputPage::keyboard(composition::Context& context) -> Element {
  const auto last = context.state(pressed_key);
  const auto shown_page = context.state(page);
  const auto fired = context.state(shortcuts);
  return components::VStack(
             components::VStackOptions{
                 .alignment = components::HorizontalAlignment::center,
                 .spacing = theme::space::lg},
             components::Text(last.get().empty() ? std::string{"—"}
                                                 : last.get())
                 .font(theme::Font::title)
                 .frame({.width = 320.0F, .height = 120.0F})
                 .background(ColourRole::surface_raised,
                             theme::Shape::rounded(theme::radius::card, true))
                 .focusable(true, composition::FocusInteractions::edit)
                 .on_key_press(
                     composition::KeyPress::Phases::down,
                     [last](const composition::KeyPress& press) {
                       if (press.characters.empty()) {
                         return composition::KeyPress::Result::ignored;
                       }
                       last.set(press.characters);
                       return composition::KeyPress::Result::handled;
                     })
                 .id(std::string{keyboard_area}),
             components::Text(
                 std::format("{} / {}", shown_page.get(), page_count))
                 .monospaced_digit(),
             components::HStack(
                 components::HStackOptions{.spacing = theme::space::md},
                 components::Button(std::format("Next {}", fired.get()),
                                    [fired] { fired.set(fired.get() + 1); })
                     .keyboard_shortcut('n')
                     .id(std::string{next_button})))
      .page_command(composition::Binding<int>{shown_page}, 1, page_count)
      .frame({.max_width = infinity, .max_height = infinity});
}

auto InputPage::Shown::body(composition::Context& context) const -> Element {
  Element shown = [&] {
    switch (chosen_) {
      case Demo::pinch:
        return pinch(context);
      case Demo::location:
        return location(context);
      case Demo::sequence:
        return sequence(context);
      case Demo::clipboard:
        return clipboard(context);
      case Demo::home:
        return home(context);
      case Demo::focused:
        return focused(context);
      case Demo::reset:
        return reset(context);
      case Demo::keyboard:
        return keyboard(context);
    }
    return pinch(context);
  }();
  return std::move(shown).navigation_title(std::string{title_of(chosen_)});
}

auto InputPage::body(composition::Context& context) const -> Element {
  const auto chosen = context.state(demo);
  const auto shown_flag = shown_;
  return presentation::NavigationStack([chosen, shown_flag] {
    return tiles(chosen)
        .navigation_title("Input")
        .toolbar(ToolbarItem(
            ToolbarItemPlacement::cancellation_action,
            components::Button("Close", [shown_flag] { shown_flag.set(false); })
                .id(std::string{close_button})))
        .navigation_destination(
            chosen, [](const Demo& each) { return Element{Shown{each}}; });
  });
}

auto InputPage::Opener::body(composition::Context& context) const -> Element {
  const auto shown = context.state(shown_key);
  return components::Button("Input", [shown] { shown.set(true); })
      .id(std::string{button})
      .full_screen_cover(shown, [shown] { return Element{InputPage{shown}}; });
}

}
