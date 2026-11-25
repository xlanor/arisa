#pragma once

#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/controls/closed_range.hpp>
#include <arisa/controls/gauge_style.hpp>

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace arisa::controls {

class Gauge : public composition::Element {
 public:
  class Content;

  struct Labels {
    composition::Element label;
    std::optional<composition::Element> current_value{};
    std::optional<composition::Element> minimum_value{};
    std::optional<composition::Element> maximum_value{};
  };

  Gauge(std::string title, double value,
        ClosedRange<double> bounds = {.lower_bound = 0.0, .upper_bound = 1.0});

  Gauge(double value, composition::Element label);

  Gauge(double value, ClosedRange<double> bounds, Labels labels);

  static auto fraction(double value, ClosedRange<double> bounds) noexcept
      -> double;
};

class Gauge::Content final : public composition::ElementContent {
 public:
  using Source = std::shared_ptr<composition::PlaceholderContentView::Source>;

  Content(double fraction, Source label, Source current, Source minimum,
          Source maximum) noexcept
      : fraction_(fraction),
        label_(std::move(label)),
        current_(std::move(current)),
        minimum_(std::move(minimum)),
        maximum_(std::move(maximum)) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

  auto fraction() const noexcept -> double { return fraction_; }

 private:
  static auto shown(const Source& source)
      -> std::optional<composition::Element>;

  double fraction_;
  Source label_;
  Source current_;
  Source minimum_;
  Source maximum_;
};

}
