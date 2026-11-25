#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/shape_view.hpp>
#include <arisa/components/spacer.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/controls/gauge_style.hpp>
#include <arisa/controls/progress_view_style.hpp>
#include <arisa/graphics/path.hpp>
#include <arisa/graphics/stroke_style.hpp>
#include <arisa/motion/angle.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/typography.hpp>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>
#include <vector>

namespace arisa::controls {

const composition::EnvironmentKey<GaugeStyle> GaugeStyle::key{
    "gauge_style", GaugeStyle::automatic()};

auto GaugeStyle::automatic() noexcept -> GaugeStyle {
  return GaugeStyle{Kind::automatic};
}

auto GaugeStyle::linear_capacity() noexcept -> GaugeStyle {
  return GaugeStyle{Kind::linear_capacity};
}

auto GaugeStyle::linear_capacity(graphics::ShapeStyle tint) -> GaugeStyle {
  return GaugeStyle{Kind::linear_capacity, std::move(tint)};
}

auto GaugeStyle::accessory_linear() noexcept -> GaugeStyle {
  return GaugeStyle{Kind::accessory_linear};
}

auto GaugeStyle::accessory_linear_capacity() noexcept -> GaugeStyle {
  return GaugeStyle{Kind::accessory_linear_capacity};
}

auto GaugeStyle::accessory_circular() noexcept -> GaugeStyle {
  return GaugeStyle{Kind::accessory_circular};
}

auto GaugeStyle::accessory_circular(graphics::ShapeStyle tint) -> GaugeStyle {
  return GaugeStyle{Kind::accessory_circular, std::move(tint)};
}

auto GaugeStyle::accessory_circular_capacity() noexcept -> GaugeStyle {
  return GaugeStyle{Kind::accessory_circular_capacity};
}

auto operator==(const GaugeStyle& left, const GaugeStyle& right) -> bool {
  if (left.kind_ != right.kind_ || left.tint_ != right.tint_) {
    return false;
  }
  return left.kind_ != GaugeStyle::Kind::custom ||
         left.custom_->equals(*right.custom_);
}

auto GaugeStyle::make_body(Configuration configuration) const
    -> composition::Element {
  switch (kind_) {
    case Kind::automatic:
    case Kind::linear_capacity:
      return capacity(std::move(configuration),
                      ProgressViewStyle::bar_thickness, tint_);
    case Kind::accessory_linear_capacity:
      return capacity(std::move(configuration), linear_thickness);
    case Kind::accessory_linear:
      return marked(std::move(configuration));
    case Kind::accessory_circular:
      return circular(std::move(configuration), false, tint_);
    case Kind::accessory_circular_capacity:
      return circular(std::move(configuration), true, tint_);
    case Kind::custom:
      return custom_->make_body(std::move(configuration));
  }
  return capacity(std::move(configuration), ProgressViewStyle::bar_thickness);
}

auto GaugeStyle::ends(Configuration& configuration)
    -> std::optional<composition::Element> {
  if (!configuration.minimum_value_label.has_value() &&
      !configuration.maximum_value_label.has_value() &&
      !configuration.current_value_label.has_value()) {
    return std::nullopt;
  }
  std::vector<composition::Element> row;
  if (configuration.minimum_value_label.has_value()) {
    row.push_back(std::move(*configuration.minimum_value_label));
  }
  row.push_back(components::Spacer());
  if (configuration.current_value_label.has_value()) {
    row.push_back(std::move(*configuration.current_value_label));
    row.push_back(components::Spacer());
  }
  if (configuration.maximum_value_label.has_value()) {
    row.push_back(std::move(*configuration.maximum_value_label));
  }
  return composition::Element{
      components::HStack(
          components::HStackOptions{
              .alignment = components::VerticalAlignment::center,
              .spacing = theme::space::xs},
          std::move(row))
          .font(theme::Font::caption)
          .foreground_style(theme::ColourRole::text_secondary)
          .frame({.max_width = composition::infinity})};
}

auto GaugeStyle::capacity(Configuration configuration, float thickness,
                          const std::optional<graphics::ShapeStyle>& tint)
    -> composition::Element {
  const auto filled = static_cast<float>(configuration.value);
  std::vector<composition::Element> parts;
  if (!components::Group::is_empty(configuration.label)) {
    parts.push_back(std::move(configuration.label));
  }
  parts.push_back(
      components::HStack(components::HStackOptions{.spacing = 0.0F},
                         components::VStack()
                             .flex_grow(filled)
                             .frame({.max_height = composition::infinity})
                             .background(tint.value_or(graphics::ShapeStyle{
                                             theme::ColourRole::accent}),
                                         theme::Shape::capsule()),
                         components::VStack().flex_grow(1.0F - filled))
          .frame({.height = thickness, .max_width = composition::infinity})
          .background(theme::ColourRole::system_fill, theme::Shape::capsule())
          .clip_shape(theme::Shape::capsule()));
  if (std::optional<composition::Element> row = ends(configuration)) {
    parts.push_back(std::move(*row));
  }
  return components::VStack(
             components::VStackOptions{
                 .alignment = components::HorizontalAlignment::leading,
                 .spacing = theme::space::xs},
             std::move(parts))
      .frame({.max_width = composition::infinity,
              .alignment = composition::Alignment::leading});
}

auto GaugeStyle::marked(Configuration configuration) -> composition::Element {
  const auto at = static_cast<float>(configuration.value);
  composition::Element track =
      components::ZStack(
          components::ZStack()
              .frame({.height = linear_thickness,
                      .max_width = composition::infinity})
              .background(theme::ColourRole::system_fill,
                          theme::Shape::capsule()),
          components::HStack(
              components::HStackOptions{
                  .alignment = components::VerticalAlignment::center,
                  .spacing = 0.0F},
              components::VStack().flex_grow(at),
              components::ZStack()
                  .frame({.width = marker_extent, .height = marker_extent})
                  .background(theme::ColourRole::accent,
                              theme::Shape::capsule()),
              components::VStack().flex_grow(1.0F - at))
              .frame({.max_width = composition::infinity}))
          .frame({.max_width = composition::infinity});
  std::vector<composition::Element> row;
  if (configuration.minimum_value_label.has_value()) {
    row.push_back(std::move(*configuration.minimum_value_label)
                      .font(theme::Font::caption));
  }
  row.push_back(std::move(track));
  if (configuration.maximum_value_label.has_value()) {
    row.push_back(std::move(*configuration.maximum_value_label)
                      .font(theme::Font::caption));
  }
  std::vector<composition::Element> parts;
  if (!components::Group::is_empty(configuration.label)) {
    parts.push_back(std::move(configuration.label));
  }
  parts.push_back(components::HStack(
                      components::HStackOptions{
                          .alignment = components::VerticalAlignment::center,
                          .spacing = theme::space::xs},
                      std::move(row))
                      .frame({.max_width = composition::infinity}));
  return components::VStack(
             components::VStackOptions{
                 .alignment = components::HorizontalAlignment::leading,
                 .spacing = theme::space::xs},
             std::move(parts))
      .frame({.max_width = composition::infinity,
              .alignment = composition::Alignment::leading});
}

auto GaugeStyle::circular(Configuration configuration, bool capacity,
                          const std::optional<graphics::ShapeStyle>& tint)
    -> composition::Element {
  const graphics::ShapeStyle ink =
      tint.value_or(graphics::ShapeStyle{theme::ColourRole::accent});
  const float radius = (circular_extent - circular_thickness) * 0.5F;
  const composition::Point centre{.x = circular_extent * 0.5F,
                                  .y = circular_extent * 0.5F};
  const double start = capacity ? -90.0 : circular_start_degrees;
  const double sweep = capacity ? 360.0 : circular_sweep_degrees;
  const double reached = start + (sweep * configuration.value);
  const graphics::StrokeStyle stroke{.line_width = circular_thickness,
                                     .line_cap = graphics::LineCap::round};
  graphics::Path track;
  track.add_arc(centre, radius, motion::Angle::degrees(start),
                motion::Angle::degrees(start + sweep), false);
  std::vector<composition::Element> layers;
  layers.push_back(
      components::ShapeView(std::move(track))
          .stroke(theme::ColourRole::system_fill, stroke)
          .frame({.width = circular_extent, .height = circular_extent}));
  if (capacity) {
    if (configuration.value > 0.0) {
      graphics::Path filled;
      filled.add_arc(centre, radius, motion::Angle::degrees(start),
                     motion::Angle::degrees(reached), false);
      layers.push_back(
          components::ShapeView(std::move(filled))
              .stroke(ink, stroke)
              .frame({.width = circular_extent, .height = circular_extent}));
    }
  } else {
    const double angle = reached * std::numbers::pi / 180.0;
    const float half = marker_extent * 0.5F;
    graphics::Path dot;
    dot.add_ellipse(composition::Rect{
        .x = centre.x + (radius * static_cast<float>(std::cos(angle))) - half,
        .y = centre.y + (radius * static_cast<float>(std::sin(angle))) - half,
        .width = marker_extent,
        .height = marker_extent});
    layers.push_back(
        components::ShapeView(std::move(dot))
            .fill(ink)
            .frame({.width = circular_extent, .height = circular_extent}));
  }
  if (configuration.current_value_label.has_value()) {
    layers.push_back(std::move(*configuration.current_value_label)
                         .font(theme::Font::title3));
  } else if (!components::Group::is_empty(configuration.label)) {
    layers.push_back(std::move(configuration.label).font(theme::Font::caption));
  }
  composition::Element dial =
      components::ZStack(components::ZStackOptions{}, std::move(layers))
          .frame({.width = circular_extent, .height = circular_extent});
  if (!capacity && (configuration.minimum_value_label.has_value() ||
                    configuration.maximum_value_label.has_value())) {
    std::vector<composition::Element> row;
    if (configuration.minimum_value_label.has_value()) {
      row.push_back(std::move(*configuration.minimum_value_label));
    }
    row.push_back(components::Spacer());
    if (configuration.maximum_value_label.has_value()) {
      row.push_back(std::move(*configuration.maximum_value_label));
    }
    dial = std::move(dial).overlay(
        composition::Alignment::bottom,
        components::HStack(components::HStackOptions{.spacing = 0.0F},
                           std::move(row))
            .font(theme::Font::caption2)
            .foreground_style(theme::ColourRole::text_secondary)
            .frame({.width = circular_extent}));
  }
  return dial;
}

}
