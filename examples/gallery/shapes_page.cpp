#include <shapes_page.hpp>

#include <arisa/components/button.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/scroll_view.hpp>
#include <arisa/components/shape_view.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/toolbar_item.hpp>
#include <arisa/graphics/gradient.hpp>
#include <arisa/graphics/offset_shape.hpp>
#include <arisa/graphics/shadow_style.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/motion/angle.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/presentation/navigation_stack.hpp>
#include <arisa/presentation/presentations.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/radius.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/typography.hpp>

#include <cmath>
#include <numbers>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace arisa::examples {

using composition::Element;
using composition::infinity;
using graphics::HierarchicalShapeStyle;
using theme::ColourRole;

auto ShapesPage::Star::path(composition::Rect rect) const -> graphics::Path {
  graphics::Path star;
  const float cx = rect.x + (rect.width * 0.5F);
  const float cy = rect.y + (rect.height * 0.5F);
  const float outer = std::min(rect.width, rect.height) * 0.5F;
  const float inner = outer * 0.45F;
  const int corners = points * 2;
  for (int index = 0; index < corners; ++index) {
    const double angle = (std::numbers::pi * static_cast<double>(index) /
                          static_cast<double>(points)) -
                         (std::numbers::pi * 0.5);
    const float reach = index % 2 == 0 ? outer : inner;
    const composition::Point at{
        .x = cx + (reach * static_cast<float>(std::cos(angle))),
        .y = cy + (reach * static_cast<float>(std::sin(angle)))};
    if (index == 0) {
      star.move(at);
    } else {
      star.add_line(at);
    }
  }
  star.close_subpath();
  return star;
}

