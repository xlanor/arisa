#include <arisa/components/control_metrics.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/labeled_content.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/command.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/control_environment.hpp>
#include <arisa/composition/drag_gesture.hpp>
#include <arisa/composition/enablement.hpp>
#include <arisa/composition/gesture.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/controls/slider.hpp>
#include <arisa/controls/toggle_style.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/stroke.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <functional>
#include <memory>
#include <utility>

namespace arisa::controls {

Slider::Slider(composition::Binding<double> value, ClosedRange<double> bounds,
               std::optional<double> step, std::optional<Labels> labels,
               std::shared_ptr<const OnEditingChanged> on_editing_changed)
    : Element(std::make_unique<Content>(
          value, bounds, step,
          labels.has_value()
              ? std::make_shared<composition::PlaceholderContentView::Source>(
                    std::move(labels->minimum))
              : nullptr,
          labels.has_value()
              ? std::make_shared<composition::PlaceholderContentView::Source>(
                    std::move(labels->maximum))
              : nullptr,
          on_editing_changed)) {
  install(std::move(value), bounds, step, std::move(on_editing_changed));
}

Slider::Slider(Parts parts, composition::Binding<double> value,
               ClosedRange<double> bounds)
    : Element(std::make_unique<Content>(
          value, bounds, parts.step,
          source(std::move(parts.minimum_value_label)),
          source(std::move(parts.maximum_value_label)),
          shared(std::move(parts.on_editing_changed)),
          Content::Extras{.neutral_value = parts.neutral_value,
                          .enabled_bounds = parts.enabled_bounds,
                          .label = source(std::move(parts.label)),
                          .current_value_label =
                              source(std::move(parts.current_value_label)),
                          .ticks = marks(parts, bounds)})) {
  const ClosedRange<double> reachable =
      parts.enabled_bounds.has_value()
          ? ClosedRange<double>{.lower_bound =
                                    std::max(bounds.lower_bound,
                                             parts.enabled_bounds->lower_bound),
                                .upper_bound =
                                    std::min(bounds.upper_bound,
                                             parts.enabled_bounds->upper_bound)}
          : bounds;
  install(std::move(value), reachable, parts.step,
          edit<Content>().on_editing_changed());
}

auto Slider::source(std::optional<composition::Element> element)
    -> std::shared_ptr<composition::PlaceholderContentView::Source> {
  if (!element.has_value()) {
    return nullptr;
  }
  return std::make_shared<composition::PlaceholderContentView::Source>(
      std::move(*element));
}

auto Slider::marks(Parts& parts, ClosedRange<double> bounds)
    -> std::vector<SliderTick> {
  std::vector<SliderTick> found = parts.ticks.sorted();
  if (parts.tick && parts.step.has_value() && *parts.step > 0.0) {
    const double span = bounds.upper_bound - bounds.lower_bound;
    const auto count =
        static_cast<std::int64_t>(std::floor(span / *parts.step));
    for (std::int64_t index = 0; index <= count; ++index) {
      const double at =
          bounds.lower_bound + (static_cast<double>(index) * *parts.step);
      if (std::optional<SliderTick> made = parts.tick(at)) {
        found.push_back(std::move(*made));
      }
    }
    std::ranges::stable_sort(found, std::less<>{});
  }
  std::erase_if(found, [bounds](const SliderTick& tick) {
    return tick.value() < bounds.lower_bound ||
           tick.value() > bounds.upper_bound;
  });
  return found;
}

void Slider::install(
    composition::Binding<double> value, ClosedRange<double> bounds,
    std::optional<double> step,
    std::shared_ptr<const OnEditingChanged> on_editing_changed) {
  declared().focusable = true;
  declared().focus_shape = theme::Shape::capsule();
  declared().commands.push_back(
      std::make_shared<composition::Command>(composition::Command::move(
          [value, bounds, step, editing = std::move(on_editing_changed),
           mirrored = edit<Content>().mirrored()](
              composition::MoveCommandDirection direction) {
            if (editing != nullptr) {
              (*editing)(true);
            }
            const bool flipped =
                *mirrored &&
                (direction == composition::MoveCommandDirection::left ||
                 direction == composition::MoveCommandDirection::right);
            const composition::MoveCommandDirection logical =
                !flipped ? direction
                : direction == composition::MoveCommandDirection::left
                    ? composition::MoveCommandDirection::right
                    : composition::MoveCommandDirection::left;
            value.set(nudged(value.get(), logical, bounds, step));
            if (editing != nullptr) {
              (*editing)(false);
            }
          },
          composition::Axis::horizontal)));
}

auto Slider::shared(OnEditingChanged on_editing_changed)
    -> std::shared_ptr<const OnEditingChanged> {
  if (!on_editing_changed) {
    return nullptr;
  }
  return std::make_shared<const OnEditingChanged>(
      std::move(on_editing_changed));
}

auto Slider::snapped(double value, ClosedRange<double> bounds,
                     std::optional<double> step) noexcept -> double {
  double kept = bounds.clamped(value);
  if (step.has_value() && *step > 0.0) {
    kept = bounds.lower_bound +
           (std::round((kept - bounds.lower_bound) / *step) * *step);
  }
  return bounds.clamped(kept);
}

auto Slider::fraction(double value, ClosedRange<double> bounds) noexcept
    -> double {
  const double span = bounds.upper_bound - bounds.lower_bound;
  if (span <= 0.0) {
    return 0.0;
  }
  return std::clamp((value - bounds.lower_bound) / span, 0.0, 1.0);
}

auto Slider::located(float x, float width, ClosedRange<double> bounds,
                     std::optional<double> step, float knob) noexcept
    -> double {
  const float travel = width - knob;
  if (travel <= 0.0F) {
    return bounds.lower_bound;
  }
  const double share =
      std::clamp(static_cast<double>((x - (knob * 0.5F)) / travel), 0.0, 1.0);
  return snapped(
      bounds.lower_bound + (share * (bounds.upper_bound - bounds.lower_bound)),
      bounds, step);
}

auto Slider::nudged(double value, composition::MoveCommandDirection direction,
                    ClosedRange<double> bounds,
                    std::optional<double> step) noexcept -> double {
  const double increment = step.value_or(
      (bounds.upper_bound - bounds.lower_bound) / default_increments);
  const bool raises = direction == composition::MoveCommandDirection::right;
  return snapped(value + (raises ? increment : -increment), bounds, step);
}

auto Slider::detent(double value, ClosedRange<double> bounds,
                    std::optional<double> step) noexcept -> std::int64_t {
  if (step.has_value() && *step > 0.0) {
    return std::llround((bounds.clamped(value) - bounds.lower_bound) / *step);
  }
  return std::llround(fraction(value, bounds) * default_increments);
}

auto Slider::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto Slider::Content::accessibility() const
    -> composition::AccessibilityDefaults {
  const double span = bounds_.upper_bound - bounds_.lower_bound;
  const double fraction =
      span > 0.0 ? (value_.get() - bounds_.lower_bound) / span : 0.0;
  return composition::AccessibilityDefaults{
      .label = std::nullopt,
      .value = std::format("{}%", std::lround(fraction * 100.0)),
      .traits = {},
      .speech = std::nullopt,
      .adjustable = true};
}

auto Slider::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  const auto measured = context.state(width);
  const bool enabled = context.environment(composition::Enablement::is_enabled);
  const bool rtl =
      context.environment(components::Environment::layout_direction) ==
      composition::LayoutDirection::right_to_left;
  *mirrored_ = rtl;
  const bool pressed = context.is_pressed();
  const ClosedRange<double> reachable =
      extras_.enabled_bounds.has_value()
          ? ClosedRange<double>{.lower_bound = std::max(
                                    bounds_.lower_bound,
                                    extras_.enabled_bounds->lower_bound),
                                .upper_bound = std::min(
                                    bounds_.upper_bound,
                                    extras_.enabled_bounds->upper_bound)}
          : bounds_;
  const double current = reachable.clamped(value_.get());
  const auto share = static_cast<float>(fraction(current, bounds_));
  const float anchor =
      extras_.neutral_value.has_value()
          ? static_cast<float>(fraction(*extras_.neutral_value, bounds_))
          : 0.0F;
  const float low = std::min(anchor, share);
  const float high = std::max(anchor, share);
  const components::ControlMetrics metrics = components::ControlMetrics::of(
      context.environment(components::Environment::control_size));
  const float thickness = metrics.slider_track;
  const float knob_width = metrics.slider_knob_width;
  const bool thumbless =
      context.environment(
          composition::ControlEnvironment::slider_thumb_visibility) ==
      composition::Visibility::hidden;
  const auto spacer = [thickness](float grow) {
    return components::VStack().flex_grow(grow).frame({.height = thickness});
  };
  composition::Element track = components::Group();
  if (!extras_.neutral_value.has_value() &&
      !extras_.enabled_bounds.has_value()) {
    track =
        components::HStack(components::HStackOptions{.spacing = 0.0F},
                           spacer(share).background(theme::ColourRole::accent,
                                                    theme::Shape::capsule()),
                           spacer(1.0F - share)
                               .background(theme::ColourRole::system_fill,
                                           theme::Shape::capsule()))
            .padding(theme::Edge::horizontal, knob_width * 0.5F)
            .frame({.max_width = composition::infinity});
  } else {
    composition::Element fill =
        components::HStack(
            components::HStackOptions{.spacing = 0.0F}, spacer(low),
            spacer(high - low)
                .background(theme::ColourRole::accent, theme::Shape::capsule()),
            spacer(1.0F - high))
            .frame({.max_width = composition::infinity});
    const auto start =
        static_cast<float>(fraction(reachable.lower_bound, bounds_));
    const auto stop =
        static_cast<float>(fraction(reachable.upper_bound, bounds_));
    composition::Element groove =
        components::HStack(components::HStackOptions{.spacing = 0.0F},
                           spacer(start),
                           spacer(std::max(stop - start, 0.0F))
                               .background(theme::ColourRole::system_fill,
                                           theme::Shape::capsule()),
                           spacer(std::max(1.0F - stop, 0.0F)))
            .frame({.max_width = composition::infinity})
            .background(
                theme::ColourValue{theme::ColourRole::system_fill}.opacity(
                    extras_.enabled_bounds.has_value() ? disabled_track_opacity
                                                       : 1.0F),
                theme::Shape::capsule());
    track = components::ZStack(std::move(groove), std::move(fill))
                .padding(theme::Edge::horizontal, knob_width * 0.5F)
                .frame({.max_width = composition::infinity});
  }
  if (!extras_.ticks.empty()) {
    track = marked(std::move(track), knob_width, measured.get(), rtl);
  }
  std::vector<composition::Element> layers;
  layers.push_back(std::move(track));
  if (!thumbless) {
    composition::Element knob =
        components::ZStack()
            .frame({.width = knob_width, .height = metrics.slider_knob_height})
            .background(theme::ColourRole::highlight, theme::Shape::capsule())
            .border(theme::ColourRole::separator, theme::Stroke::hairline())
            .shadow(theme::Colour{0.0F, 0.0F, 0.0F,
                                  ToggleStyle::knob_shadow_opacity},
                    ToggleStyle::knob_shadow_radius, 0.0F, 1.0F)
            .scale_effect(pressed ? 1.1F : 1.0F);
    layers.push_back(components::HStack(
                         components::HStackOptions{.spacing = 0.0F},
                         components::VStack().flex_grow(share), std::move(knob),
                         components::VStack().flex_grow(1.0F - share))
                         .frame({.max_width = composition::infinity}));
  }
  const composition::Binding<double> value = value_;
  const std::optional<double> step = step_;
  const auto engaged = context.state(editing);
  const std::shared_ptr<const OnEditingChanged> changed = on_editing_changed_;
  const ClosedRange<double> bounds = bounds_;
  const auto follow = [measured, value, bounds, reachable, step, engaged,
                       changed, knob_width,
                       rtl](const composition::DragGesture::Value& drag) {
    if (!engaged.get()) {
      engaged.set(true);
      if (changed != nullptr) {
        (*changed)(true);
      }
    }
    const float x = rtl ? measured.get() - drag.location.x : drag.location.x;
    value.set(reachable.clamped(
        located(x, measured.get(), bounds, step, knob_width)));
  };
  const auto release = [follow, engaged,
                        changed](const composition::DragGesture::Value& drag) {
    follow(drag);
    engaged.set(false);
    if (changed != nullptr) {
      (*changed)(false);
    }
  };
  composition::Element body =
      components::ZStack(components::ZStackOptions{}, std::move(layers))
          .frame({.max_width = composition::infinity,
                  .min_height = metrics.slider_height})
          .content_shape(theme::Shape::rectangle())
          .on_geometry_change(
              std::type_identity<float>{},
              [](const composition::GeometryProxy& proxy) {
                return proxy.size().width;
              },
              [measured](const float& extent) { measured.set(extent); })
          .gesture(composition::DragGesture(0.0F).on_changed(follow).on_ended(
              release))
          .animation(motion::Animation::interactive_spring(), share)
          .animation(motion::Animation::snappy(0.2), pressed)
          .default_sensory_feedback(composition::SensoryFeedback::selection,
                                    detent(current, bounds_, step_));
  std::optional<composition::Element> named =
      captions(knob_width, measured.get(), rtl);
  const std::optional<float> row =
      named.has_value() ? std::optional<float>{metrics.slider_height}
                        : std::nullopt;
  if (named.has_value()) {
    body = components::VStack(
        components::VStackOptions{.spacing = theme::space::xxs},
        std::move(body), std::move(*named));
  }
  if (minimum_ != nullptr && maximum_ != nullptr) {
    body = labelled(std::move(body));
  }
  if (extras_.current_value_label != nullptr) {
    body = components::HStack(
               components::HStackOptions{
                   .alignment = row.has_value()
                                    ? components::VerticalAlignment::top
                                    : components::VerticalAlignment::center,
                   .spacing = label_spacing},
               std::move(body),
               composition::PlaceholderContentView(extras_.current_value_label)
                   .foreground_style(theme::ColourRole::text_secondary)
                   .frame({.min_height = row}))
               .frame({.max_width = composition::infinity});
  }
  if (extras_.label != nullptr) {
    body = titled(context, std::move(body), row);
  }
  if (!enabled) {
    body = std::move(body).opacity(0.4F);
  }
  std::vector<composition::Element> produced;
  produced.push_back(std::move(body));
  return produced;
}

