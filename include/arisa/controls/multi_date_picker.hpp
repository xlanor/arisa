#pragma once

#include <arisa/composition/binding.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/controls/closed_range.hpp>

#include <chrono>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace arisa::controls {

class MultiDatePicker : public composition::Element {
 public:
  class Content;

  using Day = std::chrono::year_month_day;
  using Days = std::set<Day>;
  using Range = ClosedRange<Day>;

  MultiDatePicker(std::string title, composition::Binding<Days> selection);

  MultiDatePicker(std::string title, composition::Binding<Days> selection,
                  Range range);

  MultiDatePicker(composition::Binding<Days> selection,
                  std::optional<Range> range, composition::Element label);
};

class MultiDatePicker::Content final : public composition::ElementContent {
 public:
  using Source = std::shared_ptr<composition::PlaceholderContentView::Source>;

  static constexpr composition::StateKey<std::optional<std::chrono::year_month>>
      page{"multi-date-picker-page", std::nullopt};

  Content(Source label, composition::Binding<Days> selection,
          std::optional<Range> range) noexcept
      : label_(std::move(label)),
        selection_(std::move(selection)),
        range_(range) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

 private:
  Source label_;
  composition::Binding<Days> selection_;
  std::optional<Range> range_;
};

}