auto ShapesPage::card(std::string_view title, Element content) -> Element {
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

auto ShapesPage::basics() -> Element {
  return components::HStack(
      components::HStackOptions{.spacing = theme::space::md},
      components::ShapeView(theme::Ellipse{})
          .fill(ColourRole::indigo)
          .frame({.width = 120.0F, .height = 64.0F}),
      components::ShapeView(
          theme::UnevenRoundedRectangle(4.0F, 12.0F, 24.0F, 36.0F))
          .fill(ColourRole::teal)
          .frame({.width = 96.0F, .height = 64.0F}),
      components::ShapeView(theme::Circle{})
          .stroke_border(ColourRole::orange, 8.0F)
          .frame({.width = 64.0F, .height = 64.0F}),
      components::Button("Uneven", [] {})
          .padding(theme::space::md)
          .background(ColourRole::accent,
                      theme::UnevenRoundedRectangle(16.0F, 4.0F, 16.0F, 4.0F))
          .foreground_style(ColourRole::on_accent)
          .id(std::string{uneven_button}));
}

auto ShapesPage::concentric() -> Element {
  return components::ZStack(
             components::VStack(
                 components::VStackOptions{.spacing = theme::space::sm},
                 components::ShapeView(theme::ConcentricRectangle{})
                     .fill(ColourRole::secondary_system_fill)
                     .frame({.height = 56.0F, .max_width = infinity}),
                 components::HStack(
                     components::HStackOptions{.spacing = theme::space::sm},
                     components::ShapeView(theme::ContainerRelativeShape{})
                         .fill(ColourRole::accent)
                         .frame({.height = 44.0F, .max_width = infinity}),
                     components::ShapeView(
                         theme::ConcentricRectangle(
                             theme::ConcentricCorner::concentric(8.0F), true))
                         .fill(ColourRole::green)
                         .frame({.height = 44.0F, .max_width = infinity})))
                 .padding(theme::space::md))
      .frame({.width = 300.0F})
      .glass_effect(theme::Glass::regular(), theme::RoundedRectangle(28.0F))
      .container_shape(theme::RoundedRectangle(28.0F))
      .id(std::string{concentric_card});
}

auto ShapesPage::well() -> Element {
  return components::HStack(
      components::HStackOptions{.spacing = theme::space::md},
      components::ShapeView(theme::RoundedRectangle(14.0F))
          .fill(graphics::ShapeStyle{ColourRole::secondary_system_background}
                    .shadow(graphics::ShadowStyle::inner(
                        theme::Colour{0.0F, 0.0F, 0.0F, 0.45F}, 6.0F, 0.0F,
                        2.0F)))
          .frame({.width = 140.0F, .height = 72.0F}),
      components::Text("Well")
          .padding(theme::space::lg)
          .background(
              graphics::ShapeStyle{ColourRole::system_background}.shadow(
                  graphics::ShadowStyle::drop(
                      theme::Colour{0.0F, 0.0F, 0.0F, 0.3F}, 6.0F, 0.0F, 3.0F)),
              theme::Capsule{}));
}

auto ShapesPage::gradient_text() -> Element {
  return components::Text("Gradient text")
      .font(theme::Font::large_title)
      .foreground_style(graphics::LinearGradient{
          .gradient = graphics::Gradient{std::vector<theme::ColourValue>{
              ColourRole::pink, ColourRole::orange, ColourRole::yellow}},
          .start_point = motion::UnitPoint::leading,
          .end_point = motion::UnitPoint::trailing});
}

auto ShapesPage::tiers(theme::ColorScheme scheme) -> Element {
  const auto row = [](std::optional<graphics::ShapeStyle> primary) {
    std::vector<Element> swatches;
    for (const HierarchicalShapeStyle level :
         {HierarchicalShapeStyle::primary, HierarchicalShapeStyle::secondary,
          HierarchicalShapeStyle::tertiary, HierarchicalShapeStyle::quaternary,
          HierarchicalShapeStyle::quinary}) {
      swatches.emplace_back(components::ShapeView(theme::RoundedRectangle(6.0F))
                                .fill(level)
                                .frame({.width = 36.0F, .height = 28.0F}));
    }
    Element stacked{components::HStack(
        components::HStackOptions{.spacing = theme::space::xs},
        std::move(swatches))};
    return primary.has_value()
               ? std::move(stacked).foreground_style(std::move(*primary))
               : std::move(stacked);
  };
  return components::VStack(
             components::VStackOptions{
                 .alignment = composition::HorizontalAlignment::leading,
                 .spacing = theme::space::xs},
             row(std::nullopt), row(graphics::ShapeStyle{ColourRole::red}),
             components::Text("Secondary")
                 .foreground_style(HierarchicalShapeStyle::secondary))
      .padding(theme::space::sm)
      .background(ColourRole::window_background, theme::RoundedRectangle(10.0F))
      .environment(components::Environment::colour_scheme,
                   std::optional<theme::ColorScheme>{scheme});
}

auto ShapesPage::gradients() -> Element {
  return components::HStack(
      components::HStackOptions{.spacing = theme::space::md},
      components::ShapeView(theme::Rectangle{})
          .fill(graphics::EllipticalGradient{
              .gradient = graphics::Gradient{std::vector<theme::ColourValue>{
                  ColourRole::yellow, ColourRole::red, ColourRole::indigo}}})
          .frame({.width = 140.0F, .height = 72.0F}),
      components::Text("Background")
          .padding(theme::space::lg)
          .background(
              graphics::LinearGradient{
                  .gradient =
                      graphics::Gradient{std::vector<theme::ColourValue>{
                          ColourRole::mint, ColourRole::blue}},
                  .start_point = motion::UnitPoint::top_leading,
                  .end_point = motion::UnitPoint::bottom_trailing},
              theme::RoundedRectangle(12.0F))
          .foreground_style(theme::Colour::hex(0xFFFFFF)));
}

auto ShapesPage::transforms() -> Element {
  const auto star = [](Element shape) {
    return std::move(shape).frame({.width = 64.0F, .height = 64.0F});
  };
  return components::HStack(
      components::HStackOptions{.spacing = theme::space::md},
      star(Element{components::ShapeView(Star{}).fill(ColourRole::yellow)}),
      star(Element{
          components::ShapeView(Star{}).scale(0.6F).fill(ColourRole::orange)}),
      star(Element{components::ShapeView(Star{6})
                       .rotation(motion::Angle::degrees(30.0))
                       .fill(ColourRole::pink)}),
      star(Element{
          components::ShapeView(
              graphics::OffsetShape{
                  Star{}, composition::Size{.width = 6.0F, .height = 6.0F}})
              .fill(ColourRole::purple)}),
      star(Element{components::ShapeView(theme::RoundedRectangle(12.0F))
                       .inset(8.0F)
                       .fill(ColourRole::cyan)}));
}

auto ShapesPage::styles() -> Element {
  return components::HStack(
      components::HStackOptions{.spacing = theme::space::md},
      components::Text("Overlay")
          .padding(theme::space::lg)
          .background(ColourRole::secondary_system_fill, theme::Capsule{})
          .overlay(graphics::ShapeStyle{ColourRole::accent}.opacity(0.25F),
                   theme::Ellipse{}),
      components::Text("Style")
          .padding(theme::space::lg)
          .background()
          .background_style(ColourRole::yellow));
}

auto ShapesPage::body(composition::Context&) const -> Element {
  const auto shown_flag = shown_;
  return presentation::NavigationStack([shown_flag] {
    return Element{
        components::ScrollView(
            components::VStack(
                components::VStackOptions{
                    .alignment = composition::HorizontalAlignment::leading,
                    .spacing = theme::space::lg},
                card("Shapes", basics()), card("Concentric", concentric()),
                card("Shadows", well()), card("Gradient", gradient_text()),
                card("Tiers",
                     components::HStack(
                         components::HStackOptions{.spacing = theme::space::md},
                         tiers(theme::ColorScheme::light),
                         tiers(theme::ColorScheme::dark))),
                card("Gradients", gradients()),
                card("Transforms", transforms()), card("Styles", styles()))
                .padding(theme::space::lg))
            .navigation_title("Shapes")
            .toolbar(composition::ToolbarItem(
                composition::ToolbarItemPlacement::cancellation_action,
                components::Button("Close", [shown_flag] {
                  shown_flag.set(false);
                }).id(std::string{close_button})))};
  });
}

auto ShapesPage::Opener::body(composition::Context& context) const -> Element {
  const auto shown = context.state(shown_key);
  return components::Button("Shapes", [shown] { shown.set(true); })
      .id(std::string{button})
      .full_screen_cover(shown, [shown] { return Element{ShapesPage{shown}}; });
}

}
