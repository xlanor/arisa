#include <arisa/components/control_metrics.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/geometry_proxy.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/controls/progress_view_style.hpp>
#include <arisa/motion/angle.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

namespace arisa::controls {

const composition::EnvironmentKey<ProgressViewStyle> ProgressViewStyle::key{
    "progress_view_style", ProgressViewStyle::automatic()};

auto ProgressViewStyle::automatic() noexcept -> ProgressViewStyle {
  return ProgressViewStyle{Kind::automatic};
}

auto ProgressViewStyle::linear() noexcept -> ProgressViewStyle {
  return ProgressViewStyle{Kind::linear};
}

auto ProgressViewStyle::circular() noexcept -> ProgressViewStyle {
  return ProgressViewStyle{Kind::circular};
}

ProgressViewStyle::ProgressViewStyle(SegmentedProgressViewStyle style) noexcept
    : kind_(Kind::segmented), segments_(style.segments()) {}

auto ProgressViewStyle::segmented(std::size_t segments) noexcept
    -> ProgressViewStyle {
  return ProgressViewStyle{SegmentedProgressViewStyle{segments}};
}

auto ProgressViewStyle::current_segment(std::size_t segments,
                                        std::optional<double> fraction) noexcept
    -> std::optional<std::size_t> {
  if (!fraction.has_value() || std::isnan(*fraction) || segments == 0) {
    return std::nullopt;
  }
  const double scaled =
      std::clamp(*fraction, 0.0, 1.0) * static_cast<double>(segments);
  const double nearest = std::round(scaled);
  const double reached = std::abs(scaled - nearest) <= segment_tolerance
                             ? nearest
                             : std::floor(scaled);
  return static_cast<std::size_t>(reached);
}

auto operator==(const ProgressViewStyle& left, const ProgressViewStyle& right)
    -> bool {
  if (left.kind_ != right.kind_) {
    return false;
  }
  if (left.kind_ == ProgressViewStyle::Kind::segmented) {
    return left.segments_ == right.segments_;
  }
  return left.kind_ != ProgressViewStyle::Kind::custom ||
         left.custom_->equals(*right.custom_);
}

auto ProgressViewStyle::make_body(Configuration configuration) const
    -> composition::Element {
  switch (kind_) {
    case Kind::automatic:
      if (configuration.fraction_completed.has_value()) {
        return lined(std::move(configuration));
      }
      return circled(std::move(configuration));
    case Kind::linear:
      return lined(std::move(configuration));
    case Kind::circular:
      return circled(std::move(configuration));
    case Kind::segmented:
      return segmented(std::move(configuration), segments_);
    case Kind::custom:
      return custom_->make_body(std::move(configuration));
  }
  return circled(std::move(configuration));
}

auto ProgressViewStyle::spinner(double phase, std::optional<double> fraction)
    -> composition::Element {
  const auto count = static_cast<double>(spokes);
  const double head = std::floor(phase * count);
  std::vector<composition::Element> arms;
  arms.reserve(spokes);
  for (std::size_t index = 0; index < spokes; ++index) {
    const auto position = static_cast<double>(index);
    float alpha = 0.0F;
    if (fraction.has_value()) {
      alpha = position < std::round(*fraction * count) ? 1.0F : 0.2F;
    } else {
      const double behind = std::fmod(head - position + count, count);
      alpha = static_cast<float>(1.0 - (behind / count) * 0.85);
    }
    arms.push_back(
        components::ZStack(
            components::ZStackOptions{.alignment = composition::Alignment::top},
            components::ZStack()
                .frame({.width = spinner_extent * 0.09F,
                        .height = spinner_extent * 0.28F})
                .background(theme::ColourRole::text_secondary,
                            theme::Shape::capsule())
                .opacity(alpha))
            .frame({.width = spinner_extent,
                    .height = spinner_extent,
                    .alignment = composition::Alignment::top})
            .rotation_effect(motion::Angle::degrees(360.0 * position / count)));
  }
  return components::ZStack(components::ZStackOptions{}, std::move(arms))
      .frame({.width = spinner_extent, .height = spinner_extent});
}

auto ProgressViewStyle::bar(double start, double share, float thickness)
    -> composition::Element {
  const auto lead =
      static_cast<float>(std::isnan(start) ? 0.0 : std::clamp(start, 0.0, 1.0));
  const auto filled = static_cast<float>(
      std::isnan(share)
          ? 0.0
          : std::clamp(share, 0.0, 1.0 - static_cast<double>(lead)));
  const float rest = std::max(1.0F - lead - filled, 0.0F);
  return components::HStack(components::HStackOptions{.spacing = 0.0F},
                            components::VStack().flex_grow(lead),
                            components::VStack()
                                .flex_grow(filled)
                                .frame({.max_height = composition::infinity})
                                .background(theme::ColourRole::accent,
                                            theme::Shape::capsule()),
                            components::VStack().flex_grow(rest))
      .frame({.height = thickness, .max_width = composition::infinity})
      .background(theme::ColourRole::system_fill, theme::Shape::capsule())
      .clip_shape(theme::Shape::capsule());
}

auto ProgressViewStyle::segments(std::size_t count,
                                 std::optional<double> fraction)
    -> composition::Element {
  const std::optional<std::size_t> current = current_segment(count, fraction);
  std::vector<composition::Element> pieces;
  pieces.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    const bool done = current.has_value() && index < *current;
    const bool active = current.has_value() && index == *current;
    const theme::ColourValue fill =
        done || active
            ? theme::ColourValue{theme::ColourRole::accent}
            : theme::ColourValue{theme::ColourRole::text_secondary}.opacity(
                  pending_segment_opacity);
    composition::Element piece =
        components::ZStack()
            .frame({.width = active ? current_segment_width : segment_width,
                    .height = segment_thickness})
            .background(fill, theme::Shape::capsule());
    if (active) {
      piece = std::move(piece).shadow(
          theme::ColourValue{theme::ColourRole::accent}.opacity(
              segment_glow_opacity),
          segment_glow_radius);
    }
    pieces.push_back(std::move(piece).id(static_cast<std::int64_t>(index)));
  }
  return components::HStack(
             components::HStackOptions{
                 .alignment = components::VerticalAlignment::center,
                 .spacing = theme::Space{segment_spacing}},
             std::move(pieces))
      .animation(motion::Animation::smooth(0.3), current)
      .accessibility_hidden(true);
}

