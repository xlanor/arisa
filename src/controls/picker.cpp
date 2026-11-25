#include <arisa/components/environment.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/label.hpp>
#include <arisa/components/labeled_content.hpp>
#include <arisa/components/text.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/control_environment.hpp>
#include <arisa/composition/enablement.hpp>
#include <arisa/controls/picker.hpp>
#include <arisa/text/localized_string_key.hpp>

#include <memory>
#include <utility>

namespace arisa::controls {

Picker::Picker(std::string title, Selection selection, Builder content)
    : Element(std::make_unique<Content>(std::move(title), std::move(selection),
                                        std::move(content))) {}

Picker::Picker(std::string title, Selection selection, Builder content,
               std::optional<composition::Element> label)
    : Element(std::make_unique<Content>(
          std::move(title), std::move(selection), std::move(content),
          label.has_value()
              ? std::make_shared<composition::PlaceholderContentView::Source>(
                    std::move(*label))
              : nullptr)) {}

auto Picker::labelled(std::string title,
                      components::Image::SystemName system_image)
    -> composition::Element {
  return components::Label(std::move(title),
                           components::Image{std::move(system_image)});
}

auto Picker::labelled(std::string title, components::Image::Resource image)
    -> composition::Element {
  return components::Label(std::move(title),
                           components::Image{std::move(image)});
}

auto Picker::options(const Builder& content, const Selection& selection)
    -> std::vector<PickerStyle::Option> {
  std::vector<PickerStyle::Option> found;
  if (!content) {
    return found;
  }
  for (composition::Element& member : components::Group::members(content())) {
    const std::shared_ptr<const composition::TagValue> tag = member.tag();
    if (tag == nullptr) {
      continue;
    }
    const bool selected = selection.is_selected && selection.is_selected(*tag);
    found.push_back(PickerStyle::Option{
        .label = std::move(member),
        .is_selected = selected,
        .select =
            [select = selection.select, tag] {
              if (select) {
                select(*tag);
              }
            },
        .is_mixed = selection.is_mixed && selection.is_mixed(*tag)});
  }
  return found;
}

auto Picker::Content::type() const noexcept -> composition::ElementType {
  return composition::ElementType::of<Content>();
}

auto Picker::Content::title_label() const -> composition::Element {
  if (label_ != nullptr) {
    return composition::PlaceholderContentView(label_);
  }
  return components::Text(text::LocalizedStringKey{title_});
}

auto Picker::Content::counted(const Selection& selection,
                              composition::StateHandle<std::int64_t> changed)
    -> Selection {
  return Selection{
      .is_selected = selection.is_selected,
      .select =
          [is_selected = selection.is_selected, select = selection.select,
           changed](const composition::TagValue& tag) {
            const bool already = is_selected && is_selected(tag);
            if (select) {
              select(tag);
            }
            if (!already) {
              changed.set(changed.get() + 1);
            }
          },
      .is_mixed = selection.is_mixed};
}

auto Picker::Content::body(composition::Context& context) const
    -> std::vector<composition::Element> {
  const PickerStyle& style = context.environment(PickerStyle::key);
  const auto changed = context.state(changes);
  const bool hidden =
      context.environment(composition::ControlEnvironment::labels_visibility) ==
      composition::Visibility::hidden;
  const std::optional<components::LabelColumn> column =
      hidden ? std::nullopt : components::LabeledContent::column(context);
  composition::Element control =
      style
          .make_body(PickerStyle::Configuration{
              .label = hidden || column.has_value()
                           ? composition::Element{components::Group()}
                           : title_label(),
              .title = title_,
              .options =
                  [content = content_,
                   selection = counted(selection_, changed)] {
                    return options(content, selection);
                  },
              .is_enabled =
                  context.environment(composition::Enablement::is_enabled),
              .control_size =
                  context.environment(components::Environment::control_size),
              .palette_effect =
                  context.environment(PaletteSelectionEffect::key),
              .radio_group_layout = context.environment(
                  composition::ControlEnvironment::radio_group_layout)})
          .default_sensory_feedback(composition::SensoryFeedback::selection,
                                    changed.get());
  std::vector<composition::Element> produced;
  produced.push_back(column.has_value()
                         ? components::LabeledContent::columned(
                               *column, title_label(), std::move(control))
                         : std::move(control));
  return produced;
}

}
