#pragma once

#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/graphics/affine_transform.hpp>
#include <arisa/graphics/any_shape.hpp>
#include <arisa/graphics/path.hpp>
#include <arisa/graphics/shader.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/graphics/stroke_style.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/shape.hpp>

#include <memory>
#include <optional>
#include <variant>

namespace arisa::components {

class ShapeView : public composition::Element {
 public:
  class Content;

  using Source = std::variant<graphics::Path, theme::Shape, graphics::AnyShape>;

  explicit ShapeView(graphics::Path path);

  explicit ShapeView(theme::Shape shape);

  explicit ShapeView(graphics::AnyShape shape);

  template <graphics::PathShape S>
  explicit ShapeView(S shape)
      : ShapeView(graphics::AnyShape{std::move(shape)}) {}

  auto inset(float amount) && -> ShapeView;

  auto scale(
      float x, float y,
      motion::UnitPoint anchor = motion::UnitPoint::center) && -> ShapeView;

  auto scale(float amount, motion::UnitPoint anchor =
                               motion::UnitPoint::center) && -> ShapeView;

  auto rotation(
      motion::Angle angle,
      motion::UnitPoint anchor = motion::UnitPoint::center) && -> ShapeView;

  auto transform(const graphics::AffineTransform& transform) && -> ShapeView;

  auto trim(double from = 0.0, double to = 1.0) && -> ShapeView;

  auto trim_along(graphics::Path guide, float reach) && -> ShapeView;

  auto fill() && -> ShapeView;

  auto fill(graphics::ShapeStyle style,
            graphics::FillStyle fill_style = {}) && -> ShapeView;

  auto fill(graphics::Shader shader,
            graphics::FillStyle fill_style = {}) && -> composition::Element;

  auto stroke(graphics::ShapeStyle style, graphics::StrokeStyle stroke_style,
              bool antialiased = true) && -> ShapeView;

  auto stroke(graphics::ShapeStyle style, float line_width = 1.0F,
              bool antialiased = true) && -> ShapeView;

  auto stroke_border(graphics::ShapeStyle style,
                     graphics::StrokeStyle stroke_style,
                     bool antialiased = true) && -> ShapeView;

  auto stroke_border(graphics::ShapeStyle style, float line_width = 1.0F,
                     bool antialiased = true) && -> ShapeView;

  auto glass_effect(
      theme::Glass glass = theme::Glass::regular()) && -> ShapeView;

 private:
  auto any_shape() const -> graphics::AnyShape;

  auto layered() && -> ShapeView;

  void blend_like(const graphics::ShapeStyle& style);
};

class ShapeView::Content final : public composition::ElementContent {
 public:
  explicit Content(Source source) noexcept
      : source_(std::make_shared<const Source>(std::move(source))) {}

  auto type() const noexcept -> composition::ElementType override;

  auto framed() const noexcept -> bool override { return true; }

  auto same_as(const composition::ElementContent& other) const -> bool override;

  auto paints_like(const composition::ElementContent& other) const
      -> bool override;

  auto source() const noexcept -> const Source& { return *source_; }

  void set_source(Source source) {
    source_ = std::make_shared<const Source>(std::move(source));
  }

  auto inset() const noexcept -> float { return inset_; }

  void set_inset(float amount) noexcept { inset_ = amount; }

  auto strokes_border() const noexcept -> bool { return border_; }

  auto shared_source() const noexcept -> const std::shared_ptr<const Source>& {
    return source_;
  }

  auto style() const noexcept -> const std::optional<graphics::ShapeStyle>& {
    return style_;
  }

  auto stroke_style() const noexcept
      -> const std::optional<graphics::StrokeStyle>& {
    return stroke_;
  }

  auto fill_style() const noexcept -> graphics::FillStyle { return fill_; }

  auto is_antialiased() const noexcept -> bool { return fill_.antialiased; }

  auto painted() const noexcept -> bool { return painted_; }

  auto has_background() const noexcept -> bool { return under_ != nullptr; }

  auto under() const noexcept -> const Content* { return under_.get(); }

  void push_under() { under_ = std::make_shared<const Content>(*this); }

  auto trim_from() const noexcept -> double { return from_; }

  auto trim_to() const noexcept -> double { return to_; }

  auto follows_glass() const noexcept -> bool { return glass_; }

  auto guide() const noexcept -> const std::shared_ptr<const graphics::Path>& {
    return guide_;
  }

  auto reach() const noexcept -> float { return reach_; }

  void set_guide(graphics::Path guide, float reach) {
    guide_ = std::make_shared<const graphics::Path>(std::move(guide));
    reach_ = reach;
  }

  void set_trim(double from, double to) noexcept {
    from_ = from;
    to_ = to;
  }

  void set_fill(std::optional<graphics::ShapeStyle> style,
                graphics::FillStyle fill) {
    style_ = std::move(style);
    stroke_.reset();
    border_ = false;
    fill_ = fill;
    painted_ = true;
  }

  void set_stroke(graphics::ShapeStyle style, graphics::StrokeStyle stroke,
                  bool border = false, bool antialiased = true) {
    style_ = std::move(style);
    stroke_ = std::move(stroke);
    border_ = border;
    fill_ = graphics::FillStyle{.eo_fill = false, .antialiased = antialiased};
    painted_ = true;
  }

  void set_glass() noexcept { glass_ = true; }

  friend auto operator==(const Content& left, const Content& right) -> bool {
    return (left.source_ == right.source_ || *left.source_ == *right.source_) &&
           left.style_ == right.style_ && left.stroke_ == right.stroke_ &&
           left.fill_ == right.fill_ && left.from_ == right.from_ &&
           left.to_ == right.to_ && left.glass_ == right.glass_ &&
           (left.guide_ == right.guide_ ||
            (left.guide_ != nullptr && right.guide_ != nullptr &&
             *left.guide_ == *right.guide_)) &&
           left.reach_ == right.reach_ && left.inset_ == right.inset_ &&
           left.border_ == right.border_ && left.painted_ == right.painted_ &&
           (left.under_ == right.under_ ||
            (left.under_ != nullptr && right.under_ != nullptr &&
             *left.under_ == *right.under_));
  }

 private:
  std::shared_ptr<const Source> source_;
  std::optional<graphics::ShapeStyle> style_;
  std::optional<graphics::StrokeStyle> stroke_;
  graphics::FillStyle fill_{};
  double from_{0.0};
  double to_{1.0};
  bool glass_{false};
  std::shared_ptr<const graphics::Path> guide_;
  float reach_{0.0F};
  float inset_{0.0F};
  bool border_{false};
  bool painted_{false};
  std::shared_ptr<const Content> under_;
};

}
