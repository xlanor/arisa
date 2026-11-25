#include <glass_preview.hpp>

#include <arisa/components/alignment.hpp>
#include <arisa/components/glass_effect_container.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/shape_view.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/coordinate_space.hpp>
#include <arisa/composition/drag_gesture.hpp>
#include <arisa/composition/element_key.hpp>
#include <arisa/composition/geometry_proxy.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/graphics/gradient.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/space.hpp>
#include <glass_scene.hpp>

#include <algorithm>
#include <cstddef>
#include <type_traits>
#include <vector>

namespace arisa::examples {

auto GlassPreview::glass(const Look& look) -> theme::Glass {
  theme::Glass chosen = theme::Glass::regular();
  switch (look.variant) {
    case Variant::regular:
      break;
    case Variant::clear:
      chosen = theme::Glass::clear();
      break;
    case Variant::identity:
      chosen = theme::Glass::identity();
      break;
  }
  return chosen.tint(tint_colour(look.tint)).interactive(look.interactive);
}

auto GlassPreview::tint_colour(Tint tint) -> std::optional<theme::ColourValue> {
  const auto soft = [](theme::ColourRole role) {
    return theme::ColourValue{role}.opacity(static_cast<float>(soft_opacity));
  };
  switch (tint) {
    case Tint::none:
      return std::nullopt;
    case Tint::accent:
      return theme::ColourValue{theme::ColourRole::accent};
    case Tint::accent_soft:
      return soft(theme::ColourRole::accent);
    case Tint::red:
      return theme::ColourValue{theme::ColourRole::red};
    case Tint::red_soft:
      return soft(theme::ColourRole::red);
    case Tint::green:
      return theme::ColourValue{theme::ColourRole::green};
    case Tint::green_soft:
      return soft(theme::ColourRole::green);
    case Tint::orange:
      return theme::ColourValue{theme::ColourRole::orange};
    case Tint::orange_soft:
      return soft(theme::ColourRole::orange);
    case Tint::purple:
      return theme::ColourValue{theme::ColourRole::purple};
    case Tint::purple_soft:
      return soft(theme::ColourRole::purple);
  }
  return std::nullopt;
}

auto GlassPreview::clamped(composition::Point point, composition::Size bounds)
    -> composition::Point {
  if (bounds.width <= 0.0F || bounds.height <= 0.0F) {
    return point;
  }
  const float across = bounds.width * 0.5F;
  const float down = bounds.height * 0.5F;
  return composition::Point{.x = std::clamp(point.x, -across, across),
                            .y = std::clamp(point.y, -down, down)};
}

auto GlassPreview::nudged(composition::Point point,
                          composition::MoveCommandDirection direction,
                          composition::Size bounds) -> composition::Point {
  switch (direction) {
    case composition::MoveCommandDirection::up:
      point.y -= nudge;
      break;
    case composition::MoveCommandDirection::down:
      point.y += nudge;
      break;
    case composition::MoveCommandDirection::left:
      point.x -= nudge;
      break;
    case composition::MoveCommandDirection::right:
      point.x += nudge;
      break;
  }
  return clamped(point, bounds);
}

auto GlassPreview::extent(const Look& look) -> composition::Size {
  composition::Size base;
  switch (look.size) {
    case Size::small:
      base = {.width = 132.0F, .height = 48.0F};
      break;
    case Size::medium:
      base = {.width = 240.0F, .height = 120.0F};
      break;
    case Size::large:
      base = {.width = row_width, .height = 260.0F};
      break;
  }
  if (look.form == Form::panel) {
    base.height = base.width * 0.66F;
  }
  if (look.grouping != Grouping::single) {
    const auto count = static_cast<float>(std::max(look.count, 1));
    base.width =
        std::min(base.width, (row_width - (gap * (count - 1.0F))) / count);
  }
  if (look.form == Form::circle) {
    const float diameter = std::min(base.width, base.height);
    base = {.width = diameter, .height = diameter};
  }
  return base;
}

auto GlassPreview::outline(Form form, Size size) -> theme::Shape {
  switch (form) {
    case Form::capsule:
    case Form::circle:
      return theme::Shape::capsule();
    case Form::rounded:
      switch (size) {
        case Size::small:
          return theme::RoundedRectangle(12.0F,
                                         theme::RoundedCornerStyle::continuous);
        case Size::medium:
          return theme::RoundedRectangle(20.0F,
                                         theme::RoundedCornerStyle::continuous);
        case Size::large:
          return theme::RoundedRectangle(28.0F,
                                         theme::RoundedCornerStyle::continuous);
      }
      break;
    case Form::panel:
      return theme::RoundedRectangle(theme::radius::sheet,
                                     theme::RoundedCornerStyle::continuous);
  }
  return theme::Shape::capsule();
}

auto GlassPreview::scenery(const Photo& photo) -> composition::Element {
  if (photo != nullptr) {
    return composition::Element{
        components::Image::Raster(photo, components::Image::ContentMode::fill)}
        .frame({.max_width = composition::infinity,
                .max_height = composition::infinity});
  }
  return components::ZStack(
      components::ZStackOptions{.alignment =
                                    components::Alignment::top_leading},
      components::ShapeView(theme::Shape::rectangle())
          .fill(graphics::ShapeStyle{graphics::LinearGradient{
              .gradient = graphics::Gradient{std::vector<theme::ColourValue>{
                  theme::Colour::hex(sky_top), theme::Colour::hex(sky_bottom),
                  theme::Colour::hex(ground)}},
              .start_point = motion::UnitPoint::top,
              .end_point = motion::UnitPoint::bottom}}),
      GlassScene{});
}

auto GlassPreview::piece(std::size_t index, const Handles& handles,
                         std::optional<composition::Namespace::ID> space) const
    -> composition::Element {
  const composition::Size size = extent(look_);
  const composition::StateHandle<composition::Point> placement = handles.placed;
  const composition::Size bounds = handles.bounds;
  composition::Element shape =
      components::ZStack()
          .frame({.width = size.width, .height = size.height})
          .glass_effect(glass(look_), outline(look_.form, look_.size));
  if (space.has_value()) {
    shape = std::move(shape).glass_effect_union(
        composition::ElementKey{union_id}, *space);
  }
  return std::move(shape)
      .focusable()
      .gesture(
          composition::DragGesture(composition::DragGesture::default_distance,
                                   composition::CoordinateSpace::global)
              .updating(
                  handles.dragged,
                  [](const composition::DragGesture::Value& value,
                     composition::Size& state) { state = value.translation; })
              .on_ended([placement,
                         bounds](const composition::DragGesture::Value& value) {
                const composition::Point from = placement.get();
                placement.set(clamped(
                    composition::Point{.x = from.x + value.translation.width,
                                       .y = from.y + value.translation.height},
                    bounds));
              }))
      .id(shape_views.at(index));
}

auto GlassPreview::pieces(const Handles& handles,
                          std::optional<composition::Namespace::ID> space) const
    -> composition::Element {
  const auto count = static_cast<std::size_t>(std::clamp(look_.count, 1, 3));
  std::vector<composition::Element> shapes;
  shapes.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    shapes.push_back(piece(index, handles, space));
  }
  return components::GlassEffectContainer(
      theme::Space{static_cast<float>(look_.spacing)},
      components::HStack(
          components::HStackOptions{
              .alignment = components::VerticalAlignment::center,
              .spacing = theme::Space{gap}},
          std::move(shapes)));
}

