#include <arisa/components/environment.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/timeline_schedule.hpp>
#include <arisa/components/timeline_view.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/controls/progress_view.hpp>
#include <arisa/controls/progress_view_style.hpp>
#include <arisa/text/localized_string_key.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <memory>
#include <utility>

namespace arisa::controls {

ProgressView::ProgressView() : ProgressView(std::nullopt, std::nullopt) {}

ProgressView::ProgressView(std::string title)
    : ProgressView(std::optional<std::string>{std::move(title)}, std::nullopt) {
}

ProgressView::ProgressView(composition::Element label)
    : ProgressView(std::nullopt,
                   std::optional<composition::Element>{std::move(label)},
                   std::nullopt, std::nullopt, true) {}

ProgressView::ProgressView(double value, double total)
    : ProgressView(std::nullopt, fraction(value, total)) {}

ProgressView::ProgressView(std::string title, double value, double total)
    : ProgressView(std::optional<std::string>{std::move(title)},
                   fraction(value, total)) {}

ProgressView::ProgressView(double value, double total,
                           composition::Element label)
    : ProgressView(fraction(value, total),
                   std::optional<composition::Element>{std::move(label)},
                   std::nullopt, std::nullopt, true) {}

ProgressView::ProgressView(double value, double total,
                           composition::Element label,
                           composition::Element current_value_label)
    : ProgressView(
          fraction(value, total),
          std::optional<composition::Element>{std::move(label)},
          std::optional<composition::Element>{std::move(current_value_label)},
          std::nullopt, true) {}

ProgressView::ProgressView(TimerInterval interval, bool counts_down)
    : ProgressView(std::nullopt, std::nullopt, std::nullopt, interval,
                   counts_down) {}

ProgressView::ProgressView(TimerInterval interval, bool counts_down,
                           composition::Element label)
    : ProgressView(std::nullopt,
                   std::optional<composition::Element>{std::move(label)},
                   std::nullopt, interval, counts_down) {}

ProgressView::ProgressView(TimerInterval interval, bool counts_down,
                           composition::Element label,
                           composition::Element current_value_label)
    : ProgressView(
          std::nullopt, std::optional<composition::Element>{std::move(label)},
          std::optional<composition::Element>{std::move(current_value_label)},
          interval, counts_down) {}

ProgressView::ProgressView(std::optional<std::string> title,
                           std::optional<double> fraction_completed)
    : Element(std::make_unique<Content>(std::move(title), fraction_completed,
                                        nullptr, nullptr, std::nullopt, true)) {
}

ProgressView::ProgressView(
    std::optional<double> fraction_completed,
    std::optional<composition::Element> label,
    std::optional<composition::Element> current_value_label,
    std::optional<TimerInterval> interval, bool counts_down)
    : Element(std::make_unique<Content>(
          std::nullopt, fraction_completed, source(std::move(label)),
          source(std::move(current_value_label)), interval, counts_down)) {}

ProgressView::ProgressView(std::optional<double> fraction_completed,
                           Source label, Source current_value_label)
    : Element(std::make_unique<Content>(
          std::nullopt, fraction_completed, std::move(label),
          std::move(current_value_label), std::nullopt, true)) {}

auto ProgressView::source(std::optional<composition::Element> element)
    -> Source {
  if (!element.has_value()) {
    return nullptr;
  }
  return std::make_shared<composition::PlaceholderContentView::Source>(
      std::move(*element));
}

auto ProgressView::fraction(double value, double total) noexcept
    -> std::optional<double> {
  if (std::isnan(value) || std::isnan(total)) {
    return std::nullopt;
  }
  if (total <= 0.0) {
    return 0.0;
  }
  const double share = value / total;
  if (std::isnan(share)) {
    return std::nullopt;
  }
  return std::clamp(share, 0.0, 1.0);
}

auto ProgressView::timed(TimerInterval interval, bool counts_down,
                         Date now) noexcept -> double {
  const double span =
      std::chrono::duration<double>(interval.upper_bound - interval.lower_bound)
          .count();
  if (span <= 0.0) {
    return counts_down ? 0.0 : 1.0;
  }
  const double elapsed = std::clamp(
      std::chrono::duration<double>(now - interval.lower_bound).count(), 0.0,
      span);
  const double share = elapsed / span;
  return counts_down ? 1.0 - share : share;
}

auto ProgressView::remaining(TimerInterval interval, bool counts_down, Date now)
    -> std::string {
  const Date clamped =
      std::clamp(now, interval.lower_bound, interval.upper_bound);
  const auto shown = std::chrono::duration_cast<std::chrono::seconds>(
      counts_down ? interval.upper_bound - clamped
                  : clamped - interval.lower_bound);
  const auto total = std::max<std::int64_t>(shown.count(), 0);
  return std::format("{}:{:02}", total / 60, total % 60);
}

auto ProgressView::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto ProgressView::Content::styled(
    composition::Context& context, std::optional<double> fraction,
    std::optional<composition::Element> fallback_value) const
    -> composition::Element {
  const ProgressViewStyle& style = context.environment(ProgressViewStyle::key);
  std::optional<composition::Element> label;
  if (label_ != nullptr) {
    label.emplace(composition::PlaceholderContentView(label_));
  } else if (title_.has_value()) {
    label.emplace(components::Text(text::LocalizedStringKey{*title_})
                      .font(theme::Font::callout));
  }
  std::optional<composition::Element> current;
  if (current_value_label_ != nullptr) {
    current.emplace(composition::PlaceholderContentView(current_value_label_));
  } else if (fallback_value.has_value()) {
    current = std::move(fallback_value);
  }
  return style.make_body(ProgressViewStyle::Configuration{
      .fraction_completed = fraction.has_value() && std::isnan(*fraction)
                                ? std::nullopt
                                : fraction,
      .label = std::move(label),
      .current_value_label = std::move(current),
      .control_size =
          context.environment(components::Environment::control_size)});
}

auto ProgressView::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  std::vector<composition::Element> produced;
  if (!interval_.has_value()) {
    produced.push_back(styled(context, fraction_completed_, std::nullopt));
    return produced;
  }
  const TimerInterval interval = *interval_;
  const bool down = counts_down_;
  produced.push_back(components::TimelineView(
      components::TimelineSchedule::custom(
          [end = interval.upper_bound](components::TimelineSchedule::Date after)
              -> std::optional<components::TimelineSchedule::Date> {
            if (after >= end) {
              return std::nullopt;
            }
            return std::min(after + timer_tick, end);
          }),
      [interval, down, label = label_, current = current_value_label_](
          const components::TimelineView::Context& at) {
        return composition::Element{ProgressView{
            timed(interval, down, at.date), label,
            current != nullptr
                ? current
                : source(composition::Element{
                      components::Text(remaining(interval, down, at.date))
                          .font(theme::Font::caption)})}};
      }));
  return produced;
}

}
