#pragma once

#include <arisa/composition/binding.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/move_command_direction.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/controls/closed_range.hpp>
#include <arisa/controls/slider_tick.hpp>
#include <arisa/controls/slider_tick_content.hpp>
#include <arisa/theme/metrics.hpp>
#include <arisa/theme/space.hpp>

#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace arisa::controls {

class Slider : public composition::Element {
 public:
  class Content;

  static constexpr float knob_extent = theme::metrics::slider.knob_width;
  static constexpr float knob_height = theme::metrics::slider.knob_height;
  static constexpr float track_thickness =
      theme::metrics::slider.track_thickness;
  static constexpr float height = theme::metrics::slider.height;
  static constexpr double default_increments = 20.0;
  static constexpr theme::Space label_spacing{3.0F};

  using OnEditingChanged = std::function<void(bool)>;
  using TickAt = std::function<std::optional<SliderTick>(double)>;

  static constexpr float tick_extent = 4.0F;
  static constexpr float disabled_track_opacity = 0.35F;

  struct Parts {
    std::optional<double> step{};
    std::optional<double> neutral_value{};
    std::optional<ClosedRange<double>> enabled_bounds{};
    std::optional<composition::Element> label{};
    std::optional<composition::Element> current_value_label{};
    std::optional<composition::Element> minimum_value_label{};
    std::optional<composition::Element> maximum_value_label{};
    SliderTickContent ticks{};
    TickAt tick{};
    OnEditingChanged on_editing_changed{};
  };

  template <composition::BindingSource Source>
    requires std::floating_point<composition::BoundValue<Source>>
  explicit Slider(
      Source value,
      ClosedRange<composition::BoundValue<Source>> bounds = {.lower_bound = 0,
                                                             .upper_bound = 1},
      std::optional<composition::BoundValue<Source>> step = std::nullopt,
      OnEditingChanged on_editing_changed = {})
      : Slider(widened(composition::Binding<composition::BoundValue<Source>>{
                   std::move(value)}),
               widened(bounds), widened(step), std::nullopt,
               shared(std::move(on_editing_changed))) {}

  template <composition::BindingSource Source>
    requires std::floating_point<composition::BoundValue<Source>>
  Slider(Source value, ClosedRange<composition::BoundValue<Source>> bounds,
         composition::Element minimum_value_label,
         composition::Element maximum_value_label,
         OnEditingChanged on_editing_changed = {})
      : Slider(widened(composition::Binding<composition::BoundValue<Source>>{
                   std::move(value)}),
               widened(bounds), std::nullopt,
               Labels{std::move(minimum_value_label),
                      std::move(maximum_value_label)},
               shared(std::move(on_editing_changed))) {}

  template <composition::BindingSource Source>
    requires std::floating_point<composition::BoundValue<Source>>
  Slider(Source value, ClosedRange<composition::BoundValue<Source>> bounds,
         std::optional<composition::BoundValue<Source>> step,
         composition::Element minimum_value_label,
         composition::Element maximum_value_label,
         OnEditingChanged on_editing_changed = {})
      : Slider(widened(composition::Binding<composition::BoundValue<Source>>{
                   std::move(value)}),
               widened(bounds), widened(step),
               Labels{std::move(minimum_value_label),
                      std::move(maximum_value_label)},
               shared(std::move(on_editing_changed))) {}

  template <composition::BindingSource Source>
    requires std::floating_point<composition::BoundValue<Source>>
  Slider(Source value, ClosedRange<composition::BoundValue<Source>> bounds,
         composition::Element label, OnEditingChanged on_editing_changed = {})
      : Slider(std::move(value), bounds,
               Parts{.label = std::move(label),
                     .on_editing_changed = std::move(on_editing_changed)}) {}

  template <composition::BindingSource Source>
    requires std::floating_point<composition::BoundValue<Source>>
  Slider(Source value, ClosedRange<composition::BoundValue<Source>> bounds,
         composition::BoundValue<Source> step, composition::Element label,
         OnEditingChanged on_editing_changed = {})
      : Slider(std::move(value), bounds,
               Parts{.step = widened(step),
                     .label = std::move(label),
                     .on_editing_changed = std::move(on_editing_changed)}) {}

  template <composition::BindingSource Source>
    requires std::floating_point<composition::BoundValue<Source>>
  Slider(Source value, ClosedRange<composition::BoundValue<Source>> bounds,
         Parts parts)
      : Slider(std::move(parts),
               widened(composition::Binding<composition::BoundValue<Source>>{
                   std::move(value)}),
               widened(bounds)) {}

  static auto snapped(double value, ClosedRange<double> bounds,
                      std::optional<double> step) noexcept -> double;

  static auto fraction(double value, ClosedRange<double> bounds) noexcept
      -> double;

  static auto located(float x, float width, ClosedRange<double> bounds,
                      std::optional<double> step,
                      float knob = knob_extent) noexcept -> double;

