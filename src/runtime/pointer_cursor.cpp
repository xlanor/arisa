#include <runtime/pointer_cursor.hpp>

#include <arisa/components/shape_view.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/alignment.hpp>
#include <arisa/composition/layer.hpp>
#include <arisa/composition/overlay_input.hpp>
#include <arisa/composition/overlay_layer.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/graphics/affine_transform.hpp>
#include <arisa/graphics/stroke_style.hpp>
#include <arisa/motion/angle.hpp>
#include <arisa/motion/transition.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/shape.hpp>

#include <array>
#include <span>
#include <utility>
#include <vector>

namespace arisa::runtime {

auto PointerCursor::shape_of(const composition::PointerStyle& style) noexcept
    -> provider::PointerShape {
  using Kind = composition::PointerStyle::Kind;
  using Shape = provider::PointerShape;
  using Position = composition::FrameResizePosition;
  switch (style.kind()) {
    case Kind::automatic:
      return Shape::arrow;
    case Kind::horizontal_text:
      return Shape::text;
    case Kind::vertical_text:
      return Shape::vertical_text;
    case Kind::rect_selection:
      return Shape::crosshair;
    case Kind::grab_idle:
      return Shape::open_hand;
    case Kind::grab_active:
      return Shape::closed_hand;
    case Kind::link:
      return Shape::pointing_hand;
    case Kind::zoom_in:
      return Shape::zoom_in;
    case Kind::zoom_out:
      return Shape::zoom_out;
    case Kind::image:
      return Shape::custom;
    case Kind::column_resize:
      if (style.horizontal() == composition::HorizontalDirections::leading) {
        return Shape::resize_leading;
      }
      if (style.horizontal() == composition::HorizontalDirections::trailing) {
        return Shape::resize_trailing;
      }
      return Shape::resize_horizontal;
    case Kind::row_resize:
      if (style.vertical() == composition::VerticalDirections::up) {
        return Shape::resize_top;
      }
      if (style.vertical() == composition::VerticalDirections::down) {
        return Shape::resize_bottom;
      }
      return Shape::resize_vertical;
    case Kind::frame_resize:
      break;
  }
  const composition::FrameResizeDirections directions =
      style.frame_directions();
  const bool both = directions == composition::FrameResizeDirections::all ||
                    directions.empty();
  const bool outward =
      directions == composition::FrameResizeDirections::outward;
  const auto pick = [&](Shape bidirectional, Shape away, Shape toward) {
    if (both) {
      return bidirectional;
    }
    return outward ? away : toward;
  };
  switch (style.position()) {
    case Position::top:
      return pick(Shape::resize_vertical, Shape::resize_top,
                  Shape::resize_bottom);
    case Position::bottom:
      return pick(Shape::resize_vertical, Shape::resize_bottom,
                  Shape::resize_top);
    case Position::leading:
      return pick(Shape::resize_horizontal, Shape::resize_leading,
                  Shape::resize_trailing);
    case Position::trailing:
      return pick(Shape::resize_horizontal, Shape::resize_trailing,
                  Shape::resize_leading);
    case Position::top_leading:
      return pick(Shape::resize_descending, Shape::resize_top_leading,
                  Shape::resize_bottom_trailing);
    case Position::bottom_trailing:
      return pick(Shape::resize_descending, Shape::resize_bottom_trailing,
                  Shape::resize_top_leading);
    case Position::top_trailing:
      return pick(Shape::resize_ascending, Shape::resize_top_trailing,
                  Shape::resize_bottom_leading);
    case Position::bottom_leading:
      return pick(Shape::resize_ascending, Shape::resize_bottom_leading,
                  Shape::resize_top_trailing);
  }
  return Shape::arrow;
}

auto PointerCursor::hot_spot(const composition::PointerStyle& style) noexcept
    -> composition::Point {
  if (style.kind() == composition::PointerStyle::Kind::image) {
    return composition::Point{.x = style.hot_spot().x * canvas,
                              .y = style.hot_spot().y * canvas};
  }
  if (shape_of(style) == provider::PointerShape::arrow) {
    return composition::Point{.x = dot * 0.5F, .y = dot * 0.5F};
  }
  return composition::Point{.x = canvas * 0.5F, .y = canvas * 0.5F};
}

auto PointerCursor::glyph(graphics::Path path) -> composition::Element {
  graphics::Path outline_path = path;
  return components::ZStack(
             components::ShapeView(std::move(path))
                 .fill(theme::Colour{0.0F, 0.0F, 0.0F, 1.0F})
                 .frame({.width = canvas, .height = canvas}),
             components::ShapeView(std::move(outline_path))
                 .stroke(theme::Colour{1.0F, 1.0F, 1.0F, 1.0F},
                         graphics::StrokeStyle{
                             .line_width = outline,
                             .line_join = graphics::LineJoin::round})
                 .frame({.width = canvas, .height = canvas}))
      .frame({.width = canvas, .height = canvas});
}

auto PointerCursor::arrow(bool both) -> graphics::Path {
  graphics::Path path;
  std::vector<composition::Point> outline_points;
  if (both) {
    outline_points = {{.x = 3.0F, .y = 12.0F},  {.x = 8.0F, .y = 7.0F},
                      {.x = 8.0F, .y = 10.5F},  {.x = 16.0F, .y = 10.5F},
                      {.x = 16.0F, .y = 7.0F},  {.x = 21.0F, .y = 12.0F},
                      {.x = 16.0F, .y = 17.0F}, {.x = 16.0F, .y = 13.5F},
                      {.x = 8.0F, .y = 13.5F},  {.x = 8.0F, .y = 17.0F}};
  } else {
    outline_points = {{.x = 3.0F, .y = 12.0F},  {.x = 9.0F, .y = 6.0F},
                      {.x = 9.0F, .y = 10.5F},  {.x = 20.0F, .y = 10.5F},
                      {.x = 20.0F, .y = 13.5F}, {.x = 9.0F, .y = 13.5F},
                      {.x = 9.0F, .y = 18.0F}};
  }
  path.move(outline_points.front());
  path.add_lines(std::span{outline_points}.subspan(1));
  path.close_subpath();
  return path;
}

auto PointerCursor::hand(bool closed) -> graphics::Path {
  graphics::Path path;
  path.add_rounded_rect(
      composition::Rect{.x = 6.0F, .y = 11.0F, .width = 12.0F, .height = 10.0F},
      composition::Size{.width = 4.0F, .height = 4.0F});
  const float reach = closed ? 3.0F : 7.0F;
  for (const float x : {6.5F, 9.5F, 12.5F, 15.5F}) {
    path.add_rounded_rect(
        composition::Rect{
            .x = x, .y = 12.0F - reach, .width = 2.5F, .height = reach + 2.0F},
        composition::Size{.width = 1.25F, .height = 1.25F});
  }
  return path;
}

auto PointerCursor::magnifier(bool plus) -> graphics::Path {
  graphics::Path path;
  path.add_ellipse(
      composition::Rect{.x = 3.0F, .y = 3.0F, .width = 13.0F, .height = 13.0F});
  path.add_ellipse(
      composition::Rect{.x = 5.5F, .y = 5.5F, .width = 8.0F, .height = 8.0F});
  const std::array<composition::Point, 4> handle{{{.x = 14.0F, .y = 15.5F},
                                                  {.x = 15.5F, .y = 14.0F},
                                                  {.x = 21.0F, .y = 19.5F},
                                                  {.x = 19.5F, .y = 21.0F}}};
  path.move(handle.front());
  path.add_lines(std::span{handle}.subspan(1));
  path.close_subpath();
  path.add_rect(
      composition::Rect{.x = 7.0F, .y = 8.75F, .width = 5.0F, .height = 1.5F});
  if (plus) {
    path.add_rect(composition::Rect{
        .x = 8.75F, .y = 7.0F, .width = 1.5F, .height = 5.0F});
  }
  return path;
}

auto PointerCursor::resize(provider::PointerShape shape)
    -> composition::Element {
  using Shape = provider::PointerShape;
  bool both = true;
  double degrees = 0.0;
  switch (shape) {
    case Shape::resize_horizontal:
      break;
    case Shape::resize_vertical:
      degrees = 90.0;
      break;
    case Shape::resize_descending:
      degrees = 45.0;
      break;
    case Shape::resize_ascending:
      degrees = -45.0;
      break;
    case Shape::resize_leading:
      both = false;
      break;
    case Shape::resize_trailing:
      both = false;
      degrees = 180.0;
      break;
    case Shape::resize_top:
      both = false;
      degrees = 90.0;
      break;
    case Shape::resize_bottom:
      both = false;
      degrees = -90.0;
      break;
    case Shape::resize_top_leading:
      both = false;
      degrees = 45.0;
      break;
    case Shape::resize_bottom_trailing:
      both = false;
      degrees = 225.0;
      break;
    case Shape::resize_top_trailing:
      both = false;
      degrees = 135.0;
      break;
    case Shape::resize_bottom_leading:
      both = false;
      degrees = -45.0;
      break;
    default:
      break;
  }
  const graphics::Path base = arrow(both);
  const float centre = canvas * 0.5F;
  const graphics::AffineTransform turned =
      graphics::AffineTransform::translation(-centre, -centre)
          .concatenating(graphics::AffineTransform::rotation(
              motion::Angle::degrees(degrees)))
          .concatenating(
              graphics::AffineTransform::translation(centre, centre));
  return glyph(base.applying(turned));
}

auto PointerCursor::art(const composition::PointerStyle& style)
    -> composition::Element {
  using Shape = provider::PointerShape;
  switch (const Shape shape = shape_of(style)) {
    case Shape::arrow:
      return components::ShapeView(theme::Shape::capsule())
          .fill(theme::Colour{0.0F, 0.0F, 0.0F, dot_opacity})
          .overlay(components::ShapeView(theme::Shape::capsule())
                       .stroke_border(theme::Colour{1.0F, 1.0F, 1.0F, 0.9F},
                                      outline))
          .frame({.width = dot, .height = dot});
    case Shape::text:
      return glyph(graphics::Path{
          composition::Rect{
              .x = 10.5F, .y = 2.0F, .width = 3.0F, .height = 20.0F},
          1.5F});
    case Shape::vertical_text:
      return glyph(graphics::Path{
          composition::Rect{
              .x = 2.0F, .y = 10.5F, .width = 20.0F, .height = 3.0F},
          1.5F});
    case Shape::crosshair: {
      graphics::Path path;
      path.add_rect(composition::Rect{
          .x = 11.0F, .y = 3.0F, .width = 2.0F, .height = 18.0F});
      path.add_rect(composition::Rect{
          .x = 3.0F, .y = 11.0F, .width = 18.0F, .height = 2.0F});
      return glyph(std::move(path));
    }
    case Shape::pointing_hand: {
      graphics::Path path;
      path.add_ellipse(composition::Rect{
          .x = 5.0F, .y = 5.0F, .width = 14.0F, .height = 14.0F});
      path.add_ellipse(composition::Rect{
          .x = 9.0F, .y = 9.0F, .width = 6.0F, .height = 6.0F});
      return glyph(std::move(path));
    }
    case Shape::open_hand:
      return glyph(hand(false));
    case Shape::closed_hand:
      return glyph(hand(true));
    case Shape::zoom_in:
      return glyph(magnifier(true));
    case Shape::zoom_out:
      return glyph(magnifier(false));
    case Shape::custom:
      if (const auto built = style.built(); built != nullptr && *built) {
        return (*built)().frame({.width = canvas, .height = canvas});
      }
      return glyph(arrow(true));
    default:
      return resize(shape);
  }
}

void PointerCursor::hide(composition::Tree& tree) {
  if (shown_.has_value()) {
    tree.present_overlay(overlay, std::nullopt);
    shown_.reset();
  }
}

void PointerCursor::present(
    composition::Tree& tree,
    const std::optional<interaction::InputRouter::Pointer>& pointer,
    provider::InputProvider& input) {
  if (input.pointer_host() == provider::PointerHost::system) {
    hide(tree);
    if (!pointer.has_value()) {
      return;
    }
    const provider::PointerAppearance appearance{
        .shape = shape_of(pointer->style), .visible = pointer->visible};
    if (presented_ != appearance) {
      presented_ = appearance;
      input.present_pointer(appearance);
    }
    return;
  }
  if (!pointer.has_value() || !pointer->visible) {
    hide(tree);
    return;
  }
  if (shown_ == pointer) {
    return;
  }
  shown_ = pointer;
  const composition::Point hot = hot_spot(pointer->style);
  std::vector<composition::Element> children;
  children.push_back(
      art(pointer->style)
          .offset(pointer->location.x - hot.x, pointer->location.y - hot.y)
          .allows_hit_testing(false)
          .transition(motion::Transition::identity()));
  tree.present_overlay(
      overlay, composition::Layer(composition::OverlayLayer::pointer,
                                  composition::OverlayInput::passthrough,
                                  composition::Alignment::top_leading,
                                  std::move(children)));
}

}
