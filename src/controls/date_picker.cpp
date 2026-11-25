#include <arisa/components/environment.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/labeled_content.hpp>
#include <arisa/components/text.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/control_environment.hpp>
#include <arisa/composition/enablement.hpp>
#include <arisa/controls/date_picker.hpp>
#include <arisa/text/localized_string_key.hpp>

#include <memory>
#include <utility>

namespace arisa::controls {

DatePicker::DatePicker(std::string title,
                       composition::Binding<Instant> selection,
                       DatePickerComponents components)
    : DatePicker(std::move(selection), std::nullopt, components,
                 components::Text(text::LocalizedStringKey{std::move(title)})) {
}

DatePicker::DatePicker(std::string title,
                       composition::Binding<Instant> selection, Range range,
                       DatePickerComponents components)
    : DatePicker(std::move(selection), range, components,
                 components::Text(text::LocalizedStringKey{std::move(title)})) {
}

DatePicker::DatePicker(composition::Binding<Instant> selection,
                       std::optional<Range> range,
                       DatePickerComponents components,
                       composition::Element label)
    : Element(std::make_unique<Content>(
          std::make_shared<composition::PlaceholderContentView::Source>(
              std::move(label)),
          std::move(selection), range, components)) {}

auto DatePicker::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto DatePicker::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  const DatePickerStyle& style = context.environment(DatePickerStyle::key);
  const bool hidden =
      context.environment(composition::ControlEnvironment::labels_visibility) ==
      composition::Visibility::hidden;
  const std::optional<components::LabelColumn> column =
      hidden ? std::nullopt : components::LabeledContent::column(context);
  DatePickerStyle::Inputs inputs{
      .selection = selection_,
      .components = components_,
      .range = range_,
      .calendar = context.environment(components::Environment::calendar),
      .time_zone = context.environment(components::Environment::time_zone),
      .locale = context.environment(components::Environment::locale)};
  composition::Element control = style.make_body(DatePickerStyle::Configuration{
      .label = hidden || column.has_value()
                   ? composition::Element{components::Group()}
                   : composition::Element{composition::PlaceholderContentView(
                         label_)},
      .inputs = std::move(inputs)});
  if (!context.environment(composition::Enablement::is_enabled)) {
    control = std::move(control).opacity(0.4F);
  }
  std::vector<composition::Element> produced;
  produced.push_back(column.has_value()
                         ? components::LabeledContent::columned(
                               *column,
                               composition::Element{
                                   composition::PlaceholderContentView(label_)},
                               std::move(control))
                         : std::move(control));
  return produced;
}

}
