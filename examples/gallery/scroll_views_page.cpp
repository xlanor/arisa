#include <scroll_views_page.hpp>

#include <arisa/composition/length.hpp>
#include <arisa/markup/markup.hpp>
#include <arisa/structure/list_style.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/space.hpp>

#include <cmath>
#include <format>
#include <ranges>
#include <string>
#include <vector>

namespace arisa::examples {

using namespace markup;
using composition::Element;
using composition::infinity;
using theme::ColourRole;
using theme::RoundedCornerStyle;
using theme::RoundedRectangle;

const composition::StateKey<composition::ScrollPosition>
    ScrollViewsPage::position_key{"scroll-views-position",
                                  composition::ScrollPosition{}};

void ScrollViewsPage::CardSnap::update_target(
    composition::ScrollTarget& target,
    const composition::ScrollTargetContext&) const {
  if (pitch_ <= 0.0F) {
    return;
  }
  target.rect.x = std::round(target.rect.x / pitch_) * pitch_;
}

auto ScrollViewsPage::chat(composition::StateHandle<int> messages) -> Element {
  const int count = messages.get();
  return VStack(alignment::leading, spacing(theme::space::sm),
                frame({.max_width = infinity, .max_height = infinity}))(
      Button(id(send_button),
             action([messages] { messages.set(messages.get() + 1); }))("Send"),
      ScrollView(id(chat_view),
                 frame({.max_width = infinity, .max_height = infinity}),
                 content_margins(theme::space::sm),
                 default_scroll_anchor(UnitPoint::bottom))(VStack(
          spacing(theme::space::xs), frame({.max_width = infinity}))(ForEach(
          std::views::iota(0, count), [](int index) { return index; },
          [](int index) {
            const bool mine = index % 3 == 0;
            return HStack(frame({.max_width = infinity}))(
                mine ? Element{Spacer()} : Element{components::Group()},
                Text(font::body,
                     foreground_style(mine ? ColourRole::on_accent
                                           : ColourRole::label),
                     padding(theme::Insets::symmetric(theme::space::md,
                                                      theme::space::xs)),
                     background(
                         mine ? ColourRole::accent
                              : ColourRole::secondary_system_fill,
                         RoundedRectangle(theme::radius::card,
                                          RoundedCornerStyle::continuous)),
                     focusable())(std::format("Message {}", index + 1)),
                mine ? Element{components::Group()} : Element{Spacer()});
          }))));
}

auto ScrollViewsPage::carousel() -> Element {
  return ScrollView(
      axes(Axes::horizontal), id(carousel_view), frame({.max_width = infinity}),
      content_margins(theme::Edge::horizontal, theme::space::sm),
      scroll_target_behavior(CardSnap{card_width + card_spacing}))(
      HStack(spacing(card_spacing))(ForEach(
          std::views::iota(0, card_count), [](int index) { return index; },
          [](int index) {
            return Button(action([index] { static_cast<void>(index); }),
                          frame({.width = card_width, .height = 140.0F}))(
                std::format("Card {}", index + 1));
          })));
}

auto ScrollViewsPage::map() -> Element {
  std::vector<Element> lines;
  lines.reserve(map_rows);
  for (int row = 0; row < map_rows; ++row) {
    std::vector<Element> tiles;
    tiles.reserve(map_columns);
    for (int column = 0; column < map_columns; ++column) {
      const bool dark = (row + column) % 2 == 0;
      tiles.push_back(Text(font::caption,
                           frame({.width = map_tile, .height = map_tile}),
                           background(dark ? ColourRole::secondary_system_fill
                                           : ColourRole::tertiary_system_fill,
                                      theme::Shape::rectangle()),
                           focusable())(
          std::format("{}{}", static_cast<char>('A' + column), row + 1)));
    }
    lines.push_back(Element{components::HStack(
        components::HStackOptions{.spacing = theme::space::none},
        std::move(tiles))});
  }
  return ScrollView(axes(Axes::all), id(map_view),
                    frame({.max_width = infinity, .max_height = infinity}),
                    scroll_indicators_flash(true),
                    scroll_deceleration_rate(ScrollDecelerationRate::fast()))(
      Element{components::VStack(
          components::VStackOptions{.spacing = theme::space::none},
          std::move(lines))});
}

auto ScrollViewsPage::rows(int count) -> Element {
  return VStack(spacing(theme::space::xs),
                frame({.max_width = infinity}))(ForEach(
      std::views::iota(0, count), [](int index) { return index; },
      [](int index) {
        return Text(font::body, frame({.height = 44.0F, .max_width = infinity}),
                    background(ColourRole::surface_raised,
                               RoundedRectangle(theme::radius::control)),
                    focusable())(std::format("Row {}", index + 1));
      }));
}

auto ScrollViewsPage::bounce() -> Element {
  const auto column = [](std::string title, Element body) {
    return VStack(alignment::leading, spacing(theme::space::sm),
                  frame({.max_width = infinity, .max_height = infinity}))(
        Text(font::headline)(std::move(title)), std::move(body));
  };
  return HStack(alignment::top, spacing(theme::space::lg),
                frame({.max_width = infinity, .max_height = infinity}))(
      column("Bounces", ScrollView(id(short_view),
                                   frame({.max_width = infinity,
                                          .max_height = infinity}))(rows(3))),
      column("Based on size",
             ScrollView(id(sized_view),
                        frame({.max_width = infinity, .max_height = infinity}),
                        scroll_bounce_behavior(
                            ScrollBounceBehavior::based_on_size))(rows(3))),
      column("Background", List(list_style(structure::InsetGroupedListStyle{}),
                                scroll_content_background(Visibility::visible))(
                               Text("One"), Text("Two"), Text("Three"))));
}

auto ScrollViewsPage::position(
    composition::StateHandle<composition::ScrollPosition> placed) -> Element {
  const composition::ScrollPosition current = placed.get();
  std::string shown = "Automatic";
  if (const auto row = current.view_id_as<int>()) {
    shown = std::format("Row {}", *row + 1);
  } else if (const auto point = current.point()) {
    shown = std::format("y {:.0f}", point->y);
  } else if (const auto edge = current.edge()) {
    shown = *edge == theme::Edge::top ? "Top" : "Bottom";
  }
  const auto go = [placed](auto change) {
    return [placed, change] {
      composition::ScrollPosition next = placed.get();
      change(next);
      placed.set(next);
    };
  };
  return VStack(alignment::leading, spacing(theme::space::sm),
                frame({.max_width = infinity, .max_height = infinity}))(
      HStack(spacing(theme::space::md))(
          Button(action(go([](composition::ScrollPosition& next) {
            next.scroll_to(theme::Edge::top);
          })))("Top"),
          Button(action(go([](composition::ScrollPosition& next) {
            next.scroll_to(composition::ElementKey{99});
          })))("Row 100"),
          Button(action(go([](composition::ScrollPosition& next) {
            next.scroll_to(theme::Edge::bottom);
          })))("Bottom"),
          Text(font::headline,
               foreground_style(current.is_positioned_by_user()
                                    ? ColourRole::accent
                                    : ColourRole::label))(shown)),
      ScrollView(
          id(position_view),
          frame({.max_width = infinity, .max_height = infinity}),
          scroll_position(
              composition::Binding<composition::ScrollPosition>{placed}),
          scroll_indicators(ScrollIndicatorVisibility::hidden))(LazyVStack(
          spacing(theme::space::xs), estimated_extent(44.0F),
          scroll_target_layout())(ForEach(
          std::views::iota(0, position_rows), [](int index) { return index; },
          [](int index) {
            return Text(font::body,
                        frame({.height = 44.0F, .max_width = infinity}),
                        background(ColourRole::surface_raised,
                                   RoundedRectangle(theme::radius::control)),
                        focusable())(std::format("Row {}", index + 1));
          }))));
}

auto ScrollViewsPage::body(composition::Context& context) const -> Element {
  const auto shown = shown_;
  const auto panel = context.state(panel_key);
  Element content = chat(context.state(messages_key));
  switch (panel.get()) {
    case Panel::chat:
      break;
    case Panel::carousel:
      content = carousel();
      break;
    case Panel::map:
      content = map();
      break;
    case Panel::bounce:
      content = bounce();
      break;
    case Panel::position:
      content = position(context.state(position_key));
      break;
  }
  return VStack(alignment::leading, spacing(theme::space::md),
                padding(theme::space::lg),
                frame({.max_width = infinity,
                       .max_height = infinity,
                       .alignment = composition::Alignment::top_leading}),
                background(ColourRole::system_grouped_background,
                           theme::Shape::rectangle()))(
      HStack(spacing(theme::space::md))(
          Button(id(close_button), button_style(ButtonStyle::bordered()),
                 action([shown] { shown.set(false); }))("Close"),
          controls::Picker(
              "Panel", panel,
              [] {
                return Element{components::Group(
                    components::Text("Chat").tag(Panel::chat),
                    components::Text("Carousel").tag(Panel::carousel),
                    components::Text("Map").tag(Panel::map),
                    components::Text("Bounce").tag(Panel::bounce),
                    components::Text("Position").tag(Panel::position))};
              })
              .picker_style(controls::SegmentedPickerStyle{})),
      std::move(content).frame(
          {.max_width = infinity, .max_height = infinity}));
}

}
