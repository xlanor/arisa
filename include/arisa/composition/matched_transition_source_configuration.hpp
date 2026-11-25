#pragma once

#include <arisa/graphics/shape_style.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/shape.hpp>

#include <optional>

namespace arisa::composition {

class MatchedTransitionSourceConfiguration {
 public:
  struct Shadow {
    theme::ColourValue colour;
    float radius{0.0F};
    float x{0.0F};
    float y{0.0F};

    friend auto operator==(const Shadow&, const Shadow&) -> bool = default;
  };

  static constexpr float default_shadow_alpha = 0.33F;

  auto background(graphics::ShapeStyle style) const
      -> MatchedTransitionSourceConfiguration;

  auto clip_shape(theme::Shape shape) const
      -> MatchedTransitionSourceConfiguration;

  auto shadow(float radius, float x = 0.0F, float y = 0.0F) const
      -> MatchedTransitionSourceConfiguration;

  auto shadow(theme::ColourValue colour, float radius, float x = 0.0F,
              float y = 0.0F) const -> MatchedTransitionSourceConfiguration;

  auto background_style() const noexcept
      -> const std::optional<graphics::ShapeStyle>& {
    return background_;
  }

  auto clip() const noexcept -> const std::optional<theme::Shape>& {
    return clip_;
  }

  auto cast() const noexcept -> const std::optional<Shadow>& { return shadow_; }

  friend auto operator==(const MatchedTransitionSourceConfiguration&,
                         const MatchedTransitionSourceConfiguration&)
      -> bool = default;

 private:
  std::optional<graphics::ShapeStyle> background_;
  std::optional<theme::Shape> clip_;
  std::optional<Shadow> shadow_;
};

}
