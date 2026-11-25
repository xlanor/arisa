#include <custom_layout_page.hpp>
#include <geometry_page.hpp>
#include <layout_scroll_page.hpp>

#include <arisa/composition/length.hpp>
#include <arisa/markup/markup.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/space.hpp>

#include <array>
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

auto GeometryPage::badge_key(int index) -> std::string {
  return std::format("{}{}", badge_prefix, index);
}

auto GeometryPage::section(std::string_view title, Element content) -> Element {
  return VStack(alignment::leading, spacing(theme::space::sm),
                padding(theme::space::md),
                frame({.max_width = infinity,
                       .alignment = composition::Alignment::leading}),
                background(ColourRole::surface,
                           RoundedRectangle(theme::radius::card,
                                            RoundedCornerStyle::continuous)))(
      Text(font::headline)(std::string{title}), std::move(content));
}

auto GeometryPage::carousel() -> Element {
  return ScrollView(axes(Axes::horizontal), id(carousel_view),
                    frame({.max_width = infinity}),
                    content_margins(theme::Edge::horizontal, theme::space::sm))(
      HStack(spacing(theme::space::md))(ForEach(
          std::views::iota(0, card_count), [](int index) { return index; },
          [](int index) {
            return Text(
                font::title3, frame({.height = 120.0F}),
                container_relative_frame(Axes::horizontal, cards_per_page, 1,
                                         theme::space::md),
                background(ColourRole::surface_raised,
                           RoundedRectangle(theme::radius::card,
                                            RoundedCornerStyle::continuous)),
                focusable(), id(std::format("geometry-card-{}", index)))(
                std::format("Card {}", index + 1));
          })));
}

auto GeometryPage::badges(composition::StateHandle<int> raised) -> Element {
  constexpr std::array colours{ColourRole::accent, ColourRole::success,
                               ColourRole::warning, ColourRole::destructive};
  constexpr std::array letters{"A", "B", "C", "D"};
  std::vector<Element> stacked;
  stacked.reserve(badge_count);
  for (int index = 0; index < badge_count; ++index) {
    const auto at = static_cast<std::size_t>(index);
    stacked.push_back(
        Button(id(badge_key(index)), button_style(ButtonStyle::plain()),
               action([raised, index] { raised.set(index); }),
               z_index(raised.get() == index ? 1.0 : 0.0))(
            Text(font::title3, foreground_style(ColourRole::on_accent),
                 frame({.width = badge_size, .height = badge_size}),
                 background(colours.at(at), theme::Circle()),
                 border(ColourRole::surface, theme::Stroke{3.0F}))(
                letters.at(at)))
            .offset((badge_step - badge_size) * static_cast<float>(index)));
  }
  return HStack(spacing(theme::space::none))(std::move(stacked));
}

auto GeometryPage::price_row(std::string_view title, std::string_view price,
                             std::string_view key, bool kept) -> Element {
  Element amount =
      Text(font::body, line_limit(1), id(std::string{key}),
           foreground_style(ColourRole::accent))(std::string{price});
  if (kept) {
    amount = std::move(amount).layout_priority(1.0);
  }
  return HStack(spacing(theme::space::sm), padding(theme::space::sm),
                frame({.width = row_width}),
                background(ColourRole::surface_raised,
                           RoundedRectangle(theme::radius::control)))(
      Text(font::body, line_limit(1))(std::string{title}), std::move(amount));
}

auto GeometryPage::prices() -> Element {
  return VStack(alignment::leading, spacing(theme::space::sm))(
      price_row("Kingdom of the Lost Lantern", "Deluxe Edition with soundtrack",
                kept_price, true),
      price_row("Kingdom of the Lost Lantern", "Deluxe Edition with soundtrack",
                plain_price, false));
}

