#include <arisa/graphics/any_shape.hpp>
#include <arisa/graphics/combined_shape.hpp>
#include <arisa/graphics/offset_shape.hpp>
#include <arisa/graphics/rotated_shape.hpp>
#include <arisa/graphics/scaled_shape.hpp>
#include <arisa/graphics/sized_shape.hpp>
#include <arisa/graphics/transformed_shape.hpp>

#include <algorithm>
#include <type_traits>

namespace arisa::graphics {

auto AnyShape::inset_rect(composition::Rect rect, float amount) noexcept
    -> composition::Rect {
  const float x = std::min(amount, rect.width * 0.5F);
  const float y = std::min(amount, rect.height * 0.5F);
  return composition::Rect{.x = rect.x + x,
                           .y = rect.y + y,
                           .width = rect.width - (2.0F * x),
                           .height = rect.height - (2.0F * y)};
}

auto AnyShape::path(composition::Rect rect,
                    const theme::RectangleCornerRadii& radii) const -> Path {
  return std::visit(
      [&](const auto& each) -> Path {
        using T = std::remove_cvref_t<decltype(each)>;
        if constexpr (std::same_as<T, Path>) {
          return each;
        } else if constexpr (std::same_as<T, theme::Shape>) {
          const auto less = [this](float radius) {
            return std::max(radius - inset_, 0.0F);
          };
          theme::RectangleCornerRadii own = radii;
          if (own == theme::RectangleCornerRadii{}) {
            if (each.form == theme::ShapeForm::uneven) {
              own = each.corner_radii;
            } else if (const float* points = each.radius.points()) {
              own = theme::RectangleCornerRadii{.top_leading = *points,
                                                .bottom_leading = *points,
                                                .bottom_trailing = *points,
                                                .top_trailing = *points};
            } else if (const theme::RadiusRole* role = each.radius.role();
                       role != nullptr && *role == theme::RadiusRole::capsule) {
              const float half = std::min(rect.width, rect.height) * 0.5F;
              own = theme::RectangleCornerRadii{.top_leading = half,
                                                .bottom_leading = half,
                                                .bottom_trailing = half,
                                                .top_trailing = half};
            }
          }
          return Path::shaped(each, inset_rect(rect, inset_),
                              theme::RectangleCornerRadii{
                                  .top_leading = less(own.top_leading),
                                  .bottom_leading = less(own.bottom_leading),
                                  .bottom_trailing = less(own.bottom_trailing),
                                  .top_trailing = less(own.top_trailing)});
        } else {
          return each->path(inset_rect(rect, inset_), radii);
        }
      },
      value_);
}

auto AnyShape::leaf() const -> std::optional<theme::Shape> {
  if (const auto* shape = std::get_if<theme::Shape>(&value_)) {
    return *shape;
  }
  if (const auto* model = std::get_if<std::shared_ptr<const Model>>(&value_)) {
    return (*model)->leaf();
  }
  return std::nullopt;
}

auto AnyShape::layout_direction_behavior() const
    -> composition::LayoutDirectionBehavior {
  if (const auto* shape = std::get_if<theme::Shape>(&value_)) {
    return shape->mirrors() ? composition::LayoutDirectionBehavior::mirrors()
                            : composition::LayoutDirectionBehavior::fixed();
  }
  if (const auto* model = std::get_if<std::shared_ptr<const Model>>(&value_)) {
    return (*model)->layout_direction_behavior();
  }
  return composition::LayoutDirectionBehavior::mirrors();
}

auto AnyShape::path_mirrors_in(composition::LayoutDirection direction) const
    -> bool {
  return !std::holds_alternative<theme::Shape>(value_) &&
         layout_direction_behavior().mirrored_in(direction);
}

auto AnyShape::inset(float amount) const -> AnyShape {
  if (const auto* model = std::get_if<std::shared_ptr<const Model>>(&value_)) {
    if (std::shared_ptr<const Model> inset = (*model)->inset(amount)) {
      return AnyShape{std::move(inset), inset_};
    }
  }
  AnyShape inset = *this;
  inset.inset_ += amount;
  return inset;
}

auto AnyShape::scale(float x, float y, motion::UnitPoint anchor) const
    -> AnyShape {
  return AnyShape{
      ScaledShape{*this, composition::Size{.width = x, .height = y}, anchor}};
}

auto AnyShape::scale(float amount, motion::UnitPoint anchor) const -> AnyShape {
  return scale(amount, amount, anchor);
}

auto AnyShape::rotation(motion::Angle angle, motion::UnitPoint anchor) const
    -> AnyShape {
  return AnyShape{RotatedShape{*this, angle, anchor}};
}

auto AnyShape::offset(float x, float y) const -> AnyShape {
  return AnyShape{
      OffsetShape{*this, composition::Size{.width = x, .height = y}}};
}

auto AnyShape::transform(const AffineTransform& transform) const -> AnyShape {
  return AnyShape{TransformedShape{*this, transform}};
}

auto AnyShape::union_with(AnyShape other, bool eo_fill) const -> AnyShape {
  return AnyShape{CombinedShape{*this, std::move(other),
                                CombinedShape::Operation::union_with, eo_fill}};
}

auto AnyShape::intersection(AnyShape other, bool eo_fill) const -> AnyShape {
  return AnyShape{CombinedShape{*this, std::move(other),
                                CombinedShape::Operation::intersection,
                                eo_fill}};
}

auto AnyShape::subtracting(AnyShape other, bool eo_fill) const -> AnyShape {
  return AnyShape{CombinedShape{
      *this, std::move(other), CombinedShape::Operation::subtracting, eo_fill}};
}

auto AnyShape::symmetric_difference(AnyShape other, bool eo_fill) const
    -> AnyShape {
  return AnyShape{CombinedShape{*this, std::move(other),
                                CombinedShape::Operation::symmetric_difference,
                                eo_fill}};
}

auto AnyShape::line_intersection(AnyShape other, bool eo_fill) const
    -> AnyShape {
  return AnyShape{CombinedShape{*this, std::move(other),
                                CombinedShape::Operation::line_intersection,
                                eo_fill}};
}

auto AnyShape::line_subtraction(AnyShape other, bool eo_fill) const
    -> AnyShape {
  return AnyShape{CombinedShape{*this, std::move(other),
                                CombinedShape::Operation::line_subtraction,
                                eo_fill}};
}

auto AnyShape::size(composition::Size size) const -> AnyShape {
  return AnyShape{SizedShape{*this, size}};
}

auto AnyShape::size(composition::Size size, motion::UnitPoint anchor) const
    -> AnyShape {
  return AnyShape{SizedShape{*this, size, anchor}};
}

auto AnyShape::size(float width, float height) const -> AnyShape {
  return size(composition::Size{.width = width, .height = height});
}

auto AnyShape::size(float width, float height, motion::UnitPoint anchor) const
    -> AnyShape {
  return size(composition::Size{.width = width, .height = height}, anchor);
}

auto AnyShape::role() const -> ShapeRole {
  if (const auto* model = std::get_if<std::shared_ptr<const Model>>(&value_)) {
    return (*model)->role();
  }
  return ShapeRole::fill;
}

auto AnyShape::corners(std::optional<composition::Size> size) const
    -> std::optional<theme::RoundedRectangularShapeCorners> {
  if (const auto* shape = std::get_if<theme::Shape>(&value_)) {
    return shape->corners(size);
  }
  if (const auto* model = std::get_if<std::shared_ptr<const Model>>(&value_)) {
    return (*model)->corners(size);
  }
  return std::nullopt;
}

auto operator==(const AnyShape& left, const AnyShape& right) -> bool {
  if (left.inset_ != right.inset_ ||
      left.value_.index() != right.value_.index()) {
    return false;
  }
  if (const auto* model =
          std::get_if<std::shared_ptr<const AnyShape::Model>>(&left.value_)) {
    const auto& other =
        std::get<std::shared_ptr<const AnyShape::Model>>(right.value_);
    return *model == other || (*model)->equals(*other);
  }
  return left.value_ == right.value_;
}

}
