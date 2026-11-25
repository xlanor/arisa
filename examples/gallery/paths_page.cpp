#include <paths_page.hpp>

#include <arisa/async/awaitables.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/mesh_gradient.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/shape_view.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/graphics/gradient.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/graphics/stroke_style.hpp>
#include <arisa/motion/angle.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/with_animation.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>

#include <chrono>
#include <cmath>
#include <numbers>
#include <string>
#include <utility>

namespace arisa::examples {

using composition::Element;
using composition::infinity;
using composition::Point;
using theme::ColourRole;

auto PathsPage::star(float size) -> graphics::Path {
  graphics::Path path;
  const float centre = size * 0.5F;
  constexpr int points = 5;
  for (int index = 0; index < points; ++index) {
    const double angle = (-std::numbers::pi / 2.0) +
                         (static_cast<double>(index) * 4.0 * std::numbers::pi /
                          static_cast<double>(points));
    const Point at{
        .x = centre + (centre * static_cast<float>(std::cos(angle))),
        .y = centre + (centre * static_cast<float>(std::sin(angle)))};
    if (index == 0) {
      path.move(at);
    } else {
      path.add_line(at);
    }
  }
  path.close_subpath();
  return path;
}

auto PathsPage::mesh_points(bool drifted) -> std::vector<Point> {
  const float shift = drifted ? 0.12F : 0.0F;
  return {
      {0.0F, 0.0F},         {0.5F + shift, 0.0F},         {1.0F, 0.0F},
      {0.0F, 0.5F - shift}, {0.5F - shift, 0.5F + shift}, {1.0F, 0.5F + shift},
      {0.0F, 1.0F},         {0.5F - shift, 1.0F},         {1.0F, 1.0F}};
}

auto PathsPage::tile(Element content, std::string_view caption) -> Element {
  return components::VStack(
             components::VStackOptions{
                 .alignment = components::HorizontalAlignment::center,
                 .spacing = theme::space::xs},
             std::move(content),
             components::Text(std::string{caption})
                 .font(theme::Font::caption)
                 .foreground_style(ColourRole::text_secondary))
      .frame({.width = tile_width});
}

auto PathsPage::section(std::string_view title, std::vector<Element> tiles)
    -> Element {
  return components::VStack(
             components::VStackOptions{
                 .alignment = components::HorizontalAlignment::leading,
                 .spacing = theme::space::sm},
             components::Text(std::string{title}).font(theme::Font::headline),
             components::HStack(
                 components::HStackOptions{
                     .alignment = components::VerticalAlignment::top,
                     .spacing = theme::space::lg,
                     .wrap = true},
                 std::move(tiles))
                 .frame({.max_width = infinity,
                         .alignment = composition::Alignment::top}))
      .frame({.max_width = infinity,
              .alignment = composition::Alignment::leading});
}

auto PathsPage::shapes() -> Element {
  std::vector<Element> tiles;
  tiles.push_back(tile(
      Element{components::ShapeView(theme::Circle{}).fill(ColourRole::blue)}
          .frame({.width = 64.0F, .height = 64.0F}),
      "Circle"));
  tiles.push_back(
      tile(Element{components::ShapeView(
                       theme::RoundedRectangle{
                           18.0F, theme::RoundedCornerStyle::continuous})
                       .fill(ColourRole::green)}
               .frame({.width = 96.0F, .height = 64.0F}),
           "Continuous"));
  tiles.push_back(tile(
      Element{components::ShapeView(theme::Capsule{}).fill(ColourRole::orange)}
          .frame({.width = 96.0F, .height = 40.0F}),
      "Capsule"));
  tiles.push_back(tile(Element{components::ShapeView(star(64.0F))
                                   .fill(ColourRole::purple,
                                         graphics::FillStyle{.eo_fill = true})}
                           .frame({.width = 64.0F, .height = 64.0F}),
                       "Even-odd"));
  tiles.push_back(tile(
      Element{components::ShapeView(star(64.0F)).fill(ColourRole::pink)}.frame(
          {.width = 64.0F, .height = 64.0F}),
      "Non-zero"));
  graphics::Path arc;
  arc.add_arc({32.0F, 32.0F}, 28.0F, motion::Angle::degrees(0.0),
              motion::Angle::degrees(270.0), false);
  tiles.push_back(
      tile(Element{components::ShapeView(std::move(arc))
                       .stroke(ColourRole::teal,
                               graphics::StrokeStyle{
                                   .line_width = 6.0F,
                                   .line_cap = graphics::LineCap::round})}
               .frame({.width = 64.0F, .height = 64.0F}),
           "Arc 0-270"));
  return section("Shapes and fills", std::move(tiles));
}

auto PathsPage::strokes() -> Element {
  std::vector<Element> tiles;
  const std::array caps{std::pair{graphics::LineCap::butt, "Butt cap"},
                        std::pair{graphics::LineCap::round, "Round cap"},
                        std::pair{graphics::LineCap::square, "Square cap"}};
  for (const auto& [cap, name] : caps) {
    graphics::Path line;
    line.move({16.0F, 32.0F});
    line.add_line({80.0F, 32.0F});
    tiles.push_back(
        tile(Element{components::ShapeView(std::move(line))
                         .stroke(ColourRole::label,
                                 graphics::StrokeStyle{.line_width = 14.0F,
                                                       .line_cap = cap})}
                 .frame({.width = 96.0F, .height = 64.0F}),
             name));
  }
  const std::array joins{std::pair{graphics::LineJoin::miter, "Miter join"},
                         std::pair{graphics::LineJoin::round, "Round join"},
                         std::pair{graphics::LineJoin::bevel, "Bevel join"}};
  for (const auto& [join, name] : joins) {
    graphics::Path zigzag;
    zigzag.move({12.0F, 52.0F});
    zigzag.add_line({36.0F, 14.0F});
    zigzag.add_line({60.0F, 52.0F});
    zigzag.add_line({84.0F, 14.0F});
    tiles.push_back(
        tile(Element{components::ShapeView(std::move(zigzag))
                         .stroke(ColourRole::indigo,
                                 graphics::StrokeStyle{.line_width = 10.0F,
                                                       .line_join = join})}
                 .frame({.width = 96.0F, .height = 64.0F}),
             name));
  }
  tiles.push_back(
      tile(Element{components::ShapeView(theme::RoundedRectangle{12.0F})
                       .stroke(ColourRole::red,
                               graphics::StrokeStyle{
                                   .line_width = 4.0F,
                                   .line_cap = graphics::LineCap::round,
                                   .dash = {10.0F, 8.0F},
                                   .dash_phase = 4.0F})}
               .frame({.width = 96.0F, .height = 64.0F}),
           "Dash 10/8"));
  return section("Strokes", std::move(tiles));
}

auto PathsPage::gradients() -> Element {
  std::vector<Element> tiles;
  const graphics::Gradient joycon = graphics::Gradient{
      std::vector<theme::ColourValue>{
          theme::Colour::hex(0x0AB9E6),
          theme::Colour::hex(
              0xFF3C28)}}.color_space(graphics::Gradient::ColorSpace::oklch);
  const graphics::Gradient device{std::vector<theme::ColourValue>{
      theme::Colour::hex(0x0AB9E6), theme::Colour::hex(0xFF3C28)}};
  tiles.push_back(
      tile(Element{components::ShapeView(theme::RoundedRectangle{10.0F})
                       .fill(graphics::LinearGradient{.gradient = device})}
               .frame({.width = 104.0F, .height = 48.0F}),
           "Linear sRGB"));
  tiles.push_back(
      tile(Element{components::ShapeView(theme::RoundedRectangle{10.0F})
                       .fill(graphics::LinearGradient{.gradient = joycon})}
               .frame({.width = 104.0F, .height = 48.0F}),
           "Linear OKLCH"));
  tiles.push_back(tile(
      Element{components::ShapeView(theme::Circle{})
                  .fill(graphics::RadialGradient{
                      .gradient =
                          graphics::Gradient{std::vector<theme::ColourValue>{
                              ColourRole::yellow, ColourRole::orange,
                              ColourRole::red}},
                      .center = motion::UnitPoint::center,
                      .start_radius = 0.0F,
                      .end_radius = 32.0F})}
          .frame({.width = 64.0F, .height = 64.0F}),
      "Radial"));
  tiles.push_back(tile(
      Element{
          components::ShapeView(theme::Circle{})
              .stroke(
                  graphics::AngularGradient{
                      .gradient =
                          graphics::Gradient{std::vector<theme::ColourValue>{
                              ColourRole::red, ColourRole::yellow,
                              ColourRole::green, ColourRole::blue,
                              ColourRole::purple, ColourRole::red}}},
                  graphics::StrokeStyle{.line_width = 10.0F})}
          .frame({.width = 64.0F, .height = 64.0F}),
      "Angular stroke"));
  graphics::Path wave;
  wave.move({8.0F, 40.0F});
  wave.add_curve({96.0F, 40.0F}, {36.0F, 0.0F}, {68.0F, 80.0F});
  tiles.push_back(
      tile(Element{components::ShapeView(std::move(wave))
                       .stroke(graphics::LinearGradient{.gradient = joycon},
                               graphics::StrokeStyle{
                                   .line_width = 9.0F,
                                   .line_cap = graphics::LineCap::round,
                                   .line_join = graphics::LineJoin::round})}
               .frame({.width = 104.0F, .height = 80.0F}),
           "Gradient stroke"));
  return section("Gradients", std::move(tiles));
}

auto PathsPage::delay_chase(composition::StateHandle<bool> chase)
    -> async::Task<void> {
  co_await async::sleep(std::chrono::milliseconds{400});
  motion::with_animation(
      motion::Animation::ease_in(1.6).repeat_forever(true).preferred_frame_rate(
          30.0),
      [chase] { chase.set(true); });
}

auto PathsPage::trims(composition::StateHandle<bool> write,
                      composition::StateHandle<bool> chase) -> Element {
  std::vector<Element> tiles;
  tiles.push_back(
      tile(Element{components::ShapeView(theme::Circle{})
                       .trim(0.0, write.get() ? 1.0 : 0.05)
                       .stroke(ColourRole::blue,
                               graphics::StrokeStyle{
                                   .line_width = 8.0F,
                                   .line_cap = graphics::LineCap::round})}
               .frame({.width = 64.0F, .height = 64.0F})
               .animation(motion::Animation::ease_in_out(1.4)
                              .repeat_forever(true)
                              .preferred_frame_rate(30.0),
                          write.get())
               .id(trim_view),
           "trim end"));
  tiles.push_back(
      tile(Element{components::ShapeView(star(64.0F))
                       .trim(chase.get() ? 0.6 : 0.0, write.get() ? 1.0 : 0.3)
                       .stroke(ColourRole::orange,
                               graphics::StrokeStyle{
                                   .line_width = 5.0F,
                                   .line_cap = graphics::LineCap::round,
                                   .line_join = graphics::LineJoin::round})}
               .frame({.width = 64.0F, .height = 64.0F})
               .animation(motion::Animation::ease_out(1.1)
                              .repeat_forever(true)
                              .preferred_frame_rate(30.0),
                          write.get())
               .id(chase_view)
               .task([chase] { return delay_chase(chase); }),
           "both ends"));
  return section("Animated trim", std::move(tiles));
}

auto PathsPage::mesh(composition::StateHandle<bool> drift) -> Element {
  std::vector<theme::ColourValue> colours{
      theme::Colour::hex(0x7DB8E6), theme::Colour::hex(0x5A9AD6),
      theme::Colour::hex(0x2482C0), theme::Colour::hex(0xD9406E),
      theme::Colour::hex(0xB85C7A), theme::Colour::hex(0xE07468),
      theme::Colour::hex(0x8A3A78), theme::Colour::hex(0x1F4570),
      theme::Colour::hex(0x2D3F78)};
  std::vector<Element> tiles;
  tiles.push_back(
      tile(Element{components::MeshGradient(3, 3, mesh_points(drift.get()),
                                            std::move(colours))
                       .grain(0.04F)}
               .frame({.width = 220.0F, .height = 124.0F})
               .clip_shape(theme::RoundedRectangle{14.0F})
               .animation(motion::Animation::ease_in_out(6.0)
                              .repeat_forever(true)
                              .preferred_frame_rate(20.0),
                          drift.get())
               .id(mesh_view),
           "MeshGradient 3x3"));
  return section("MeshGradient", std::move(tiles));
}

auto PathsPage::body(composition::Context& context) const -> Element {
  const auto write = context.state(writing);
  const auto chase = context.state(chasing);
  const auto drift = context.state(drifting);
  return components::ScrollView(
             components::VStack(
                 components::VStackOptions{
                     .alignment = components::HorizontalAlignment::leading,
                     .spacing = theme::space::xl},
                 shapes(), strokes(), gradients(), trims(write, chase),
                 mesh(drift))
                 .frame({.max_width = infinity,
                         .alignment = composition::Alignment::leading})
                 .on_appear([write, drift] {
                   write.set(true);
                   drift.set(true);
                 }))
      .frame({.max_width = infinity, .max_height = infinity});
}

}
