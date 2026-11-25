#include <canvas_page.hpp>

#include <gallery_shaders.hpp>

#include <arisa/components/button.hpp>
#include <arisa/components/canvas.hpp>
#include <arisa/components/graphics_context.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/shape_view.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/toolbar_item.hpp>
#include <arisa/graphics/any_shape.hpp>
#include <arisa/graphics/gradient.hpp>
#include <arisa/graphics/path.hpp>
#include <arisa/graphics/shader.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/presentation/navigation_stack.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/colour_interpolation.hpp>
#include <arisa/theme/colour_mix.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/material.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/rgb_color_space.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <array>
#include <string>
#include <utility>
#include <vector>

namespace arisa::examples {

using composition::Element;
using composition::infinity;
using theme::ColourRole;

auto CanvasPage::card(std::string_view title, Element content) -> Element {
  return components::VStack(
             components::VStackOptions{
                 .alignment = composition::HorizontalAlignment::leading,
                 .spacing = theme::space::sm},
             components::Text(std::string{title}).font(theme::Font::headline),
             std::move(content))
      .padding(theme::space::lg)
      .frame(
          {.max_width = infinity, .alignment = composition::Alignment::leading})
      .background(ColourRole::surface_raised,
                  theme::RoundedRectangle(theme::radius::card));
}

auto CanvasPage::labelled(std::string_view name, Element shown) -> Element {
  return components::VStack(
      components::VStackOptions{.spacing = theme::space::xs}, std::move(shown),
      components::Text(std::string{name}).font(theme::Font::caption));
}

auto CanvasPage::swatch(theme::ColourValue colour) -> Element {
  return components::ShapeView(theme::RoundedRectangle(10.0F))
      .fill(colour)
      .frame({.width = 64.0F, .height = 64.0F});
}

auto CanvasPage::booleans() -> Element {
  const graphics::AnyShape square =
      graphics::AnyShape{theme::Rectangle()}.size(40.0F, 64.0F);
  const graphics::AnyShape round = theme::Circle();
  const auto shown = [](graphics::AnyShape shape) {
    return Element{components::ShapeView(std::move(shape))
                       .fill(ColourRole::accent)
                       .frame({.width = 64.0F, .height = 64.0F})};
  };
  return components::HStack(
      components::HStackOptions{.spacing = theme::space::md, .wrap = true},
      labelled("Union", shown(square.union_with(round))),
      labelled("Intersection", shown(square.intersection(round))),
      labelled("Subtracting", shown(round.subtracting(square))),
      labelled("Difference", shown(square.symmetric_difference(round))),
      labelled(
          "Line cut",
          Element{components::ShapeView(graphics::AnyShape{theme::Rectangle()}
                                            .line_intersection(round))
                      .stroke(ColourRole::accent, 3.0F)
                      .frame({.width = 64.0F, .height = 64.0F})}));
}

auto CanvasPage::circles() -> Element {
  return components::HStack(
      components::HStackOptions{.spacing = theme::space::md, .wrap = true},
      labelled("Wide circle", components::ShapeView(theme::Circle())
                                  .fill(ColourRole::accent)
                                  .frame({.width = 120.0F, .height = 64.0F})
                                  .background(ColourRole::tertiary_system_fill,
                                              theme::Shape::rectangle())),
      labelled("Fill then stroke",
               components::ShapeView(theme::Circle())
                   .fill(theme::Colour::hex(0xFF9500))
                   .stroke(theme::Colour::hex(0x5856D6), 6.0F)
                   .frame({.width = 64.0F, .height = 64.0F})),
      labelled("Aliased stroke", components::ShapeView(theme::Circle())
                                     .stroke(ColourRole::accent, 3.0F, false)
                                     .frame({.width = 64.0F, .height = 64.0F})),
      labelled("Elliptical corners",
               components::ShapeView(
                   theme::RoundedRectangle(
                       composition::Size{.width = 30.0F, .height = 12.0F},
                       theme::RoundedCornerStyle::circular))
                   .fill(ColourRole::accent)
                   .frame({.width = 96.0F, .height = 64.0F})));
}

auto CanvasPage::porter_duff() -> Element {
  using Mode = components::GraphicsContext::BlendMode;
  const auto drawn = [](Mode mode) {
    return Element{
        components::Canvas([mode](components::GraphicsContext& context,
                                  composition::Size size) {
          const float half = size.width * 0.5F;
          context.fill(
              graphics::Path{composition::Rect{
                  .x = 0.0F, .y = 0.0F, .width = half, .height = size.height}},
              graphics::ShapeStyle{theme::Colour::hex(0x007AFF)});
          context.set_blend_mode(mode);
          context.fill(
              graphics::Path{composition::Rect{.x = half * 0.5F,
                                               .y = size.height * 0.25F,
                                               .width = half,
                                               .height = size.height * 0.5F}},
              graphics::ShapeStyle{theme::Colour{1.0F, 0.231F, 0.188F, 0.8F}});
        }).frame({.width = 64.0F, .height = 64.0F})};
  };
  return components::HStack(
      components::HStackOptions{.spacing = theme::space::md, .wrap = true},
      labelled("Source in", drawn(Mode::source_in)),
      labelled("Destination out", drawn(Mode::destination_out)),
      labelled("Source atop", drawn(Mode::source_atop)),
      labelled("Xor", drawn(Mode::exclusive_or)),
      labelled("Destination over", drawn(Mode::destination_over)));
}

auto CanvasPage::filters() -> Element {
  using Context = components::GraphicsContext;
  const auto disc = [](Context& context, composition::Size size) {
    context.fill(graphics::Path::ellipse_in(
                     composition::Rect{.x = 8.0F,
                                       .y = 8.0F,
                                       .width = size.width - 24.0F,
                                       .height = size.height - 24.0F}),
                 graphics::ShapeStyle{theme::Colour::hex(0x34C759)});
  };
  return components::HStack(
      components::HStackOptions{.spacing = theme::space::md, .wrap = true},
      labelled(
          "Shadow",
          components::Canvas([disc](Context& context, composition::Size size) {
            context.add_filter(Context::Filter::shadow(
                theme::Colour{0.0F, 0.0F, 0.0F, 0.5F}, 2.0F, 6.0F, 6.0F));
            disc(context, size);
          }).frame({.width = 64.0F, .height = 64.0F})),
      labelled(
          "Inverse clip",
          components::Canvas([disc](Context& context, composition::Size size) {
            context.clip(
                graphics::Path{composition::Rect{.x = 24.0F,
                                                 .y = 0.0F,
                                                 .width = 16.0F,
                                                 .height = size.height}},
                graphics::FillStyle{}, Context::ClipOptions::inverse);
            disc(context, size);
          }).frame({.width = 64.0F, .height = 64.0F})),
      labelled("Repeating ramp",
               components::Canvas([](Context& context, composition::Size size) {
                 context.fill(
                     graphics::Path{composition::Rect{.x = 0.0F,
                                                      .y = 0.0F,
                                                      .width = size.width,
                                                      .height = size.height}},
                     Context::Shading::linear_gradient(
                         graphics::Gradient{std::vector<theme::ColourValue>{
                             theme::Colour::hex(0xFF2D55),
                             theme::Colour::hex(0xFFCC00)}},
                         {.x = 0.0F, .y = 0.0F}, {.x = 16.0F, .y = 0.0F},
                         Context::GradientOptions::mirror));
               }).frame({.width = 64.0F, .height = 64.0F})));
}

auto CanvasPage::shader_arrays() -> Element {
  const std::array<float, 4> weights{1.0F, 2.0F, 3.0F, 2.0F};
  const std::array<theme::Colour, 4> palette{
      theme::Colour::hex(0xFF3B30), theme::Colour::hex(0xFF9500),
      theme::Colour::hex(0x34C759), theme::Colour::hex(0x007AFF)};
  graphics::Shader shader =
      graphics::Shader{GalleryShaders::bands()}
          .argument(graphics::Shader::Argument::float_array(weights))
          .argument(graphics::Shader::Argument::color_array(palette))
          .argument(graphics::Shader::Argument::bounding_rect());
  return labelled("Weighted bands",
                  components::ShapeView(theme::RoundedRectangle(12.0F))
                      .fill(std::move(shader))
                      .frame({.width = 240.0F, .height = 48.0F}));
}

auto CanvasPage::colours() -> Element {
  const theme::ColourValue red = theme::Colour::hex(0xFF0000);
  const theme::ColourValue blue = theme::Colour::hex(0x0000FF);
  return components::HStack(
      components::HStackOptions{.spacing = theme::space::md, .wrap = true},
      labelled("Perceptual", swatch(red.mix(blue, 0.5F))),
      labelled("Device",
               swatch(red.mix(blue, 0.5F, theme::ColourInterpolation::device))),
      labelled("Display P3",
               swatch(theme::ColourMix::rgb(theme::RGBColorSpace::display_p3,
                                            0.5F, 0.5F, 0.2F))),
      labelled("Exposure",
               swatch(theme::ColourValue{theme::Colour::hex(0x808080)}
                          .exposure_adjust(-1.0F))),
      labelled("Colour gradient",
               components::ShapeView(theme::RoundedRectangle(10.0F))
                   .fill(graphics::ShapeStyle{graphics::Gradient::standard(
                       theme::Colour::hex(0x5856D6))})
                   .frame({.width = 64.0F, .height = 64.0F})),
      labelled("Bar", components::ShapeView(theme::RoundedRectangle(10.0F))
                          .fill(graphics::ShapeStyle::bar())
                          .frame({.width = 64.0F, .height = 64.0F})));
}

auto CanvasPage::body(composition::Context&) const -> Element {
  const auto shown_flag = shown_;
  return presentation::NavigationStack([shown_flag] {
    return Element{
        components::ScrollView(
            components::VStack(
                components::VStackOptions{
                    .alignment = composition::HorizontalAlignment::leading,
                    .spacing = theme::space::lg},
                card("Path booleans", booleans()),
                card("Circles and strokes", circles()),
                card("Porter-Duff", porter_duff()),
                card("Filters and clips", filters()),
                card("Shader arrays", shader_arrays()),
                card("Colour", colours()))
                .padding(theme::space::lg))
            .navigation_title("Canvas and colour")
            .toolbar(composition::ToolbarItem(
                composition::ToolbarItemPlacement::cancellation_action,
                components::Button("Close", [shown_flag] {
                  shown_flag.set(false);
                }).id(std::string{close_button})))};
  });
}

auto CanvasPage::Opener::body(composition::Context& context) const -> Element {
  const auto shown = context.state(shown_key);
  return components::Button("Canvas and colour", [shown] { shown.set(true); })
      .id(std::string{button})
      .full_screen_cover(shown, [shown] { return Element{CanvasPage{shown}}; });
}

}
