#pragma once

#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/controls/closed_range.hpp>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace arisa::controls {

class ProgressView : public composition::Element {
 public:
  class Content;

  using Date = std::chrono::system_clock::time_point;
  using TimerInterval = ClosedRange<Date>;

  static constexpr std::chrono::milliseconds timer_tick{50};

  ProgressView();

  explicit ProgressView(std::string title);

  explicit ProgressView(composition::Element label);

  explicit ProgressView(double value, double total = 1.0);

  ProgressView(std::string title, double value, double total = 1.0);

  ProgressView(double value, double total, composition::Element label);

  ProgressView(double value, double total, composition::Element label,
               composition::Element current_value_label);

  explicit ProgressView(TimerInterval interval, bool counts_down = true);

  ProgressView(TimerInterval interval, bool counts_down,
               composition::Element label);

  ProgressView(TimerInterval interval, bool counts_down,
               composition::Element label,
               composition::Element current_value_label);

  static auto fraction(double value, double total) noexcept
      -> std::optional<double>;

  static auto timed(TimerInterval interval, bool counts_down, Date now) noexcept
      -> double;

  static auto remaining(TimerInterval interval, bool counts_down, Date now)
      -> std::string;

 private:
  using Source = std::shared_ptr<composition::PlaceholderContentView::Source>;

  ProgressView(std::optional<std::string> title,
               std::optional<double> fraction_completed);

  ProgressView(std::optional<double> fraction_completed,
               std::optional<composition::Element> label,
               std::optional<composition::Element> current_value_label,
               std::optional<TimerInterval> interval, bool counts_down);

  ProgressView(std::optional<double> fraction_completed, Source label,
               Source current_value_label);

  static auto source(std::optional<composition::Element> element) -> Source;
};

class ProgressView::Content final : public composition::ElementContent {
 public:
  Content(std::optional<std::string> title,
          std::optional<double> fraction_completed, Source label,
          Source current_value_label, std::optional<TimerInterval> interval,
          bool counts_down) noexcept
      : title_(std::move(title)),
        fraction_completed_(fraction_completed),
        label_(std::move(label)),
        current_value_label_(std::move(current_value_label)),
        interval_(interval),
        counts_down_(counts_down) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

  auto fraction_completed() const noexcept -> std::optional<double> {
    return fraction_completed_;
  }

  auto interval() const noexcept -> const std::optional<TimerInterval>& {
    return interval_;
  }

 private:
  auto styled(composition::Context& context, std::optional<double> fraction,
              std::optional<composition::Element> fallback_value) const
      -> composition::Element;

  std::optional<std::string> title_;
  std::optional<double> fraction_completed_;
  Source label_;
  Source current_value_label_;
  std::optional<TimerInterval> interval_;
  bool counts_down_{true};
};

}
