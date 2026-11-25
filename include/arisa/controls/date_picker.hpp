#pragma once

#include <arisa/composition/binding.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/element_content.hpp>
#include <arisa/composition/element_type.hpp>
#include <arisa/composition/placeholder_content_view.hpp>
#include <arisa/controls/date_picker_components.hpp>
#include <arisa/controls/date_picker_style.hpp>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace arisa::controls {

class DatePicker : public composition::Element {
 public:
  class Content;

  using Instant = std::chrono::sys_seconds;
  using Range = DatePickerStyle::Range;

  DatePicker(
      std::string title, composition::Binding<Instant> selection,
      DatePickerComponents components = DatePickerComponents::date |
                                        DatePickerComponents::hour_and_minute);

  DatePicker(
      std::string title, composition::Binding<Instant> selection, Range range,
      DatePickerComponents components = DatePickerComponents::date |
                                        DatePickerComponents::hour_and_minute);

  DatePicker(composition::Binding<Instant> selection,
             std::optional<Range> range, DatePickerComponents components,
             composition::Element label);
};

class DatePicker::Content final : public composition::ElementContent {
 public:
  using Source = std::shared_ptr<composition::PlaceholderContentView::Source>;

  Content(Source label, composition::Binding<Instant> selection,
          std::optional<Range> range, DatePickerComponents components) noexcept
      : label_(std::move(label)),
        selection_(std::move(selection)),
        range_(range),
        components_(components) {}

  auto type() const noexcept -> composition::ElementType override;

  auto composes() const noexcept -> bool override { return true; }

  auto body(composition::Context& context) const
      -> std::vector<composition::Element> override;

 private:
  Source label_;
  composition::Binding<Instant> selection_;
  std::optional<Range> range_;
  DatePickerComponents components_;
};

}
