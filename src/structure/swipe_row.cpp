#include <arisa/components/button.hpp>
#include <arisa/components/command_mapping.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/drag_gesture.hpp>
#include <arisa/composition/geometry_proxy.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/list_environment.hpp>
#include <arisa/composition/tint.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/structure/swipe_row.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/typography.hpp>

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>

namespace arisa::structure {

SwipeRow::SwipeRow(composition::Element body,
                   composition::ListRow::SwipeActions leading,
                   composition::ListRow::SwipeActions trailing)
    : Element(std::make_unique<Content>(
          std::make_shared<composition::PlaceholderContentView::Source>(
              std::move(body)),
          std::move(leading), std::move(trailing))) {}

const SwipeRow::Host SwipeRow::Host::shared{};

auto SwipeRow::host() noexcept -> const composition::SwipeHost* {
  return &Host::shared;
}

auto SwipeRow::hosted(composition::Element row,
                      const composition::ListRow& attributes)
    -> composition::Element {
  const std::optional<composition::ElementKey> key = row.key();
  composition::Element wrapped =
      composition::Element{
          SwipeRow(std::move(row), attributes.leading, attributes.trailing)}
          .environment(composition::ListEnvironment::swipe_actions_container,
                       false);
  if (key.has_value()) {
    wrapped = std::move(wrapped).id(*key);
  }
  return wrapped;
}

auto SwipeRow::Host::wrap(composition::Element row,
                          const composition::ListRow& attributes) const
    -> composition::Element {
  return hosted(std::move(row), attributes);
}

auto SwipeRow::ActionStyle::make_body(
    components::ButtonStyle::Configuration configuration) const
    -> composition::Element {
  const bool destructive =
      configuration.role == components::ButtonRole::destructive;
  composition::Element tile =
      components::VStack(std::move(configuration.label))
          .font(theme::Font::subheadline)
          .foreground_style(theme::ColourRole::on_accent)
          .frame({.width = action_width, .max_height = composition::infinity})
          .background(destructive
                          ? theme::ColourValue{theme::ColourRole::red}
                          : theme::ColourValue{theme::ColourRole::accent});
  if (configuration.is_pressed || configuration.is_focused) {
    tile = std::move(tile).opacity(
        configuration.is_pressed
            ? components::ButtonStyle::prominent_pressed_opacity
            : 0.85F);
  }
  return tile;
}

auto SwipeRow::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto SwipeRow::Content::edge_of(Reveal reveal) const noexcept
    -> const composition::ListRow::SwipeActions& {
  return reveal == Reveal::leading ? leading_ : trailing_;
}

auto SwipeRow::Content::opening() const noexcept -> Reveal {
  if (trailing_.content) {
    return Reveal::trailing;
  }
  return leading_.content ? Reveal::leading : Reveal::closed;
}

auto SwipeRow::Content::actions(composition::HorizontalEdge edge) const
    -> std::vector<composition::Element> {
  return members_of(edge == composition::HorizontalEdge::leading ? leading_
                                                                 : trailing_);
}

auto SwipeRow::Content::members_of(
    const composition::ListRow::SwipeActions& side)
    -> std::vector<composition::Element> {
  if (!side.content) {
    return {};
  }
  return components::Group::members(side.content());
}

void SwipeRow::Content::perform(std::vector<composition::Element>& members,
                                std::size_t index) {
  std::size_t seen = 0;
  for (composition::Element& member : members) {
    if (const auto button = components::Button::content(member)) {
      if (seen++ == index) {
        static_cast<void>(button->get().activate());
        return;
      }
    }
  }
}

auto SwipeRow::Content::label_of(std::vector<composition::Element>& members,
                                 std::size_t index) -> std::string {
  std::size_t seen = 0;
  for (composition::Element& member : members) {
    if (const auto button = components::Button::content(member)) {
      if (seen++ == index) {
        return std::string{button->get().label()};
      }
    }
  }
  return std::string{components::CommandMapping::select_label};
}

auto SwipeRow::Content::strip(Reveal reveal,
                              composition::StateHandle<Reveal> open,
                              std::optional<std::size_t> highlighted,
                              theme::Colour untinted) const
    -> composition::Element {
  std::vector<composition::Element> members = actions(
      reveal == Reveal::leading ? composition::HorizontalEdge::leading
                                : composition::HorizontalEdge::trailing);
  std::vector<composition::Element> tiles;
  if (reveal == Reveal::trailing) {
    tiles.push_back(components::Spacer().id("arisa.swipe.spacer"));
  }
  int index = 0;
  for (composition::Element& member : members) {
    if (const auto button = components::Button::content(member)) {
      button->get().then([open] {
        motion::with_animation(motion::Animation::snappy(0.25),
                               [&open] { open.set(Reveal::closed); });
      });
    }
    composition::Element tile =
        std::move(member)
            .transform_environment(
                composition::Tint::key,
                [untinted](std::optional<graphics::ShapeStyle>& tint) {
                  tint = untinted;
                })
            .button_style(ActionStyle{})
            .focusable(false);
    if (highlighted == static_cast<std::size_t>(index)) {
      tile = std::move(tile).overlay(
          components::ZStack()
              .frame({.max_width = composition::infinity,
                      .max_height = composition::infinity})
              .border(theme::ColourRole::on_accent, 2.0F)
              .allows_hit_testing(false));
    }
    tiles.push_back(std::move(tile).id(static_cast<std::int64_t>(index)));
    ++index;
  }
  if (reveal == Reveal::leading) {
    tiles.push_back(components::Spacer().id("arisa.swipe.spacer"));
  }
  return components::HStack(
             components::HStackOptions{
                 .alignment = components::VerticalAlignment::center,
                 .spacing = theme::space::none},
             std::move(tiles))
      .frame({.max_width = composition::infinity,
              .max_height = composition::infinity})
      .focus_section()
      .id("arisa.swipe.actions");
}

auto SwipeRow::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  const auto open = context.state(revealed);
  const auto drag = context.state(dragged);
  const auto wide = context.state(width);
  const auto picked = context.state(chosen);
  const Reveal shown = open.get();
  float shift = drag.get();
  Reveal visible = shown;
  if (shown == Reveal::closed && shift != 0.0F) {
    visible = shift > 0.0F ? Reveal::leading : Reveal::trailing;
    if (!edge_of(visible).content) {
      visible = Reveal::closed;
      shift = 0.0F;
    }
  }
  std::optional<composition::Element> behind;
  if (visible != Reveal::closed) {
    const auto count =
        static_cast<float>(actions(visible == Reveal::leading
                                       ? composition::HorizontalEdge::leading
                                       : composition::HorizontalEdge::trailing)
                               .size());
    if (shown != Reveal::closed) {
      shift += (shown == Reveal::leading ? 1.0F : -1.0F) * count * action_width;
    }
    const theme::Theme& current =
        context.environment(components::Environment::theme);
    behind =
        strip(visible, open,
              shown != Reveal::closed ? std::optional<std::size_t>{picked.get()}
                                      : std::nullopt,
              current.colour(theme::ColourRole::gray, current.scheme()));
  }
  composition::Element front =
      composition::Element{composition::PlaceholderContentView(body_)}
          .frame({.max_width = composition::infinity})
          .id("arisa.swipe.body");
  if (shift != 0.0F) {
    front = std::move(front).offset(shift, 0.0F);
  }
  composition::Element stacked =
      components::VStack(std::move(front))
          .frame({.max_width = composition::infinity});
  stacked = std::move(stacked).background(
      composition::Alignment::center,
      behind.has_value() ? std::move(*behind)
                         : composition::Element{
                               components::HStack().id("arisa.swipe.actions")});
  const auto close = [open] {
    motion::with_animation(motion::Animation::snappy(0.25),
                           [&open] { open.set(Reveal::closed); });
  };
  const Reveal opens = opening();
  composition::Element row =
      std::move(stacked)
          .clip_shape(theme::Shape::rectangle())
          .on_geometry_change(
              std::type_identity<float>{},
              [](const composition::GeometryProxy& proxy) {
                return proxy.size().width;
              },
              [wide](float value) { wide.set(value); })
          .simultaneous_gesture(
              composition::DragGesture(12.0F)
                  .on_changed(
                      [drag,
                       open](const composition::DragGesture::Value& value) {
                        if (std::abs(value.translation.width) <
                            std::abs(value.translation.height)) {
                          return;
                        }
                        if (open.get() != Reveal::closed) {
                          return;
                        }
                        drag.set(value.translation.width);
                      })
                  .on_ended([leading = leading_, trailing = trailing_, drag,
                             open, wide, close](
                                const composition::DragGesture::Value& value) {
                    const float moved = drag.get();
                    if (open.get() != Reveal::closed || moved == 0.0F) {
                      if (open.get() != Reveal::closed &&
                          std::abs(value.translation.width) >
                              std::abs(value.translation.height)) {
                        close();
                      }
                      return;
                    }
                    const Reveal side =
                        moved > 0.0F ? Reveal::leading : Reveal::trailing;
                    const composition::ListRow::SwipeActions& edge =
                        side == Reveal::leading ? leading : trailing;
                    const float extent = std::max(wide.get(), 1.0F);
                    if (edge.content && edge.allows_full_swipe &&
                        std::abs(moved) >= extent * full_swipe_fraction) {
                      std::vector<composition::Element> members =
                          members_of(edge);
                      drag.set(0.0F);
                      perform(members, 0);
                      return;
                    }
                    motion::with_animation(
                        motion::Animation::snappy(0.25), [&] {
                          drag.set(0.0F);
                          if (edge.content &&
                              std::abs(moved) >= action_width * open_fraction) {
                            open.set(side);
                          }
                        });
                  }));
  if (shown == Reveal::closed) {
    if (opens != Reveal::closed) {
      row = std::move(row).on_command(
          components::CommandMapping::actions,
          std::string{components::CommandMapping::actions_label},
          [open, opens, picked] {
            picked.set(0);
            motion::with_animation(motion::Animation::snappy(0.25),
                                   [&open, opens] { open.set(opens); });
          });
    }
  } else {
    std::vector<composition::Element> members = members_of(edge_of(shown));
    const std::size_t count = std::max<std::size_t>(members.size(), 1);
    const std::size_t at = std::min(picked.get(), count - 1);
    row = std::move(row)
              .on_command(
                  components::CommandMapping::select, label_of(members, at),
                  [side = edge_of(shown), at, close] {
                    std::vector<composition::Element> fresh = members_of(side);
                    perform(fresh, at);
                    close();
                  })
              .on_move_command(
                  [picked, count](composition::MoveCommandDirection direction) {
                    const std::size_t now = picked.get();
                    if (direction == composition::MoveCommandDirection::left &&
                        now > 0) {
                      picked.set(now - 1);
                    } else if (direction ==
                                   composition::MoveCommandDirection::right &&
                               now + 1 < count) {
                      picked.set(now + 1);
                    }
                  },
                  composition::Axis::horizontal)
              .on_command(components::CommandMapping::actions,
                          std::string{components::CommandMapping::close_label},
                          close)
              .on_exit_command(
                  std::string{components::CommandMapping::close_label}, close);
  }
  std::vector<composition::Element> produced;
  produced.push_back(std::move(row));
  return produced;
}

}