auto GlassPreview::group(const Handles& handles,
                         composition::Context& context) const
    -> composition::Element {
  switch (look_.grouping) {
    case Grouping::single:
      return components::ZStack(piece(0, handles, std::nullopt));
    case Grouping::container:
      return pieces(handles, std::nullopt);
    case Grouping::united:
      return pieces(handles, context.namespace_id(unioned));
  }
  return components::ZStack(piece(0, handles, std::nullopt));
}

auto GlassPreview::body(composition::Context& context) const
    -> composition::Element {
  const auto placement = context.state(placed);
  const auto drag = context.state(dragged);
  const auto holding = context.state(held);
  const auto area = context.state(room);
  const composition::Size bounds = area.get();
  const composition::Point at =
      clamped(composition::Point{.x = placement.get().x + drag.get().width,
                                 .y = placement.get().y + drag.get().height},
              bounds);
  composition::Element moved =
      group(Handles{.placed = placement, .dragged = drag, .bounds = bounds},
            context)
          .offset(at.x, at.y)
          .on_command(provider::Button::a, holding.get() ? "Place" : "Move",
                      [holding] { holding.set(!holding.get()); });
  if (holding.get()) {
    moved =
        std::move(moved)
            .on_move_command([placement, bounds](
                                 composition::MoveCommandDirection direction) {
              placement.set(nudged(placement.get(), direction, bounds));
            })
            .on_exit_command("Place", [holding] { holding.set(false); });
  }
  return components::ZStack(scenery(photo_), std::move(moved).id(preview_view))
      .frame({.max_width = composition::infinity,
              .max_height = composition::infinity})
      .clip_shape(theme::RoundedRectangle(
          theme::radius::card, theme::RoundedCornerStyle::continuous))
      .on_geometry_change(
          std::type_identity<composition::Size>{},
          [](const composition::GeometryProxy& proxy) { return proxy.size(); },
          [area](const composition::Size& size) { area.set(size); })
      .id(stage_view);
}

}
