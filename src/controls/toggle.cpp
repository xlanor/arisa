#include <arisa/components/environment.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/label.hpp>
#include <arisa/components/labeled_content.hpp>
#include <arisa/components/text.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/control_environment.hpp>
#include <arisa/composition/enablement.hpp>
#include <arisa/controls/toggle.hpp>
#include <arisa/controls/toggle_style.hpp>
#include <arisa/text/localized_string_key.hpp>
#include <arisa/theme/shape.hpp>

#include <algorithm>
#include <memory>
#include <utility>

namespace arisa::controls {

Toggle::Toggle(std::string title, composition::Binding<bool> is_on)
    : Toggle(std::optional<std::string>{std::move(title)}, std::move(is_on),
             std::nullopt) {}

Toggle::Toggle(composition::Binding<bool> is_on, composition::Element label)
    : Toggle(std::nullopt, std::move(is_on),
             std::optional<composition::Element>{std::move(label)}) {}

Toggle::Toggle(std::optional<std::string> title,
               composition::Binding<bool> is_on,
               std::optional<composition::Element> label)
    : Element(std::make_unique<Content>(
          std::move(title), std::move(is_on),
          label.has_value()
              ? std::make_shared<composition::PlaceholderContentView::Source>(
                    std::move(*label))
              : nullptr)) {
  declared().focus_shape = theme::Shape::rounded(theme::radius::control, true);
}

Toggle::Toggle(std::string title, components::Image::SystemName system_image,
               composition::Binding<bool> is_on)
    : Toggle(std::move(is_on),
             components::Label(std::move(title),
                               components::Image{std::move(system_image)})) {}

Toggle::Toggle(std::string title, components::Image::Resource image,
               composition::Binding<bool> is_on)
    : Toggle(std::move(is_on),
             components::Label(std::move(title),
                               components::Image{std::move(image)})) {}

Toggle::Toggle(std::string title,
               std::vector<composition::Binding<bool>> sources)
    : Toggle(std::optional<std::string>{std::move(title)}, std::move(sources),
             std::nullopt) {}

Toggle::Toggle(std::string title, components::Image::SystemName system_image,
               std::vector<composition::Binding<bool>> sources)
    : Toggle(std::move(sources),
             components::Label(std::move(title),
                               components::Image{std::move(system_image)})) {}

Toggle::Toggle(std::string title, components::Image::Resource image,
               std::vector<composition::Binding<bool>> sources)
    : Toggle(std::move(sources),
             components::Label(std::move(title),
                               components::Image{std::move(image)})) {}

Toggle::Toggle(std::vector<composition::Binding<bool>> sources,
               composition::Element label)
    : Toggle(std::nullopt, std::move(sources),
             std::optional<composition::Element>{std::move(label)}) {}

Toggle::Toggle(std::optional<std::string> title,
               std::vector<composition::Binding<bool>> sources,
               std::optional<composition::Element> label)
    : Element(std::make_unique<Content>(
          std::move(title), mixed(sources),
          label.has_value()
              ? std::make_shared<composition::PlaceholderContentView::Source>(
                    std::move(*label))
              : nullptr,
          sources)) {
  declared().focus_shape = theme::Shape::rounded(theme::radius::control, true);
}

auto Toggle::mixed(const std::vector<composition::Binding<bool>>& sources)
    -> composition::Binding<bool> {
  return composition::Binding<bool>{
      [sources] {
        return !sources.empty() &&
               std::ranges::all_of(
                   sources, [](const auto& source) { return source.get(); });
      },
      [sources](bool value) {
        for (const composition::Binding<bool>& source : sources) {
          if (source.get() != value) {
            source.set(value);
          }
        }
      }};
}

auto Toggle::is_mixed(const std::vector<composition::Binding<bool>>& sources)
    -> bool {
  const bool any_on = std::ranges::any_of(
      sources, [](const auto& source) { return source.get(); });
  const bool any_off = std::ranges::any_of(
      sources, [](const auto& source) { return !source.get(); });
  return any_on && any_off;
}

auto Toggle::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto Toggle::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  const ToggleStyle& style = context.environment(ToggleStyle::key);
  const bool hidden =
      context.environment(composition::ControlEnvironment::labels_visibility) ==
      composition::Visibility::hidden;
  composition::Element label =
      hidden ? composition::Element{components::Group()}
      : label_ != nullptr
          ? composition::Element{composition::PlaceholderContentView(label_)}
          : composition::Element{components::Text(
                text::LocalizedStringKey{title_.value_or("")})};
  const std::optional<components::LabelColumn> column =
      hidden ? std::nullopt : components::LabeledContent::column(context);
  const auto control = [&](composition::Element shown) {
    return style
        .make_body(ToggleStyle::Configuration{
            .label = std::move(shown),
            .is_on = is_on_.get(),
            .is_pressed = context.is_pressed(),
            .is_focused = context.is_focused(),
            .is_enabled =
                context.environment(composition::Enablement::is_enabled),
            .control_size =
                context.environment(components::Environment::control_size),
            .is_mixed = is_mixed(sources_),
            .is_on_binding = is_on_})
        .default_sensory_feedback(
            composition::SensoryFeedback::impact(
                composition::SensoryFeedback::Weight::light),
            is_on_.get());
  };
  std::vector<composition::Element> produced;
  if (column.has_value()) {
    produced.push_back(components::LabeledContent::columned(
        *column, std::move(label), control(components::Group())));
  } else {
    produced.push_back(control(std::move(label)));
  }
  return produced;
}

auto Toggle::Content::accessibility() const
    -> composition::AccessibilityDefaults {
  return composition::AccessibilityDefaults{
      .label = title_,
      .value = std::string{is_on_.get() ? "1" : "0"},
      .traits = composition::AccessibilityTraits{
          composition::AccessibilityTrait::is_button,
          composition::AccessibilityTrait::is_toggle}};
}

auto Toggle::Content::activate() -> bool {
  is_on_.set(is_mixed(sources_) || !is_on_.get());
  return true;
}

}
