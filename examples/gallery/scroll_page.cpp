#include <scroll_page.hpp>
#include <scroll_views_page.hpp>
#include <search_page.hpp>

#include <arisa/composition/length.hpp>
#include <arisa/markup/markup.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/stroke.hpp>

#include <cmath>
#include <format>
#include <ranges>
#include <string>
#include <utility>

namespace arisa::examples {

using namespace markup;
using composition::Element;
using composition::infinity;
using theme::ColourRole;
using theme::RoundedCornerStyle;
using theme::RoundedRectangle;

auto ScrollPage::heading(std::string_view title) -> Element {
  return Text(font::headline)(std::string{title});
}

auto ScrollPage::body(composition::Context& context) const -> Element {
  const auto position = context.state(dragged);
  const auto pressed = context.state(holding);
  const auto count = context.state(holds);
  const auto tapped = context.state(taps);
  const auto search = context.state(searching);
  const auto modifiers = context.state(views);
  return HStack(alignment::top, spacing(theme::space::xl),
                frame({.max_width = infinity,
                       .max_height = infinity,
                       .alignment = composition::Alignment::top}))(
      list().dynamic_type_size(
          theme::DynamicTypeRange::up_to(theme::DynamicTypeSize::x_large)),
      ScrollView(frame({.max_width = infinity, .max_height = infinity}))(
          VStack(alignment::leading, spacing(theme::space::lg),
                 frame({.max_width = infinity,
                        .alignment = composition::Alignment::leading}))(
              carousel(), heading("Gestures"),
              HStack(alignment::top, spacing(theme::space::lg))(
                  card(position), hold(pressed, count), tapping(tapped)),
              more(search, modifiers))));
}

auto ScrollPage::more(composition::StateHandle<bool> search,
                      composition::StateHandle<bool> modifiers) -> Element {
  return VStack(alignment::leading, spacing(theme::space::sm))(
      heading("More"),
      HStack(spacing(theme::space::md))(
          Button(id(search_button), action([search] { search.set(true); }),
                 full_screen_cover(
                     search, [search] { return Element{SearchPage{search}}; }))(
              "Search"),
          Button(id(views_button), action([modifiers] { modifiers.set(true); }),
                 full_screen_cover(modifiers, [modifiers] {
                   return Element{ScrollViewsPage{modifiers}};
                 }))("Scroll views")));
}

auto ScrollPage::list() -> Element {
  return ScrollViewReader([](ScrollViewProxy proxy) -> Element {
    return VStack(alignment::leading, spacing(theme::space::md),
                  frame({.width = 340.0F,
                         .max_height = infinity,
                         .alignment = composition::Alignment::leading}))(
        HStack(alignment::center, spacing(theme::space::md),
               frame({.max_width = infinity}))(
            heading("1,000 rows"), Spacer(),
            Button(action([proxy] {
                     with_animation(Animation::smooth(0.6), [&proxy] {
                       proxy.scroll_to(jump_row, UnitPoint::center);
                     });
                   }),
                   id(jump_button))("Jump to 500")),
        ScrollView(id(list_view),
                   frame({.max_width = infinity, .max_height = infinity}),
                   content_margins(theme::space::sm))(LazyVStack(
            spacing(theme::space::xs), estimated_extent(40.0F))(ForEach(
            std::views::iota(0, row_count), [](int index) { return index; },
            [](int index) { return row(index); }))));
  });
}

auto ScrollPage::row(int index) -> Element {
  return HStack(
      alignment::center, spacing(theme::space::sm),
      padding(theme::Edge::horizontal, theme::space::md),
      frame({.height = 40.0F, .max_width = infinity}),
      background(ColourRole::surface_raised,
                 RoundedRectangle(theme::radius::control,
                                  RoundedCornerStyle::continuous)),
      focusable(),
      scroll_transition([](VisualEffect content, ScrollTransitionPhase phase) {
        const auto amount = static_cast<float>(std::abs(phase.value()));
        return content.scale_effect(1.0F - (0.08F * amount))
            .opacity(1.0F - (0.7F * amount));
      }))(Text(font::body)(std::format("Row {}", index + 1)), Spacer(),
          Text(font::caption, foreground_style(ColourRole::text_secondary))(
              std::format("#{:04}", index)));
}

auto ScrollPage::carousel() -> Element {
  return VStack(alignment::leading, spacing(theme::space::sm),
                frame({.max_width = infinity,
                       .alignment = composition::Alignment::leading}))(
      heading("Carousel"),
      ScrollView(axes(Axes::horizontal), id(carousel_view),
                 frame({.max_width = infinity}),
                 scroll_target_behavior(ScrollTargetBehavior::view_aligned),
                 content_margins(theme::Edge::horizontal, theme::space::sm),
                 content_margins(theme::Edge::vertical, theme::space::xs))(
          HStack(spacing(theme::space::md), scroll_target_layout())(ForEach(
              std::views::iota(0, card_count),
              [](int index) { return std::format("card-{}", index); },
              [](int index) {
                return Button(action([index] { static_cast<void>(index); }),
                              frame({.width = 170.0F, .height = 96.0F}))(
                    std::format("Card {}", index + 1));
              }))));
}

auto ScrollPage::card(composition::StateHandle<composition::Point> position)
    -> Element {
  const composition::Point at = position.get();
  return VStack(
      alignment::center, justify::center, id(drag_card),
      frame({.width = 150.0F, .height = 110.0F}),
      background(ColourRole::accent,
                 RoundedRectangle(theme::radius::card,
                                  RoundedCornerStyle::continuous)),
      shadow(theme::Elevation::medium), offset(at.x, at.y),
      gesture(DragGesture()
                  .on_changed([position](const DragGesture::Value& value) {
                    position.set(
                        composition::Point{.x = value.translation.width,
                                           .y = value.translation.height});
                  })
                  .on_ended([position](const DragGesture::Value&) {
                    with_animation(Animation::spring(0.5, 0.62), [&position] {
                      position.set(composition::Point{});
                    });
                  })))(
      Text(font::headline, foreground_style(ColourRole::on_accent))("Drag me"));
}

auto ScrollPage::hold(composition::StateHandle<bool> pressed,
                      composition::StateHandle<int> count) -> Element {
  return VStack(alignment::center, justify::center, spacing(theme::space::xs),
                id(hold_tile), frame({.width = 150.0F, .height = 110.0F}),
                background(pressed.get() ? ColourRole::accent
                                         : ColourRole::surface_raised,
                           RoundedRectangle(theme::radius::card,
                                            RoundedCornerStyle::continuous)),
                border(ColourRole::separator, theme::Stroke::hairline()),
                shadow(theme::Elevation::low),
                scale_effect(pressed.get() ? 0.94F : 1.0F),
                animation(Animation::snappy(), pressed.get()),
                on_long_press_gesture(
                    0.6, [count] { count.update([](int& value) { ++value; }); },
                    [pressed](bool down) { pressed.set(down); }))(
      Text(font::headline)("Hold me"),
      Text(font::caption, foreground_style(ColourRole::text_secondary))(
          std::format("held {}", count.get())));
}

auto ScrollPage::tapping(composition::StateHandle<int> count) -> Element {
  return VStack(alignment::center, justify::center, spacing(theme::space::xs),
                id(tap_tile), frame({.width = 150.0F, .height = 110.0F}),
                background(ColourRole::surface_raised,
                           RoundedRectangle(theme::radius::card,
                                            RoundedCornerStyle::continuous)),
                border(ColourRole::separator, theme::Stroke::hairline()),
                shadow(theme::Elevation::low),
                content_shape(RoundedRectangle(theme::radius::card)),
                on_tap_gesture(
                    2, [count] { count.update([](int& value) { ++value; }); }))(
      Text(font::headline)("Double-tap"),
      Text(font::caption, foreground_style(ColourRole::text_secondary))(
          std::format("taps {}", count.get())));
}

}
