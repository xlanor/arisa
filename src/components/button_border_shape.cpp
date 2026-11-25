#include <arisa/components/button_border_shape.hpp>
#include <arisa/components/control_metrics.hpp>
#include <arisa/graphics/any_shape.hpp>
#include <arisa/theme/metrics.hpp>

namespace arisa::components {

auto ButtonBorderShape::shape(ControlSize size) const noexcept -> theme::Shape {
  switch (kind_) {
    case Kind::automatic:
      return ControlMetrics::of(size).capsule
                 ? theme::Shape::capsule()
                 : theme::Shape::rounded(theme::radius::control, true);
    case Kind::capsule:
    case Kind::circle:
      return theme::Shape::capsule();
    case Kind::rounded_rectangle:
      return theme::Shape::rounded(radius_.has_value() ? theme::Radius{*radius_}
                                                       : theme::radius::control,
                                   true);
  }
  return theme::Shape::rounded(theme::radius::control, true);
}

auto ButtonBorderShape::path(composition::Rect rect, ControlSize size) const
    -> graphics::Path {
  theme::Shape outline = shape(size);
  if (outline.radius.role() != nullptr &&
      *outline.radius.role() == theme::RadiusRole::control) {
    outline.radius = theme::Radius{theme::metrics::button.corner_radius};
  }
  return graphics::AnyShape{outline}.inset(inset_).path(rect);
}

}