  static auto nudged(double value, composition::MoveCommandDirection direction,
                     ClosedRange<double> bounds,
                     std::optional<double> step) noexcept -> double;

  static auto detent(double value, ClosedRange<double> bounds,
                     std::optional<double> step) noexcept -> std::int64_t;

 private:
  struct Labels {
    composition::Element minimum;
    composition::Element maximum;
  };

  Slider(composition::Binding<double> value, ClosedRange<double> bounds,
         std::optional<double> step, std::optional<Labels> labels,
         std::shared_ptr<const OnEditingChanged> on_editing_changed);

  Slider(Parts parts, composition::Binding<double> value,
         ClosedRange<double> bounds);

  static auto source(std::optional<composition::Element> element)
      -> std::shared_ptr<composition::PlaceholderContentView::Source>;

  static auto marks(Parts& parts, ClosedRange<double> bounds)
      -> std::vector<SliderTick>;

  void install(composition::Binding<double> value, ClosedRange<double> bounds,
               std::optional<double> step,
               std::shared_ptr<const OnEditingChanged> on_editing_changed);

  static auto shared(OnEditingChanged on_editing_changed)
      -> std::shared_ptr<const OnEditingChanged>;

  template <std::floating_point T>
  static auto widened(T value) noexcept -> double {
    if constexpr (std::same_as<T, double>) {
      return value;
    } else {
      return static_cast<double>(value);
    }
  }

  template <std::floating_point T>
  static auto widened(ClosedRange<T> bounds) noexcept -> ClosedRange<double> {
    return ClosedRange<double>{.lower_bound = widened(bounds.lower_bound),
                               .upper_bound = widened(bounds.upper_bound)};
  }

  template <std::floating_point T>
  static auto widened(std::optional<T> step) noexcept -> std::optional<double> {
    if (!step.has_value()) {
      return std::nullopt;
    }
    return widened(*step);
  }

  template <std::floating_point T>
  static auto widened(composition::Binding<T> value)
      -> composition::Binding<double> {
    if constexpr (std::same_as<T, double>) {
      return value;
    } else {
      return composition::Binding<double>{
          [value] { return static_cast<double>(value.get()); },
          [value](double changed) { value.set(static_cast<T>(changed)); }};
    }
  }
};

class Slider::Content final : public composition::ElementContent {
 public:
  using Source = std::shared_ptr<composition::PlaceholderContentView::Source>;

  static constexpr composition::StateKey<float> width{"slider-width", 0.0F};
  static constexpr composition::StateKey<bool> editing{"slider-editing", false};

  struct Extras {
    std::optional<double> neutral_value{};
    std::optional<ClosedRange<double>> enabled_bounds{};
    Source label{};
    Source current_value_label{};
    std::vector<SliderTick> ticks{};
  };

  Content(composition::Binding<double> value, ClosedRange<double> bounds,
          std::optional<double> step, Source minimum, Source maximum,
          std::shared_ptr<const OnEditingChanged> on_editing_changed) noexcept
      : Content(std::move(value), bounds, step, std::move(minimum),
                std::move(maximum), std::move(on_editing_changed), Extras{}) {}

  Content(composition::Binding<double> value, ClosedRange<double> bounds,
          std::optional<double> step, Source minimum, Source maximum,
          std::shared_ptr<const OnEditingChanged> on_editing_changed,
          Extras extras) noexcept
      : value_(std::move(value)),
        bounds_(bounds),
        step_(step),
        minimum_(std::move(minimum)),
        maximum_(std::move(maximum)),
        on_editing_changed_(std::move(on_editing_changed)),
        extras_(std::move(extras)) {}

  auto ticks() const noexcept -> const std::vector<SliderTick>& {
    return extras_.ticks;
  }

  auto on_editing_changed() const noexcept
      -> const std::shared_ptr<const OnEditingChanged>& {
    return on_editing_changed_;
  }

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto framed() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

  auto accessibility() const -> composition::AccessibilityDefaults override;

  auto mirrored() const noexcept -> const std::shared_ptr<bool>& {
    return mirrored_;
  }

 private:
  auto labelled(composition::Element track) const -> composition::Element;

  auto marked(composition::Element track, float knob_width, float extent,
              bool rtl) const -> composition::Element;

  auto captions(float knob_width, float extent, bool rtl) const
      -> std::optional<composition::Element>;

  auto tick_x(double value, float knob_width, float extent, bool rtl) const
      -> float;

  auto titled(composition::Context& context, composition::Element slider,
              std::optional<float> track_height) const -> composition::Element;

  composition::Binding<double> value_;
  ClosedRange<double> bounds_;
  std::optional<double> step_;
  Source minimum_;
  Source maximum_;
  std::shared_ptr<const OnEditingChanged> on_editing_changed_;
  Extras extras_;
  std::shared_ptr<bool> mirrored_{std::make_shared<bool>(false)};
};

}