auto GeometryPage::fixed() -> Element {
  return HStack(alignment::top, spacing(theme::space::md))(
      VStack(alignment::leading, spacing(theme::space::xs),
             frame({.width = 90.0F}))(
          Text(font::caption, fixed_size(), id(std::string{fixed_tag}),
               padding(theme::Insets::symmetric(theme::space::sm,
                                                theme::space::xxs)),
               background(ColourRole::accent, theme::Capsule()),
               foreground_style(ColourRole::on_accent))("Multiplayer online"),
          Text(font::caption,
               padding(theme::Insets::symmetric(theme::space::sm,
                                                theme::space::xxs)),
               background(ColourRole::secondary_system_fill, theme::Capsule()))(
              "Multiplayer online")),
      Text(font::body, fixed_size(false, true), frame({.width = 160.0F}))(
          "Fixed vertically, this label wraps to every line it needs"));
}

auto GeometryPage::pad(composition::StateHandle<composition::Point> dragged)
    -> Element {
  const composition::Point at = dragged.get();
  return VStack(alignment::leading, spacing(theme::space::xs))(
      Text(font::caption, id(std::string{drag_label}))(
          std::format("x {:.0f}  y {:.0f}", at.x, at.y)),
      ZStack(id(std::string{pad_view}),
             frame({.width = pad_width, .height = pad_height}),
             background(ColourRole::secondary_system_fill,
                        RoundedRectangle(theme::radius::control)),
             coordinate_space(std::string{pad_space}))(
          VStack(id(std::string{knob}),
                 frame({.width = knob_size, .height = knob_size}),
                 background(ColourRole::accent, theme::Circle()),
                 gesture(
                     composition::DragGesture(
                         0.0F, composition::CoordinateSpace::named(
                                   std::string{pad_space}))
                         .on_changed(
                             [dragged](
                                 const composition::DragGesture::Value& value) {
                               dragged.set(composition::Point{
                                   .x = std::clamp(value.location.x, 0.0F,
                                                   pad_width),
                                   .y = std::clamp(value.location.y, 0.0F,
                                                   pad_height)});
                             })))()
              .position(at.x, at.y)));
}

auto GeometryPage::reader() -> Element {
  return GeometryReader(id(std::string{reader_view}),
                        frame({.height = 56.0F, .max_width = infinity}))(
      [](const composition::GeometryProxy& proxy) -> Element {
        const composition::Size size = proxy.size();
        return VStack(alignment::leading, spacing(theme::space::xs))(
            Text(font::caption, id(std::string{reader_label}))(
                std::format("{:.0f} pt", size.width)),
            VStack(frame({.width = std::max(size.width * 0.5F, 0.0F),
                          .height = 12.0F}),
                   background(ColourRole::accent, theme::Capsule()))());
      });
}

auto GeometryPage::body(composition::Context& context) const -> Element {
  const auto shown = shown_;
  const auto raised = context.state(raised_key);
  const auto dragged = context.state(drag_key);
  return ScrollView(frame({.max_width = infinity, .max_height = infinity}),
                    background(ColourRole::system_grouped_background,
                               theme::Shape::rectangle()))(
      VStack(alignment::leading, spacing(theme::space::md),
             padding(theme::space::lg),
             frame({.max_width = infinity,
                    .alignment = composition::Alignment::top_leading}))(
          HStack(spacing(theme::space::sm))(
              Button(id(std::string{close_button}),
                     button_style(ButtonStyle::bordered()),
                     action([shown] { shown.set(false); }))("Close"),
              CustomLayoutPage::Link{}, LayoutScrollPage::Link{}),
          section("Carousel", carousel()),
          HStack(alignment::top, spacing(theme::space::md),
                 frame({.max_width = infinity,
                        .alignment = composition::Alignment::leading}))(
              section("Badges", badges(raised)), section("Prices", prices()),
              section("Fixed size", fixed())),
          HStack(alignment::top, spacing(theme::space::md),
                 frame({.max_width = infinity,
                        .alignment = composition::Alignment::leading}))(
              section("Named space", pad(dragged)),
              section("Reader", reader()))));
}

auto GeometryPage::Link::body(composition::Context& context) const -> Element {
  const auto shown = context.state(shown_key);
  return Button(id(std::string{button}), action([shown] { shown.set(true); }),
                full_screen_cover(shown, [shown] {
                  return Element{GeometryPage{shown}};
                }))("Geometry");
}

}
