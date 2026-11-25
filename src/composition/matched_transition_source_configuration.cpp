#include <arisa/composition/matched_transition_source_configuration.hpp>

#include <arisa/theme/colour.hpp>

#include <utility>

namespace arisa::composition {

auto MatchedTransitionSourceConfiguration::background(
    graphics::ShapeStyle style) const -> MatchedTransitionSourceConfiguration {
  MatchedTransitionSourceConfiguration next = *this;
  next.background_ = std::move(style);
  return next;
}

auto MatchedTransitionSourceConfiguration::clip_shape(theme::Shape shape) const
    -> MatchedTransitionSourceConfiguration {
  MatchedTransitionSourceConfiguration next = *this;
  next.clip_ = shape;
  return next;
}

auto MatchedTransitionSourceConfiguration::shadow(float radius, float x,
                                                  float y) const
    -> MatchedTransitionSourceConfiguration {
  return shadow(
      theme::ColourValue{theme::Colour{0.0F, 0.0F, 0.0F, default_shadow_alpha}},
      radius, x, y);
}

auto MatchedTransitionSourceConfiguration::shadow(theme::ColourValue colour,
                                                  float radius, float x,
                                                  float y) const
    -> MatchedTransitionSourceConfiguration {
  MatchedTransitionSourceConfiguration next = *this;
  next.shadow_ =
      Shadow{.colour = std::move(colour), .radius = radius, .x = x, .y = y};
  return next;
}

}
