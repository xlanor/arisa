#include <arisa/controls/text_input_border_shape.hpp>

#include <arisa/theme/metrics.hpp>

#include <algorithm>

namespace arisa::controls {

const composition::EnvironmentKey<TextInputBorderShape>
    TextInputBorderShape::key{"text_input_border_shape",
                              TextInputBorderShape::automatic()};

auto TextInputBorderShape::shape() const noexcept -> theme::Shape {
  switch (kind_) {
    case Kind::capsule:
      return theme::Shape::capsule();
    case Kind::automatic:
    case Kind::rounded_rectangle:
      break;
  }
  return theme::Shape::rounded(
      std::max(theme::metrics::text_field.corner_radius - inset_, 0.0F), true);
}

auto TextInputBorderShape::path(composition::Rect rect) const
    -> graphics::Path {
  const composition::Rect inner{
      .x = rect.x + inset_,
      .y = rect.y + inset_,
      .width = std::max(rect.width - (2.0F * inset_), 0.0F),
      .height = std::max(rect.height - (2.0F * inset_), 0.0F)};
  if (kind_ == Kind::capsule) {
    return graphics::Path{inner, std::min(inner.width, inner.height) * 0.5F,
                          theme::RoundedCornerStyle::circular};
  }
  return graphics::Path{
      inner, std::max(theme::metrics::text_field.corner_radius - inset_, 0.0F),
      theme::RoundedCornerStyle::continuous};
}

}
