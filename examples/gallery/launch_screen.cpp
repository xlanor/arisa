#include <launch_screen.hpp>

#include <arisa/async/awaitables.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/mesh_gradient.hpp>
#include <arisa/components/shape_view.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/command_options.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/move_command_direction.hpp>
#include <arisa/graphics/gradient.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/graphics/stroke_style.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/provider/input/buttons.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/shape.hpp>

#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstddef>
#include <initializer_list>
#include <limits>
#include <numbers>
#include <optional>
#include <ranges>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>

namespace arisa::examples {

using composition::Element;
using composition::infinity;
using composition::Point;

auto LaunchScreen::knots() -> std::vector<Knot> {
  std::vector<Knot> found{
      {70, -82},  {44, -102},  {12, -88},   {0, -50},    {12, -8},
      {38, 1},    {64, -22},   {78, -70},   {83, -102},  {86, -94},
      {80, -50},  {82, -10},   {100, 1},    {120, -16},  {138, -62},
      {150, -96}, {146, -112}, {134, -104}, {140, -92},  {160, -98},
      {178, -92}, {184, -60},  {186, -18},  {200, 1},    {218, -14},
      {230, -60}, {238, -102}, {242, -94},  {238, -46},  {242, -8},
      {258, 1},   {278, -16},  {292, -62},  {304, -104}, {312, -96},
      {332, -62}, {334, -22},  {312, 1},    {286, -3},   {274, -16},
      {284, -28}, {300, -18},  {322, -4},   {348, -8},   {370, -32},
      {396, -76}, {430, -102}, {470, -104}, {476, -96},  {446, -104},
      {414, -92}, {400, -50},  {412, -8},   {438, 1},    {464, -22},
      {478, -70}, {483, -102}, {486, -94},  {480, -50},  {482, -10},
      {500, 1},   {524, -14},  {544, -34}};
  constexpr int turns = 24;
  constexpr float spin = 2.4F;
  constexpr float radius = 9.0F;
  constexpr float shrink = 34.0F;
  constexpr Point dot{.x = 264.0F, .y = -148.0F};
  for (int step = 0; step <= turns; ++step) {
    const float share = static_cast<float>(step) / static_cast<float>(turns);
    const float angle = (share * 2.0F * std::numbers::pi_v<float> * spin) +
                        std::numbers::pi_v<float>;
    const float reach = radius * (1.0F - (static_cast<float>(step) / shrink));
    found.push_back(Knot{.x = dot.x + (reach * std::cos(angle)),
                         .y = dot.y + (reach * std::sin(angle)),
                         .lift = step == 0});
  }
  return found;
}

auto LaunchScreen::placed() -> std::vector<Knot> {
  const float lean =
      std::tan(slant_degrees * std::numbers::pi_v<float> / 180.0F);
  std::vector<Knot> found = knots();
  float left = std::numeric_limits<float>::max();
  float top = std::numeric_limits<float>::max();
  for (Knot& knot : found) {
    knot.x -= knot.y * lean;
    left = std::min(left, knot.x);
    top = std::min(top, knot.y);
  }
  for (Knot& knot : found) {
    knot.x = ((knot.x - left) * unit) + margin;
    knot.y = ((knot.y - top) * unit) + margin;
  }
  return found;
}

auto LaunchScreen::handles(Point before, Point from, Point to, Point after)
    -> std::array<Point, 2> {
  const auto reach = [](Point a, Point b) {
    return std::max(std::sqrt(std::hypot(b.x - a.x, b.y - a.y)), 1.0e-3F);
  };
  const float d1 = reach(before, from);
  const float d2 = reach(from, to);
  const float d3 = reach(to, after);
  const auto blend = [](float p0, float p1, float p2, float a, float b) {
    return ((a * a * p2) - (b * b * p0) +
            (((2.0F * a * a) + (3.0F * a * b) + (b * b)) * p1)) /
           (3.0F * a * (a + b));
  };
  return {Point{.x = blend(before.x, from.x, to.x, d1, d2),
                .y = blend(before.y, from.y, to.y, d1, d2)},
          Point{.x = blend(after.x, to.x, from.x, d3, d2),
                .y = blend(after.y, to.y, from.y, d3, d2)}};
}

auto LaunchScreen::lettering_size() -> composition::Size {
  float right = 0.0F;
  float bottom = 0.0F;
  for (const Knot& knot : placed()) {
    right = std::max(right, knot.x);
    bottom = std::max(bottom, knot.y);
  }
  return composition::Size{.width = right + margin, .height = bottom + margin};
}

auto LaunchScreen::lettering(bool with_dot) -> graphics::Path {
  std::vector<Knot> found = placed();
  if (!with_dot) {
    const auto lift =
        std::ranges::find_if(found, [](const Knot& knot) { return knot.lift; });
    found.erase(lift, found.end());
  }
  graphics::Path path;
  if (found.size() < 2) {
    return path;
  }
  const auto at = [](const Knot& knot) {
    return Point{.x = knot.x, .y = knot.y};
  };
  const auto mirror = [](Point pivot, Point other) {
    return Point{.x = (2.0F * pivot.x) - other.x,
                 .y = (2.0F * pivot.y) - other.y};
  };
  path.move(at(found.front()));
  for (std::size_t index = 0; index + 1 < found.size(); ++index) {
    const Point from = at(found[index]);
    const Point to = at(found[index + 1]);
    if (found[index + 1].lift) {
      path.move(to);
      continue;
    }
    const bool starts = index == 0 || found[index].lift || found[index].corner;
    const bool ends = index + 2 >= found.size() || found[index + 2].lift ||
                      found[index + 1].corner;
    const Point before = starts ? mirror(from, to) : at(found[index - 1]);
    const Point after = ends ? mirror(to, from) : at(found[index + 2]);
    const std::array<Point, 2> control = handles(before, from, to, after);
    path.add_curve(to, control[0], control[1]);
  }
  return path;
}

auto LaunchScreen::mesh_points(bool drifting) -> std::vector<Point> {
  if (drifting) {
    return {{0.0F, 0.0F},  {0.42F, 0.0F},  {1.0F, 0.0F},
            {0.0F, 0.36F}, {0.56F, 0.46F}, {1.0F, 0.64F},
            {0.0F, 1.0F},  {0.58F, 1.0F},  {1.0F, 1.0F}};
  }
  return {{0.0F, 0.0F},  {0.5F, 0.0F}, {1.0F, 0.0F},
          {0.0F, 0.42F}, {0.5F, 0.5F}, {1.0F, 0.58F},
          {0.0F, 1.0F},  {0.5F, 1.0F}, {1.0F, 1.0F}};
}

auto LaunchScreen::write_animation() -> motion::Animation {
  return motion::Animation::timing_curve(write_curve[0], write_curve[1],
                                         write_curve[2], write_curve[3],
                                         write_end - write_start)
      .delay(write_start);
}

auto LaunchScreen::erase_animation() -> motion::Animation {
  return motion::Animation::timing_curve(erase_curve[0], erase_curve[1],
                                         erase_curve[2], erase_curve[3],
                                         erase_end - hold_end);
}

auto LaunchScreen::wallpaper_view(bool drift) -> Element {
  const auto colour = [](std::uint32_t rgb) {
    return theme::ColourValue{theme::Colour::hex(rgb)};
  };
  const theme::Colour sky = theme::Colour::hex(wallpaper[0]);
  const theme::Colour deep = theme::Colour::hex(wallpaper[1]);
  const theme::Colour rose = theme::Colour::hex(wallpaper[2]);
  const theme::Colour coral = theme::Colour::hex(wallpaper[3]);
  const theme::Colour plum = theme::Colour::hex(wallpaper[4]);
  const theme::Colour navy = theme::Colour::hex(wallpaper[5]);
  const auto between = [](theme::Colour a, theme::Colour b) {
    return theme::ColourValue{graphics::Gradient::mix(
        a, b, 0.5F, graphics::Gradient::ColorSpace::perceptual)};
  };
  std::vector<theme::ColourValue> colours{
      colour(wallpaper[0]), between(sky, deep),   colour(wallpaper[1]),
      colour(wallpaper[2]), between(rose, coral), colour(wallpaper[3]),
      colour(wallpaper[4]), between(plum, navy),  colour(wallpaper[5])};
  return Element{
      components::MeshGradient(3, 3, mesh_points(drift), std::move(colours),
                               theme::Colour::hex(wallpaper[5]), true,
                               graphics::Gradient::ColorSpace::perceptual)
          .grain(grain)}
      .frame({.max_width = infinity, .max_height = infinity});
}

auto LaunchScreen::photo_view(const Photo& photo, bool drift) -> Element {
  return Element{
      components::Image::Raster(photo, components::Image::ContentMode::fill)}
      .frame({.max_width = infinity, .max_height = infinity})
      .scale_effect(drift ? pan_scale : pan_rest_scale,
                    motion::UnitPoint{.x = 0.58F, .y = 0.52F})
      .offset(drift ? -pan_shift : 0.0F, drift ? pan_shift * 0.4F : 0.0F)
      .clip_shape(theme::Shape::rectangle());
}

auto LaunchScreen::glow() -> Element {
  const composition::Size size = lettering_size();
  const float reach = size.width * glow_spread;
  return Element{
      components::ShapeView(theme::Shape::rectangle())
          .fill(graphics::ShapeStyle{graphics::RadialGradient{
              .gradient = graphics::Gradient{std::vector<theme::ColourValue>{
                  theme::Colour::hex(0x0B1A24).with_alpha(glow_opacity),
                  theme::Colour::hex(0x0B1A24).with_alpha(0.0F)}},
              .center = motion::UnitPoint::center,
              .start_radius = 0.0F,
              .end_radius = reach * 0.5F}})}
      .frame({.width = reach, .height = reach})
      .scale_effect(1.0F, (size.height * glow_spread * 1.4F) / reach);
}

auto LaunchScreen::ink_style(Ink ink, bool along) -> graphics::ShapeStyle {
  if (ink == Ink::white) {
    return graphics::ShapeStyle{theme::Colour::hex(0xFFFFFF)};
  }
  const graphics::Gradient joy =
      graphics::Gradient{std::vector<theme::ColourValue>{
                             theme::Colour::hex(blue), theme::Colour::hex(red)}}
          .color_space(graphics::Gradient::ColorSpace::oklch);
  static_cast<void>(along);
  return graphics::ShapeStyle{
      graphics::LinearGradient{.gradient = joy,
                               .start_point = motion::UnitPoint::leading,
                               .end_point = motion::UnitPoint::trailing}};
}

auto LaunchScreen::fitted(const graphics::Path& path, composition::Rect area)
    -> graphics::Path {
  const composition::Rect bounds = path.bounding_rect();
  const float sx = bounds.width > 0.0F ? area.width / bounds.width : 1.0F;
  const float sy = bounds.height > 0.0F ? area.height / bounds.height : 1.0F;
  const auto map = [&](Point point) {
    return Point{.x = area.x + ((point.x - bounds.x) * sx),
                 .y = area.y + ((point.y - bounds.y) * sy)};
  };
  graphics::Path moved;
  for (const graphics::Path::Element& element : path.elements()) {
    std::visit(
        [&](const auto& each) {
          using T = std::remove_cvref_t<decltype(each)>;
          if constexpr (std::same_as<T, graphics::Path::Move>) {
            moved.move(map(each.to));
          } else if constexpr (std::same_as<T, graphics::Path::Line>) {
            moved.add_line(map(each.to));
          } else if constexpr (std::same_as<T, graphics::Path::QuadCurve>) {
            moved.add_quad_curve(map(each.to), map(each.control));
          } else if constexpr (std::same_as<T, graphics::Path::Curve>) {
            moved.add_curve(map(each.to), map(each.control1),
                            map(each.control2));
          } else {
            moved.close_subpath();
          }
        },
        element);
  }
  return moved;
}

auto LaunchScreen::separated(const graphics::Path& path) -> Parts {
  std::vector<graphics::Path> pieces;
  for (const graphics::Path::Element& element : path.elements()) {
    if (std::holds_alternative<graphics::Path::Move>(element) ||
        pieces.empty()) {
      pieces.emplace_back();
    }
    append(pieces.back(), element);
  }
  const composition::Rect whole = path.bounding_rect();
  const float ceiling = whole.y + (whole.height * dot_ceiling);
  Parts parts;
  for (const graphics::Path& piece : pieces) {
    const composition::Rect bounds = piece.bounding_rect();
    graphics::Path& into =
        bounds.y + bounds.height < ceiling ? parts.dot : parts.body;
    into.add_path(piece);
  }
  return parts;
}

auto LaunchScreen::guide() -> graphics::Path {
  graphics::Path path;
  path.move(guide_points.front());
  for (const Point& point : guide_points | std::views::drop(1)) {
    path.add_line(point);
  }
  path.move(Point{.x = guide_frame.x, .y = guide_frame.y});
  path.move(Point{.x = guide_frame.x + guide_frame.width,
                  .y = guide_frame.y + guide_frame.height});
  return path;
}

auto LaunchScreen::typeset(const text::FontFace& font, Ink ink, Reveal reveal)
    -> std::optional<Element> {
  const auto size = text::PointSize::create(type_size);
  if (!size.has_value()) {
    return std::nullopt;
  }
  auto shaped = font.outline(word_text, *size);
  if (!shaped.has_value() || shaped->is_empty()) {
    return std::nullopt;
  }
  const composition::Rect bounds = shaped->bounding_rect();
  const composition::Rect area{
      .x = margin, .y = margin, .width = bounds.width, .height = bounds.height};
  const Parts parts = separated(fitted(*shaped, area));
  const composition::Rect mark = parts.dot.bounding_rect();
  graphics::Path flick;
  flick.move(Point{.x = mark.x, .y = mark.y + (mark.height * 0.5F)});
  flick.add_line(
      Point{.x = mark.x + mark.width, .y = mark.y + (mark.height * 0.5F)});
  const auto layer = [&](const graphics::Path& shape, graphics::Path guide,
                         bool shown, bool hidden) {
    return Element{components::ShapeView(shape)
                       .trim_along(std::move(guide), reveal_reach)
                       .trim(hidden ? 1.0 : 0.0, shown ? 1.0 : 0.0)
                       .fill(ink_style(ink, false))}
        .frame({.width = bounds.width + (margin * 2.0F),
                .height = bounds.height + (margin * 2.0F)});
  };
  const graphics::Path pen = fitted(guide(), area);
  return Element{
      components::ZStack(layer(parts.body, pen, reveal.write, reveal.erase),
                         layer(parts.dot, flick, reveal.dot, reveal.undot))}
      .id(stroke_view);
}

auto LaunchScreen::word(Ink ink, bool write, bool erase) -> Element {
  const composition::Size size = lettering_size();
  const double from = erase ? 1.0 : 0.0;
  const double to = write ? 1.0 : 0.0;
  const graphics::StrokeStyle pen{
      .line_width = ink == Ink::glass ? glass_width : resting_width,
      .line_cap = graphics::LineCap::round,
      .line_join = graphics::LineJoin::round};
  components::ShapeView stroke = components::ShapeView(lettering())
                                     .trim(from, to)
                                     .stroke(ink_style(ink, true), pen);
  if (ink == Ink::glass) {
    stroke = std::move(stroke).glass_effect(theme::Glass::clear());
  }
  return Element{std::move(stroke)}
      .frame({.width = size.width, .height = size.height})
      .id(stroke_view);
}

auto LaunchScreen::skip(composition::StateHandle<bool> skipping,
                        composition::StateHandle<bool> fade, Finished finished)
    -> void {
  if (skipping.get()) {
    return;
  }
  skipping.set(true);
  motion::with_animation(
      motion::Animation::ease_out(skip_duration), [fade] { fade.set(true); },
      [finished] {
        if (finished != nullptr && *finished) {
          (*finished)();
        }
      });
}

auto LaunchScreen::timeline(Ending ending, Beats beats, Finished finished,
                            bool panning) -> async::Task<void> {
  co_await async::next_frame();
  motion::with_animation(write_animation(),
                         [write = beats.write] { write.set(true); });
  co_await async::next_frame();
  motion::with_animation(
      motion::Animation::ease_out(dot_duration).delay(write_end - dot_duration),
      [dot = beats.dot] { dot.set(true); });
  co_await async::next_frame();
  motion::with_animation(
      motion::Animation::ease_in_out(panning ? pan_period : drift_period)
          .repeat_forever(true)
          .preferred_frame_rate(panning ? pan_frame_rate : drift_frame_rate),
      [drift = beats.drift] { drift.set(true); });
  co_await async::sleep(seconds(hold_end));
  if (beats.skip.get()) {
    co_return;
  }
  if (ending == Ending::erase) {
    motion::with_animation(erase_animation(),
                           [erase = beats.erase] { erase.set(true); });
    co_await async::next_frame();
    motion::with_animation(motion::Animation::ease_in(dot_duration)
                               .delay(erase_end - hold_end - dot_duration),
                           [undot = beats.undot] { undot.set(true); });
    co_await async::sleep(seconds(erase_end - hold_end));
    if (beats.skip.get()) {
      co_return;
    }
  }
  motion::with_animation(
      motion::Animation::ease_in_out(fade_duration),
      [fade = beats.fade] { fade.set(true); },
      [finished] {
        if (finished != nullptr && *finished) {
          (*finished)();
        }
      });
}

auto LaunchScreen::seconds(double value) -> std::chrono::nanoseconds {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::duration<double>(value));
}

