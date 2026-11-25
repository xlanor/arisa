#pragma once

#include <arisa/components/calendar.hpp>
#include <arisa/components/style_box.hpp>
#include <arisa/components/time_zone.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/controls/calendar_month.hpp>
#include <arisa/controls/closed_range.hpp>
#include <arisa/controls/date_picker_components.hpp>
#include <arisa/text/locale.hpp>

#include <chrono>
#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace arisa::controls {

class DatePickerStyle {
 public:
  using Range = ClosedRange<std::chrono::sys_seconds>;

  struct Inputs {
    composition::Binding<std::chrono::sys_seconds> selection;
    DatePickerComponents components{DatePickerComponents::date |
                                    DatePickerComponents::hour_and_minute};
    std::optional<Range> range;
    components::Calendar calendar;
    components::TimeZone time_zone;
    text::Locale locale{text::Locale::undetermined()};

    friend auto operator==(const Inputs& left, const Inputs& right) -> bool {
      return left.selection.identity() == right.selection.identity() &&
             left.components == right.components && left.range == right.range &&
             left.calendar == right.calendar &&
             left.time_zone == right.time_zone && left.locale == right.locale;
    }
  };

  struct Configuration {
    composition::Element label;
    Inputs inputs;
  };

  struct Month {
    CalendarMonth shown;
    std::function<bool(std::chrono::year_month_day)> is_selected;
    std::function<void(std::chrono::year_month_day)> choose;
    std::function<void(CalendarMonth)> turn;
    std::optional<std::chrono::year_month_day> lower;
    std::optional<std::chrono::year_month_day> upper;
    text::Locale locale{text::Locale::undetermined()};
  };

  class Compact;
  class Graphical;
  class Field;

  static const composition::EnvironmentKey<DatePickerStyle> key;

  static constexpr float cell_extent = 36.0F;
  static constexpr float cell_spacing = 2.0F;
  static constexpr std::string_view previous_key = "arisa.date_picker.previous";
  static constexpr std::string_view next_key = "arisa.date_picker.next";
  static constexpr std::string_view date_key = "arisa.date_picker.date";
  static constexpr std::string_view time_key = "arisa.date_picker.time";

  template <class Style>
    requires(!std::same_as<std::remove_cvref_t<Style>, DatePickerStyle> &&
             components::StyleOf<Style, Configuration>)
  DatePickerStyle(Style style)
      : kind_(Kind::custom),
        custom_(components::StyleBox<Configuration>::of(std::move(style))) {}

  static auto automatic() noexcept -> DatePickerStyle;

  static auto compact() noexcept -> DatePickerStyle;

  static auto graphical() noexcept -> DatePickerStyle;

  static auto wheel() noexcept -> DatePickerStyle;

  static auto field() noexcept -> DatePickerStyle;

  static auto stepper_field() noexcept -> DatePickerStyle;

  auto make_body(Configuration configuration) const -> composition::Element;

  friend auto operator==(const DatePickerStyle& left,
                         const DatePickerStyle& right) -> bool;

  static auto day_of(std::chrono::sys_seconds instant,
                     components::TimeZone zone) noexcept
      -> std::chrono::year_month_day;

  static auto minutes_of(std::chrono::sys_seconds instant,
                         components::TimeZone zone) noexcept
      -> std::chrono::minutes;

  static auto compose(std::chrono::year_month_day day,
                      std::chrono::minutes since_midnight,
                      components::TimeZone zone) noexcept
      -> std::chrono::sys_seconds;

  static auto clamped(std::chrono::sys_seconds instant,
                      const std::optional<Range>& range) noexcept
      -> std::chrono::sys_seconds;

  static auto month(Month month) -> composition::Element;

  static auto day_key(std::chrono::year_month_day day) -> std::string;

 private:
  enum class Kind : std::uint8_t {
    automatic,
    compact,
    graphical,
    wheel,
    field,
    stepper_field,
    custom,
  };

  explicit DatePickerStyle(Kind kind) noexcept : kind_(kind) {}

  static auto wheeled(Configuration configuration) -> composition::Element;

  static auto labelled(composition::Element label, composition::Element control,
                       bool stacked) -> composition::Element;

  Kind kind_;
  std::shared_ptr<const components::StyleBox<Configuration>> custom_;
};

class DatePickerStyle::Graphical final : public composition::Component {
 public:
  static constexpr composition::StateKey<std::optional<std::chrono::year_month>>
      page{"date-picker-page", std::nullopt};

  explicit Graphical(Inputs inputs) noexcept : inputs_(std::move(inputs)) {}

  auto operator==(const Graphical&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

  static auto time(const Inputs& inputs) -> composition::Element;

 private:
  Inputs inputs_;
};

class DatePickerStyle::Field final : public composition::Component {
 public:
  static constexpr composition::StateKey<bool> presented{
      "date-picker-field-presented", false};
  static constexpr std::string_view field_key = "arisa.date_picker.field";

  Field(Inputs inputs, bool steps) noexcept
      : inputs_(std::move(inputs)), steps_(steps) {}

  auto operator==(const Field&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

  static auto stepped(std::chrono::sys_seconds instant,
                      DatePickerComponents components, int direction) noexcept
      -> std::chrono::sys_seconds;

 private:
  Inputs inputs_;
  bool steps_{false};
};

class DatePickerStyle::Compact final : public composition::Component {
 public:
  static constexpr composition::StateKey<bool> date_shown{
      "date-picker-date-shown", false};
  static constexpr composition::StateKey<bool> time_shown{
      "date-picker-time-shown", false};

  explicit Compact(Inputs inputs) noexcept : inputs_(std::move(inputs)) {}

  auto operator==(const Compact&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  Inputs inputs_;
};

}

namespace arisa::composition {

template <class Style>
auto Element::date_picker_style(Style style) && -> Element {
  return std::move(*this).environment(
      controls::DatePickerStyle::key,
      controls::DatePickerStyle{std::move(style)});
}

}
