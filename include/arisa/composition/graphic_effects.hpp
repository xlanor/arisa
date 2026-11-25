#pragma once

#include <arisa/composition/size.hpp>
#include <arisa/graphics/blend_mode.hpp>
#include <arisa/graphics/colour_matrix.hpp>
#include <arisa/graphics/projection_transform.hpp>
#include <arisa/graphics/rotation_3d.hpp>
#include <arisa/graphics/shader.hpp>
#include <arisa/theme/colour_value.hpp>

#include <optional>
#include <variant>
#include <vector>

namespace arisa::composition {

struct GraphicEffects {
  using ColourAdjustment =
      std::variant<graphics::ColourMatrix, theme::ColourValue>;
  using Projection =
      std::variant<graphics::Rotation3D, graphics::ProjectionTransform>;

  std::vector<ColourAdjustment> colour{};
  bool premultiplied_colour{false};
  bool linear_colour{false};
  float blur_radius{0.0F};
  bool blur_opaque{false};
  bool blurs{false};
  graphics::BlendMode blend{graphics::BlendMode::normal};
  bool group{false};
  bool extends{false};
  std::vector<Projection> projections{};
  std::optional<graphics::Shader> shader{};
  composition::Size sample_offset{};

  auto empty() const noexcept -> bool {
    return colour.empty() && blur_radius <= 0.0F &&
           blend == graphics::BlendMode::normal && !group && !extends &&
           projections.empty() && !shader.has_value();
  }

  friend auto operator==(const GraphicEffects&, const GraphicEffects&)
      -> bool = default;
};

}
