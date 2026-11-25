#include <arisa/components/group.hpp>
#include <arisa/components/text.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/control_environment.hpp>
#include <arisa/controls/gauge.hpp>
#include <arisa/text/localized_string_key.hpp>

#include <algorithm>
#include <memory>
#include <utility>

namespace arisa::controls {

Gauge::Gauge(std::string title, double value, ClosedRange<double> bounds)
    : Gauge(value, bounds,
            Labels{.label = components::Text(
                       text::LocalizedStringKey{std::move(title)})}) {}

Gauge::Gauge(double value, composition::Element label)
    : Gauge(value, ClosedRange<double>{.lower_bound = 0.0, .upper_bound = 1.0},
            Labels{.label = std::move(label)}) {}

Gauge::Gauge(double value, ClosedRange<double> bounds, Labels labels)
    : Element(std::make_unique<Content>(
          fraction(value, bounds),
          std::make_shared<composition::PlaceholderContentView::Source>(
              std::move(labels.label)),
          labels.current_value.has_value()
              ? std::make_shared<composition::PlaceholderContentView::Source>(
                    std::move(*labels.current_value))
              : nullptr,
          labels.minimum_value.has_value()
              ? std::make_shared<composition::PlaceholderContentView::Source>(
                    std::move(*labels.minimum_value))
              : nullptr,
          labels.maximum_value.has_value()
              ? std::make_shared<composition::PlaceholderContentView::Source>(
                    std::move(*labels.maximum_value))
              : nullptr)) {}

auto Gauge::fraction(double value, ClosedRange<double> bounds) noexcept
    -> double {
  const double span = bounds.upper_bound - bounds.lower_bound;
  if (span <= 0.0) {
    return 0.0;
  }
  return std::clamp((value - bounds.lower_bound) / span, 0.0, 1.0);
}

auto Gauge::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto Gauge::Content::shown(const Source& source)
    -> std::optional<composition::Element> {
  if (source == nullptr) {
    return std::nullopt;
  }
  return composition::Element{composition::PlaceholderContentView(source)};
}

auto Gauge::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  const GaugeStyle& style = context.environment(GaugeStyle::key);
  const bool hidden =
      context.environment(composition::ControlEnvironment::labels_visibility) ==
      composition::Visibility::hidden;
  std::vector<composition::Element> produced;
  produced.push_back(style.make_body(GaugeStyle::Configuration{
      .value = fraction_,
      .label = hidden
                   ? composition::Element{components::Group()}
                   : composition::Element{composition::PlaceholderContentView(
                         label_)},
      .current_value_label = shown(current_),
      .minimum_value_label = shown(minimum_),
      .maximum_value_label = shown(maximum_)}));
  return produced;
}

}
