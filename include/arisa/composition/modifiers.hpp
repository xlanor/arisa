#pragma once

#include <arisa/composition/alignment.hpp>
#include <arisa/composition/alignment_guide.hpp>
#include <arisa/composition/aspect_ratio.hpp>
#include <arisa/composition/axes.hpp>
#include <arisa/composition/container_relative_frame.hpp>
#include <arisa/composition/custom_hover_effect.hpp>
#include <arisa/composition/glass_effect.hpp>
#include <arisa/composition/graphic_effects.hpp>
#include <arisa/composition/grid_cell.hpp>
#include <arisa/composition/keyed_values.hpp>
#include <arisa/composition/layout_direction_behavior.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/graphics/any_shape.hpp>
#include <arisa/graphics/shape_style.hpp>
#include <arisa/graphics/stroke_style.hpp>
#include <arisa/graphics/style_hierarchy.hpp>
#include <arisa/motion/angle.hpp>
#include <arisa/motion/content_transition.hpp>
#include <arisa/motion/hover_effect.hpp>
#include <arisa/motion/motion_path.hpp>
#include <arisa/motion/unit_point.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/elevation.hpp>
#include <arisa/theme/material.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>
#include <arisa/theme/stroke.hpp>

#include <limits>
#include <optional>
#include <vector>

namespace arisa::composition {

inline constexpr float infinity = std::numeric_limits<float>::infinity();

struct FrameSpec {
  std::optional<Length> width{};
  std::optional<Length> height{};
  std::optional<Length> min_width{};
  std::optional<Length> ideal_width{};
  std::optional<Length> max_width{};
  std::optional<Length> min_height{};
  std::optional<Length> ideal_height{};
  std::optional<Length> max_height{};
  Alignment alignment{};

  constexpr auto constrains() const noexcept -> bool {
    return width.has_value() || height.has_value() || min_width.has_value() ||
           ideal_width.has_value() || max_width.has_value() ||
           min_height.has_value() || ideal_height.has_value() ||
           max_height.has_value();
  }

  friend constexpr auto operator==(const FrameSpec&, const FrameSpec&)
      -> bool = default;
};

struct Background {
  graphics::ShapeStyle fill;
  theme::Shape shape;

  friend auto operator==(const Background&, const Background&)
      -> bool = default;
};

struct MaterialBackground {
  theme::Material material;
  theme::Shape shape;

  friend constexpr auto operator==(const MaterialBackground&,
                                   const MaterialBackground&) -> bool = default;
};

struct Border {
  theme::Stroke width;
  theme::ColourValue colour;

  friend constexpr auto operator==(const Border&, const Border&)
      -> bool = default;
};

struct DropShadow {
  theme::ColourValue colour;
  float radius{0.0F};
  float x{0.0F};
  float y{0.0F};

  friend constexpr auto operator==(const DropShadow&, const DropShadow&)
      -> bool = default;
};

struct GeometryEffects {
  float scale_x{1.0F};
  float scale_y{1.0F};
  motion::UnitPoint scale_anchor{motion::UnitPoint::center};
  float offset_x{0.0F};
  float offset_y{0.0F};
  motion::Angle rotation{};
  motion::UnitPoint rotation_anchor{motion::UnitPoint::center};

  friend constexpr auto operator==(const GeometryEffects&,
                                   const GeometryEffects&) -> bool = default;
};

struct CornerOffset {
  theme::Edge edges{theme::Edge::all};
  bool size_to_fit{false};

  friend constexpr auto operator==(const CornerOffset&, const CornerOffset&)
      -> bool = default;
};

struct PathEffect {
  motion::MotionPath path;
  double progress{0.0};

  friend auto operator==(const PathEffect&, const PathEffect&)
      -> bool = default;
};

struct Modifiers {
  std::optional<theme::Insets> padding;
  std::optional<theme::Insets> margin;
  std::optional<float> grow;
  std::optional<float> shrink;
  FrameSpec frame;
  std::optional<Background> background;
  std::optional<MaterialBackground> material;
  std::optional<theme::Shape> corners;
  std::optional<Border> border;
  theme::Elevation shadow{theme::Elevation::none};
  std::optional<DropShadow> drop_shadow;
  std::optional<graphics::StyleHierarchy> foreground;
  std::optional<graphics::ShapeStyle> background_style;
  std::optional<Background> overlay;
  std::optional<theme::Shape> container_shape;
  std::optional<CornerOffset> corner_offset;
  float opacity{1.0F};
  bool clip{false};
  std::optional<graphics::AnyShape> clip_path;
  graphics::FillStyle clip_fill{};
  GeometryEffects effects;
  std::optional<motion::ContentTransition> content_transition;
  std::optional<CustomHoverEffect> hover;
  std::optional<PathEffect> path;
  std::optional<GlassEffect> glass;
  std::optional<AspectRatio> aspect;
  std::optional<GridCell> grid_cell;
  double z_index{0.0};
  double layout_priority{0.0};
  Axes fixed_size{};
  std::optional<ContainerRelativeFrame> relative_frame;
  GraphicEffects graphics;
  bool geometry_group{false};
  KeyedValues layout_values;
  std::vector<AlignmentGuide> alignment_guides;
  bool flips_for_right_to_left{false};
  std::optional<LayoutDirectionBehavior> layout_direction_behavior;

  friend auto operator==(const Modifiers&, const Modifiers&) -> bool = default;
};

}