auto ProgressViewStyle::segmented(Configuration configuration,
                                  std::size_t count) -> composition::Element {
  return labelled(segments(count, configuration.fraction_completed),
                  std::move(configuration.label));
}

auto ProgressViewStyle::lined(Configuration configuration)
    -> composition::Element {
  const float thickness =
      components::ControlMetrics::of(configuration.control_size)
          .progress_thickness;
  composition::Element indicator =
      configuration.fraction_completed.has_value()
          ? bar(0.0, *configuration.fraction_completed, thickness)
                .animation(motion::Animation::smooth(0.3),
                           *configuration.fraction_completed)
          : composition::Element{Indeterminate(Indeterminate::Motion::sweep)};
  if (!configuration.fraction_completed.has_value() &&
      thickness != bar_thickness) {
    indicator =
        std::move(indicator)
            .scale_effect(1.0F, thickness / bar_thickness)
            .frame({.height = thickness, .max_width = composition::infinity});
  }
  if (configuration.current_value_label.has_value()) {
    indicator = components::VStack(
        components::VStackOptions{
            .alignment = components::HorizontalAlignment::leading,
            .spacing = theme::space::xs},
        std::move(indicator),
        std::move(*configuration.current_value_label)
            .font(theme::Font::caption)
            .foreground_style(theme::ColourRole::text_secondary));
  }
  composition::Element body =
      components::VStack(
          components::VStackOptions{
              .alignment = components::HorizontalAlignment::leading,
              .spacing = theme::space::xs},
          std::move(indicator))
          .frame({.max_width = composition::infinity,
                  .alignment = composition::Alignment::leading});
  if (!configuration.label.has_value()) {
    return body;
  }
  return components::VStack(
             components::VStackOptions{
                 .alignment = components::HorizontalAlignment::leading,
                 .spacing = theme::space::xs},
             std::move(*configuration.label), std::move(body))
      .frame({.max_width = composition::infinity,
              .alignment = composition::Alignment::leading});
}

auto ProgressViewStyle::circled(Configuration configuration)
    -> composition::Element {
  const std::optional<double> fraction = configuration.fraction_completed;
  composition::Element indicator = sized(
      fraction.has_value()
          ? spinner(0.0, fraction)
          : composition::Element{Indeterminate(Indeterminate::Motion::spin)},
      configuration.control_size);
  return labelled(std::move(indicator), std::move(configuration.label));
}

