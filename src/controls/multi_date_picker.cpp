#include <arisa/components/environment.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/control_environment.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/controls/calendar_month.hpp>
#include <arisa/controls/date_picker_style.hpp>
#include <arisa/controls/multi_date_picker.hpp>
#include <arisa/text/localized_string_key.hpp>

#include <memory>
#include <utility>

namespace arisa::controls {

MultiDatePicker::MultiDatePicker(std::string title,
                                 composition::Binding<Days> selection)
    : MultiDatePicker(
          std::move(selection), std::nullopt,
          components::Text(text::LocalizedStringKey{std::move(title)})) {}

MultiDatePicker::MultiDatePicker(std::string title,
                                 composition::Binding<Days> selection,
                                 Range range)
    : MultiDatePicker(
          std::move(selection), range,
          components::Text(text::LocalizedStringKey{std::move(title)})) {}

MultiDatePicker::MultiDatePicker(composition::Binding<Days> selection,
                                 std::optional<Range> range,
                                 composition::Element label)
    : Element(std::make_unique<Content>(
          std::make_shared<composition::PlaceholderContentView::Source>(
              std::move(label)),
          std::move(selection), range)) {}

auto MultiDatePicker::Content::type() const noexcept
    -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto MultiDatePicker::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  const auto paged = context.state(page);
  const composition::Binding<Days> selection = selection_;
  const Days chosen = selection.get();
  const components::TimeZone zone =
      context.environment(components::Environment::time_zone);
  const std::chrono::year_month_day anchor =
      !chosen.empty()
          ? *chosen.begin()
          : (range_.has_value() ? range_->lower_bound
                                : DatePickerStyle::day_of(
                                      std::chrono::floor<std::chrono::seconds>(
                                          std::chrono::system_clock::now()),
                                      zone));
  const std::chrono::year_month shown =
      paged.get().value_or(anchor.year() / anchor.month());
  composition::Element grid = DatePickerStyle::month(DatePickerStyle::Month{
      .shown =
          CalendarMonth{shown,
                        context.environment(components::Environment::calendar)
                            .first_weekday()},
      .is_selected = [chosen](Day day) { return chosen.contains(day); },
      .choose =
          [selection](Day day) {
            selection.update([day](Days& held) {
              if (held.erase(day) == 0) {
                held.insert(day);
              }
            });
          },
      .turn = [paged](CalendarMonth next) { paged.set(next.month()); },
      .lower = range_.has_value() ? std::optional<Day>{range_->lower_bound}
                                  : std::nullopt,
      .upper = range_.has_value() ? std::optional<Day>{range_->upper_bound}
                                  : std::nullopt,
      .locale = context.environment(components::Environment::locale)});
  std::vector<composition::Element> produced;
  if (context.environment(composition::ControlEnvironment::labels_visibility) ==
      composition::Visibility::hidden) {
    produced.push_back(std::move(grid));
    return produced;
  }
  produced.push_back(
      components::VStack(
          components::VStackOptions{
              .alignment = components::HorizontalAlignment::leading,
              .spacing = theme::space::sm},
          composition::Element{composition::PlaceholderContentView(label_)},
          std::move(grid))
          .frame({.max_width = composition::infinity,
                  .alignment = composition::Alignment::leading}));
  return produced;
}

}