auto Slider::Content::tick_x(double value, float knob_width, float extent,
                             bool rtl) const -> float {
  const float span = std::max(extent - knob_width, 0.0F);
  const float along = (knob_width * 0.5F) +
                      (static_cast<float>(fraction(value, bounds_)) * span);
  return rtl ? extent - along : along;
}

auto Slider::Content::marked(composition::Element track, float knob_width,
                             float extent, bool rtl) const
    -> composition::Element {
  std::vector<composition::Element> dots;
  for (const SliderTick& tick : extras_.ticks) {
    dots.push_back(components::ZStack()
                       .frame({.width = tick_extent, .height = tick_extent})
                       .background(theme::ColourRole::tertiary_label,
                                   theme::Shape::capsule())
                       .offset(tick_x(tick.value(), knob_width, extent, rtl) -
                               (tick_extent * 0.5F)));
  }
  return std::move(track).overlay(
      components::ZStack(
          components::ZStackOptions{.alignment =
                                        composition::Alignment::leading},
          std::move(dots))
          .frame({.max_width = composition::infinity,
                  .alignment = composition::Alignment::leading})
          .environment(components::Environment::layout_direction,
                       composition::LayoutDirection::left_to_right)
          .allows_hit_testing(false));
}

auto Slider::Content::captions(float knob_width, float extent, bool rtl) const
    -> std::optional<composition::Element> {
  std::vector<composition::Element> named;
  for (const SliderTick& tick : extras_.ticks) {
    std::optional<composition::Element> caption = tick.labelled();
    if (!caption.has_value()) {
      continue;
    }
    const double share = fraction(tick.value(), bounds_);
    const bool starts = rtl ? share >= 1.0 : share <= 0.0;
    const bool ends = rtl ? share <= 0.0 : share >= 1.0;
    composition::Element text =
        std::move(*caption)
            .font(theme::Font::caption2)
            .foreground_style(theme::ColourRole::text_secondary)
            .fixed_size();
    if (starts || ends) {
      named.push_back(
          components::ZStack(
              components::ZStackOptions{
                  .alignment = starts ? composition::Alignment::leading
                                      : composition::Alignment::trailing},
              std::move(text))
              .frame({.max_width = composition::infinity,
                      .alignment = starts ? composition::Alignment::leading
                                          : composition::Alignment::trailing}));
      continue;
    }
    named.push_back(components::ZStack(std::move(text))
                        .frame({.width = 0.0F})
                        .offset(tick_x(tick.value(), knob_width, extent, rtl) -
                                (knob_width * 0.5F)));
  }
  if (named.empty()) {
    return std::nullopt;
  }
  return components::ZStack(
             components::ZStackOptions{.alignment =
                                           composition::Alignment::leading},
             std::move(named))
      .frame({.max_width = composition::infinity,
              .alignment = composition::Alignment::leading})
      .padding(theme::Edge::horizontal, knob_width * 0.5F)
      .environment(components::Environment::layout_direction,
                   composition::LayoutDirection::left_to_right)
      .allows_hit_testing(false);
}

auto Slider::Content::titled(composition::Context& context,
                             composition::Element slider,
                             std::optional<float> track_height) const
    -> composition::Element {
  if (context.environment(composition::ControlEnvironment::labels_visibility) ==
      composition::Visibility::hidden) {
    return slider;
  }
  if (const std::optional<components::LabelColumn> column =
          components::LabeledContent::column(context)) {
    return components::LabeledContent::columned(
        *column, composition::PlaceholderContentView(extras_.label),
        std::move(slider));
  }
  return components::HStack(
             components::HStackOptions{
                 .alignment = track_height.has_value()
                                  ? components::VerticalAlignment::top
                                  : components::VerticalAlignment::center,
                 .spacing = std::nullopt},
             composition::PlaceholderContentView(extras_.label)
                 .frame({.min_height = track_height}),
             std::move(slider))
      .frame({.max_width = composition::infinity});
}

auto Slider::Content::labelled(composition::Element track) const
    -> composition::Element {
  return components::HStack(components::HStackOptions{.spacing = label_spacing},
                            composition::PlaceholderContentView(minimum_),
                            std::move(track),
                            composition::PlaceholderContentView(maximum_))
      .frame({.max_width = composition::infinity});
}

}
