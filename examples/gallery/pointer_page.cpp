#include <pointer_page.hpp>

#include <arisa/components/button.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/custom_hover_effect.hpp>
#include <arisa/composition/frame_resize_directions.hpp>
#include <arisa/composition/frame_resize_position.hpp>
#include <arisa/composition/geometry_proxy.hpp>
#include <arisa/composition/horizontal_directions.hpp>
#include <arisa/composition/hover_effect_group.hpp>
#include <arisa/composition/hover_phase.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/toolbar_item.hpp>
#include <arisa/composition/vertical_directions.hpp>
#include <arisa/composition/visibility.hpp>
#include <arisa/composition/visual_effect.hpp>
#include <arisa/motion/hover_effect.hpp>
#include <arisa/presentation/navigation_stack.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/stroke.hpp>
#include <arisa/theme/typography.hpp>

#include <cmath>
#include <format>
#include <utility>
#include <vector>

namespace arisa::examples {

using composition::Element;
using composition::infinity;
using composition::PointerStyle;
using theme::ColourRole;

auto PointerPage::tile_key(std::string_view prefix, std::size_t index)
    -> std::string {
  return std::format("{}-{}", prefix, index);
}

auto PointerPage::style_of(std::size_t index) -> PointerStyle {
  switch (index) {
    case 1:
      return PointerStyle::horizontal_text;
    case 2:
      return PointerStyle::vertical_text;
    case 3:
      return PointerStyle::rect_selection;
    case 4:
      return PointerStyle::grab_idle;
    case 5:
      return PointerStyle::grab_active;
    case 6:
      return PointerStyle::link;
    case 7:
      return PointerStyle::zoom_in;
    case 8:
      return PointerStyle::zoom_out;
    case 9:
      return PointerStyle::column_resize();
    case 10:
      return PointerStyle::column_resize(
          composition::HorizontalDirections::leading);
    case 11:
      return PointerStyle::row_resize();
    case 12:
      return PointerStyle::row_resize(composition::VerticalDirections::up);
    case 13:
      return PointerStyle::frame_resize(composition::FrameResizePosition::top);
    case 14:
      return PointerStyle::frame_resize(
          composition::FrameResizePosition::bottom_trailing,
          composition::FrameResizeDirections::outward);
    case 15:
      return PointerStyle::image(
          [] {
            return Element{components::Image::system_name("star")
                               .font(theme::Font::title2)
                               .foreground_style(ColourRole::accent)};
          },
          motion::UnitPoint::center);
    default:
      return PointerStyle::default_;
  }
}

auto PointerPage::describe(const std::optional<composition::Point>& at)
    -> std::string {
  if (!at.has_value()) {
    return "Outside";
  }
  return std::format("{:.0f}, {:.0f}", std::floor(at->x), std::floor(at->y));
}

auto PointerPage::section(std::string_view title, Element content) -> Element {
  return components::VStack(
             components::VStackOptions{
                 .alignment = components::HorizontalAlignment::leading,
                 .spacing = theme::space::md},
             components::Text(std::string{title}).font(theme::Font::headline),
             std::move(content))
      .frame({.max_width = infinity,
              .alignment = composition::Alignment::leading});
}

auto PointerPage::chip(std::string_view label, std::string_view key)
    -> Element {
  return components::Button(std::string{label}, [] {})
      .frame({.width = 150.0F, .height = 64.0F})
      .id(std::string{key});
}

auto PointerPage::effects() -> Element {
  const std::array<composition::CustomHoverEffect, 5> chosen{
      composition::CustomHoverEffect{motion::HoverEffect::automatic},
      composition::CustomHoverEffect{motion::HoverEffect::highlight},
      composition::CustomHoverEffect{motion::HoverEffect::lift},
      composition::CustomHoverEffect::empty().hover_effect(
          [](composition::VisualEffect content, bool active,
             const composition::GeometryProxy&) {
            return content.scale_effect(active ? 1.08F : 1.0F)
                .offset(0.0F, active ? -4.0F : 0.0F);
          }),
      composition::CustomHoverEffect::empty()};
  std::vector<Element> cards;
  for (std::size_t index = 0; index < effect_names.size(); ++index) {
    cards.push_back(chip(effect_names[index], tile_key("pointer-effect", index))
                        .hover_effect(chosen[index]));
  }
  return components::HStack(
      components::HStackOptions{.spacing = theme::space::lg}, std::move(cards));
}

auto PointerPage::groups(composition::Namespace::ID space) -> Element {
  const composition::HoverEffectGroup leader{space};
  std::vector<Element> members;
  for (std::size_t index = 0; index < 3; ++index) {
    members.push_back(chip(std::format("Member {}", index + 1),
                           tile_key("pointer-member", index))
                          .hover_effect(motion::HoverEffect::lift));
  }
  return components::VStack(
      components::VStackOptions{
          .alignment = components::HorizontalAlignment::leading,
          .spacing = theme::space::lg},
      components::HStack(components::HStackOptions{.spacing = theme::space::md},
                         std::move(members))
          .padding(theme::space::md)
          .background(ColourRole::surface_raised,
                      theme::Shape::rounded(theme::radius::card, true))
          .hover_effect_group()
          .id(std::string{group_row}),
      components::HStack(
          components::HStackOptions{.spacing = theme::space::md},
          chip("Leader", tile_key("pointer-linked", 0))
              .hover_effect(motion::HoverEffect::highlight, leader),
          chip("Follower", tile_key("pointer-linked", 1))
              .hover_effect(motion::HoverEffect::highlight,
                            leader.behavior(composition::HoverEffectGroup::
                                                Behavior::follows_group))));
}

auto PointerPage::styles() -> Element {
  std::vector<Element> rows;
  std::vector<Element> row;
  for (std::size_t index = 0; index < style_names.size(); ++index) {
    Element tile = components::Button(std::string{style_names[index]}, [] {})
                       .frame({.width = 150.0F, .height = 56.0F})
                       .pointer_style(style_of(index))
                       .id(tile_key("pointer-style", index));
    if (index == hidden_index) {
      tile =
          std::move(tile).pointer_visibility(composition::Visibility::hidden);
    }
    row.push_back(std::move(tile));
    if (row.size() == 6 || index + 1 == style_names.size()) {
      rows.push_back(components::HStack(
          components::HStackOptions{.spacing = theme::space::md},
          std::move(row)));
      row.clear();
    }
  }
  return components::VStack(
      components::VStackOptions{
          .alignment = components::HorizontalAlignment::leading,
          .spacing = theme::space::md},
      std::move(rows));
}

auto PointerPage::tracking(
    composition::StateHandle<bool> within,
    composition::StateHandle<std::optional<composition::Point>> where)
    -> Element {
  return components::HStack(
      components::HStackOptions{.spacing = theme::space::lg},
      components::ZStack(components::Text(within.get() ? "Inside" : "Outside")
                             .font(theme::Font::headline))
          .frame({.width = 320.0F, .height = 120.0F})
          .background(ColourRole::surface_raised,
                      theme::Shape::rounded(theme::radius::card, true))
          .border(ColourRole::separator, theme::Stroke::hairline())
          .on_hover([within](bool hovering) { within.set(hovering); })
          .on_continuous_hover([where](composition::HoverPhase phase) {
            where.set(phase.location());
          })
          .id(std::string{hover_area}),
      components::Text(describe(where.get()))
          .font(theme::Font::title2)
          .id(std::string{readout}));
}

auto PointerPage::wheel() -> Element {
  std::vector<Element> rows;
  for (std::size_t index = 0; index < row_count; ++index) {
    rows.push_back(components::Text(std::format("Row {}", index + 1))
                       .frame({.height = 40.0F,
                               .max_width = infinity,
                               .alignment = composition::Alignment::leading})
                       .padding(theme::Edge::horizontal, theme::space::md));
  }
  return components::ScrollView(
             components::VStack(
                 components::VStackOptions{
                     .alignment = components::HorizontalAlignment::leading,
                     .spacing = 0.0F},
                 std::move(rows)))
      .frame({.width = 320.0F, .height = 200.0F})
      .background(ColourRole::surface_raised,
                  theme::Shape::rounded(theme::radius::card, true))
      .id(std::string{wheel_list});
}

auto PointerPage::body(composition::Context& context) const -> Element {
  const auto shown_flag = shown_;
  const composition::Namespace::ID space = context.namespace_id(linked);
  const auto within = context.state(inside);
  const auto where = context.state(location);
  return presentation::NavigationStack([shown_flag, space, within, where] {
    return components::ScrollView(
               components::VStack(
                   components::VStackOptions{
                       .alignment = components::HorizontalAlignment::leading,
                       .spacing = theme::space::xl},
                   section("Hover effects", effects()),
                   section("Groups", groups(space)),
                   section("Pointer styles", styles()),
                   section("Hover", tracking(within, where)),
                   section("Wheel", wheel()))
                   .padding(theme::space::xl)
                   .frame({.max_width = infinity,
                           .alignment = composition::Alignment::leading}))
        .frame({.max_width = infinity, .max_height = infinity})
        .navigation_title("Pointer")
        .toolbar(composition::ToolbarItem(
            composition::ToolbarItemPlacement::cancellation_action,
            components::Button("Close", [shown_flag] {
              shown_flag.set(false);
            }).id(std::string{close_button})));
  });
}

auto PointerPage::Opener::body(composition::Context& context) const -> Element {
  const auto shown = context.state(shown_key);
  return components::Button("Pointer", [shown] { shown.set(true); })
      .id(std::string{button})
      .full_screen_cover(shown,
                         [shown] { return Element{PointerPage{shown}}; });
}

}
