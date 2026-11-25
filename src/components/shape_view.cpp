#include <arisa/components/shape_view.hpp>
#include <arisa/composition/glass_effect.hpp>
#include <arisa/composition/length.hpp>

#include <algorithm>
#include <utility>

namespace arisa::components {

ShapeView::ShapeView(graphics::Path path)
    : Element(std::make_unique<Content>(Source{std::move(path)})) {
  edit_modifiers().frame.max_width = composition::infinity;
  edit_modifiers().frame.max_height = composition::infinity;
}

ShapeView::ShapeView(theme::Shape shape)
    : Element(std::make_unique<Content>(Source{shape})) {
  edit_modifiers().frame.max_width = composition::infinity;
  edit_modifiers().frame.max_height = composition::infinity;
}

ShapeView::ShapeView(graphics::AnyShape shape)
    : Element(std::make_unique<Content>(Source{std::move(shape)})) {
  edit_modifiers().frame.max_width = composition::infinity;
  edit_modifiers().frame.max_height = composition::infinity;
}

auto ShapeView::any_shape() const -> graphics::AnyShape {
  const auto& content = static_cast<const Content&>(this->content());
  return std::visit([](const auto& each) { return graphics::AnyShape{each}; },
                    content.source());
}

auto ShapeView::inset(float amount) && -> ShapeView {
  Content& content = edit<Content>();
  content.set_inset(content.inset() + amount);
  return std::move(*this);
}

auto ShapeView::scale(float x, float y,
                      motion::UnitPoint anchor) && -> ShapeView {
  graphics::AnyShape scaled = any_shape().scale(x, y, anchor);
  edit<Content>().set_source(std::move(scaled));
  return std::move(*this);
}

auto ShapeView::scale(float amount, motion::UnitPoint anchor) && -> ShapeView {
  return std::move(*this).scale(amount, amount, anchor);
}

auto ShapeView::rotation(motion::Angle angle,
                         motion::UnitPoint anchor) && -> ShapeView {
  graphics::AnyShape rotated = any_shape().rotation(angle, anchor);
  edit<Content>().set_source(std::move(rotated));
  return std::move(*this);
}

auto ShapeView::transform(
    const graphics::AffineTransform& transform) && -> ShapeView {
  graphics::AnyShape transformed = any_shape().transform(transform);
  edit<Content>().set_source(std::move(transformed));
  return std::move(*this);
}

auto ShapeView::trim_along(graphics::Path guide, float reach) && -> ShapeView {
  edit<Content>().set_guide(std::move(guide), reach);
  return std::move(*this);
}

auto ShapeView::trim(double from, double to) && -> ShapeView {
  edit<Content>().set_trim(std::clamp(from, 0.0, 1.0),
                           std::clamp(to, 0.0, 1.0));
  return std::move(*this);
}

auto ShapeView::layered() && -> ShapeView {
  if (static_cast<const Content&>(this->content()).painted()) {
    edit<Content>().push_under();
  }
  return std::move(*this);
}

void ShapeView::blend_like(const graphics::ShapeStyle& style) {
  if (style.blend_mode() != graphics::BlendMode::normal) {
    edit_modifiers().graphics.blend = style.blend_mode();
  }
}

auto ShapeView::fill() && -> ShapeView {
  ShapeView top = std::move(*this).layered();
  top.edit<Content>().set_fill(std::nullopt, graphics::FillStyle{});
  return top;
}

auto ShapeView::fill(graphics::ShapeStyle style,
                     graphics::FillStyle fill_style) && -> ShapeView {
  ShapeView top = std::move(*this).layered();
  top.blend_like(style);
  top.edit<Content>().set_fill(std::move(style), fill_style);
  return top;
}

auto ShapeView::fill(
    graphics::Shader shader,
    graphics::FillStyle fill_style) && -> composition::Element {
  ShapeView outline{graphics::AnyShape{graphics::Path{}}};
  outline.edit<Content>() = static_cast<const Content&>(this->content());
  outline.edit<Content>().set_fill(theme::Colour{0.0F, 0.0F, 0.0F, 1.0F},
                                   fill_style);
  ShapeView painted =
      std::move(*this).fill(theme::Colour{1.0F, 1.0F, 1.0F, 1.0F}, fill_style);
  return static_cast<composition::Element&&>(std::move(painted))
      .color_effect(std::move(shader))
      .mask(composition::Alignment::center, std::move(outline));
}

auto ShapeView::stroke(graphics::ShapeStyle style,
                       graphics::StrokeStyle stroke_style,
                       bool antialiased) && -> ShapeView {
  ShapeView top = std::move(*this).layered();
  top.blend_like(style);
  top.edit<Content>().set_stroke(std::move(style), std::move(stroke_style),
                                 false, antialiased);
  return top;
}

auto ShapeView::stroke(graphics::ShapeStyle style, float line_width,
                       bool antialiased) && -> ShapeView {
  return std::move(*this).stroke(
      std::move(style), graphics::StrokeStyle{.line_width = line_width},
      antialiased);
}

auto ShapeView::stroke_border(graphics::ShapeStyle style,
                              graphics::StrokeStyle stroke_style,
                              bool antialiased) && -> ShapeView {
  ShapeView top = std::move(*this).layered();
  top.blend_like(style);
  top.edit<Content>().set_stroke(std::move(style), std::move(stroke_style),
                                 true, antialiased);
  return top;
}

auto ShapeView::stroke_border(graphics::ShapeStyle style, float line_width,
                              bool antialiased) && -> ShapeView {
  return std::move(*this).stroke_border(
      std::move(style), graphics::StrokeStyle{.line_width = line_width},
      antialiased);
}

auto ShapeView::glass_effect(theme::Glass glass) && -> ShapeView {
  composition::GlassEffect effect =
      edit_modifiers().glass.value_or(composition::GlassEffect{});
  effect.glass = glass;
  edit_modifiers().glass = effect;
  edit<Content>().set_glass();
  return std::move(*this);
}

auto ShapeView::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto ShapeView::Content::same_as(const composition::ElementContent& other) const
    -> bool {
  return other.type() == type() && static_cast<const Content&>(other) == *this;
}

auto ShapeView::Content::paints_like(
    const composition::ElementContent& other) const -> bool {
  return same_as(other);
}
}