void LaunchScreen::append(graphics::Path& into,
                          const graphics::Path::Element& element) {
  std::visit(
      [&into](const auto& each) {
        using T = std::remove_cvref_t<decltype(each)>;
        if constexpr (std::same_as<T, graphics::Path::Move>) {
          into.move(each.to);
        } else if constexpr (std::same_as<T, graphics::Path::Line>) {
          into.add_line(each.to);
        } else if constexpr (std::same_as<T, graphics::Path::QuadCurve>) {
          into.add_quad_curve(each.to, each.control);
        } else if constexpr (std::same_as<T, graphics::Path::Curve>) {
          into.add_curve(each.to, each.control1, each.control2);
        } else {
          into.close_subpath();
        }
      },
      element);
}

auto LaunchScreen::body(composition::Context& context) const -> Element {
  const auto write = context.state(written);
  const auto erase = context.state(erased);
  const auto fade = context.state(faded);
  const auto drift = context.state(drifted);
  const auto skipping = context.state(skipped);
  const auto dot = context.state(dotted);
  const auto undot = context.state(undotted);
  const auto skipper = [skipping, fade, finished = finished_] {
    skip(skipping, fade, finished);
  };
  Element root =
      components::ZStack(
          photo_ != nullptr ? photo_view(photo_, drift.get())
                            : wallpaper_view(drift.get()),
          photo_ != nullptr && style_.ink != Ink::glass
              ? glow()
              : composition::Element{components::ZStack()},
          (font_ != nullptr && style_.ink != Ink::glass
               ? typeset(*font_, style_.ink,
                         Reveal{.write = write.get(),
                                .erase = erase.get(),
                                .dot = dot.get(),
                                .undot = undot.get()})
               : std::nullopt)
              .value_or(word(style_.ink, write.get(), erase.get())))
          .frame({.max_width = infinity, .max_height = infinity})
          .opacity(fade.get() ? 0.0F : 1.0F)
          .focusable()
          .focus_effect_disabled()
          .on_tap_gesture(skipper)
          .on_move_command(
              [skipper](composition::MoveCommandDirection) { skipper(); })
          .on_exit_command("Skip", skipper);
  for (const provider::Button button :
       {provider::Button::a, provider::Button::x, provider::Button::y,
        provider::Button::l, provider::Button::r, provider::Button::zl,
        provider::Button::zr, provider::Button::plus, provider::Button::minus,
        provider::Button::left_stick, provider::Button::right_stick}) {
    root = std::move(root).on_command(
        button, std::string{"Skip"}, skipper,
        composition::CommandOptions{.hidden = button != provider::Button::a});
  }
  return std::move(root)
      .task([ending = style_.ending,
             beats = Beats{.write = write,
                           .drift = drift,
                           .erase = erase,
                           .fade = fade,
                           .skip = skipping,
                           .dot = dot,
                           .undot = undot},
             finished = finished_, panning = photo_ != nullptr] {
        return timeline(ending, beats, finished, panning);
      })
      .id(root_view);
}

}