auto ProgressViewStyle::sized(composition::Element indicator,
                              components::ControlSize size)
    -> composition::Element {
  const float extent = components::ControlMetrics::of(size).spinner_extent;
  if (extent == spinner_extent) {
    return indicator;
  }
  return std::move(indicator)
      .scale_effect(extent / spinner_extent)
      .frame({.width = extent, .height = extent});
}

auto ProgressViewStyle::labelled(composition::Element indicator,
                                 std::optional<composition::Element> label)
    -> composition::Element {
  if (!label.has_value()) {
    return indicator;
  }
  return components::VStack(
      components::VStackOptions{
          .alignment = components::HorizontalAlignment::center,
          .spacing = theme::space::sm},
      std::move(indicator), std::move(*label));
}

ProgressViewStyle::Indeterminate::Indeterminate(Motion motion)
    : Element(std::make_unique<Content>(motion)) {}

auto ProgressViewStyle::Indeterminate::cycle(
    std::chrono::milliseconds period) noexcept -> motion::Animation {
  return motion::Animation::linear(
             std::chrono::duration<double>(period).count())
      .repeat_forever(false)
      .preferred_frame_rate(indeterminate_frame_rate);
}

auto ProgressViewStyle::Indeterminate::Content::type() const noexcept
    -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto ProgressViewStyle::Indeterminate::Content::spin(
    composition::StateHandle<bool> spinning) const -> composition::Element {
  const bool turning = spinning.get();
  return spinner(0.0, std::nullopt)
      .rotation_effect(motion::Angle::degrees(turning ? 360.0 : 0.0))
      .animation(turning ? std::optional<motion::Animation>{cycle(spin_period)}
                         : std::nullopt,
                 turning)
      .on_appear([spinning] { spinning.set(true); });
}

auto ProgressViewStyle::Indeterminate::Content::sweep(
    composition::StateHandle<bool> sweeping,
    composition::StateHandle<float> measured) const -> composition::Element {
  const float track = measured.get();
  const bool moving = sweeping.get();
  const float length = track * sweep_share;
  return components::ZStack(
             components::ZStackOptions{.alignment =
                                           composition::Alignment::leading},
             components::ZStack()
                 .frame({.width = length, .height = bar_thickness})
                 .background(theme::ColourRole::accent, theme::Shape::capsule())
                 .offset(moving ? track : -length, 0.0F)
                 .animation(moving ? std::optional<motion::Animation>{cycle(
                                         sweep_period)}
                                   : std::nullopt,
                            moving))
      .frame({.height = bar_thickness,
              .max_width = composition::infinity,
              .alignment = composition::Alignment::leading})
      .background(theme::ColourRole::system_fill, theme::Shape::capsule())
      .clip_shape(theme::Shape::capsule())
      .on_geometry_change(
          std::type_identity<float>{},
          [](const composition::GeometryProxy& proxy) {
            return proxy.size().width;
          },
          [measured](float value) {
            if (measured.get() != value) {
              measured.set(value);
            }
          })
      .on_change(track > 0.0F, [sweeping] { sweeping.set(true); });
}

auto ProgressViewStyle::Indeterminate::Content::body(
    composition::Context& context) const -> std::vector<composition::Element> {
  const auto active = context.state(running);
  std::vector<composition::Element> produced;
  if (motion_ == Motion::spin) {
    produced.push_back(spin(active));
  } else {
    produced.push_back(sweep(active, context.state(width)));
  }
  return produced;
}

ProgressViewStyle::ProgressViewStyle(DefaultProgressViewStyle) noexcept
    : kind_(Kind::automatic) {}

auto DefaultProgressViewStyle::make_body(
    ProgressViewStyle::Configuration configuration) const
    -> composition::Element {
  return ProgressViewStyle{*this}.make_body(std::move(configuration));
}

ProgressViewStyle::ProgressViewStyle(LinearProgressViewStyle) noexcept
    : kind_(Kind::linear) {}

auto LinearProgressViewStyle::make_body(
    ProgressViewStyle::Configuration configuration) const
    -> composition::Element {
  return ProgressViewStyle{*this}.make_body(std::move(configuration));
}

ProgressViewStyle::ProgressViewStyle(CircularProgressViewStyle) noexcept
    : kind_(Kind::circular) {}

auto CircularProgressViewStyle::make_body(
    ProgressViewStyle::Configuration configuration) const
    -> composition::Element {
  return ProgressViewStyle{*this}.make_body(std::move(configuration));
}

auto SegmentedProgressViewStyle::make_body(
    ProgressViewStyle::Configuration configuration) const
    -> composition::Element {
  return ProgressViewStyle{*this}.make_body(std::move(configuration));
}

}