namespace arisa::composition {

auto Element::background(graphics::ShapeStyle style, graphics::AnyShape shape,
                         graphics::FillStyle fill_style) && -> Element {
  if (const std::optional<theme::Shape> leaf = shape.leaf();
      leaf.has_value() && graphics::AnyShape{*leaf} == shape &&
      fill_style == graphics::FillStyle{} && !leaf->elliptical() &&
      style.blend_mode() == graphics::BlendMode::normal) {
    return std::move(*this).background(std::move(style), *leaf);
  }
  return std::move(*this).background(Alignment::center,
                                     components::ShapeView(std::move(shape))
                                         .fill(std::move(style), fill_style));
}

auto Element::background(graphics::AnyShape shape,
                         graphics::FillStyle fill_style) && -> Element {
  return std::move(*this).background(graphics::ShapeStyle::background(),
                                     std::move(shape), fill_style);
}

auto Element::background(graphics::ShapeStyle style,
                         theme::Edge ignores_safe_area_edges) && -> Element {
  return std::move(*this).background(
      Alignment::center, components::ShapeView(theme::Rectangle())
                             .fill(std::move(style))
                             .ignores_safe_area(ignores_safe_area_edges));
}

auto Element::background(theme::Edge ignores_safe_area_edges) && -> Element {
  return std::move(*this).background(graphics::ShapeStyle::background(),
                                     ignores_safe_area_edges);
}

auto Element::overlay(graphics::ShapeStyle style, graphics::AnyShape shape,
                      graphics::FillStyle fill_style) && -> Element {
  if (const std::optional<theme::Shape> leaf = shape.leaf();
      leaf.has_value() && graphics::AnyShape{*leaf} == shape &&
      fill_style == graphics::FillStyle{} && !leaf->elliptical() &&
      style.blend_mode() == graphics::BlendMode::normal) {
    return std::move(*this).overlay(std::move(style), *leaf);
  }
  return std::move(*this).overlay(Alignment::center,
                                  components::ShapeView(std::move(shape))
                                      .fill(std::move(style), fill_style));
}

auto Element::overlay(graphics::ShapeStyle style,
                      theme::Edge ignores_safe_area_edges) && -> Element {
  return std::move(*this).overlay(
      Alignment::center, components::ShapeView(theme::Rectangle())
                             .fill(std::move(style))
                             .ignores_safe_area(ignores_safe_area_edges));
}

auto Element::border(graphics::ShapeStyle style, float width) && -> Element {
  if (const auto colour = style.colour();
      colour.has_value() && style.opacity() == 1.0F && !style.shadow()) {
    return std::move(*this).border(*colour, theme::Stroke{width});
  }
  return std::move(*this).overlay(Alignment::center,
                                  components::ShapeView(theme::Rectangle())
                                      .stroke_border(std::move(style), width)
                                      .allows_hit_testing(false));
}

}
