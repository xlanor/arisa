#include <arisa/appearance/appearance_model.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/text.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/controls/closed_range.hpp>
#include <arisa/controls/picker.hpp>
#include <arisa/controls/picker_style.hpp>
#include <arisa/controls/stepper.hpp>
#include <arisa/controls/toggle.hpp>
#include <arisa/structure/appearance_settings_section.hpp>
#include <arisa/structure/haptic_strength_slider.hpp>
#include <arisa/structure/section.hpp>
#include <arisa/theme/dynamic_type.hpp>

#include <algorithm>
#include <cmath>
#include <format>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace arisa::structure {

auto AppearanceSettingsSection::size_label(int index) -> std::string_view {
  static constexpr std::array<std::string_view, theme::dynamic_type_size_count>
      labels{"Extra Small", "Small",    "Medium",    "Large",
             "Extra Large", "XX Large", "XXX Large", "AX1",
             "AX2",         "AX3",      "AX4",       "AX5"};
  return labels[static_cast<std::size_t>(
      std::clamp(index, 0, static_cast<int>(labels.size()) - 1))];
}

auto AppearanceSettingsSection::sized(int index) const -> const std::string& {
  return labels_.sizes[static_cast<std::size_t>(
      std::clamp(index, 0, static_cast<int>(labels_.sizes.size()) - 1))];
}

auto AppearanceSettingsSection::filled(const std::string& pattern,
                                       std::string_view value) -> std::string {
  const std::size_t slot = pattern.find("{}");
  if (slot == std::string::npos) {
    return std::format("{} {}", pattern, value);
  }
  std::string text = pattern;
  text.replace(slot, 2, value);
  return text;
}

auto AppearanceSettingsSection::accent_index(
    const appearance::AppearanceSettings& settings) -> std::size_t {
  if (!settings.accent.has_value()) {
    return theme_accent;
  }
  const auto found =
      std::ranges::find(accents, *settings.accent, &Accent::colour);
  return found == accents.end()
             ? theme_accent
             : static_cast<std::size_t>(found - accents.begin());
}

auto AppearanceSettingsSection::body(composition::Context& context) const
    -> composition::Element {
  const std::optional<appearance::AppearanceModel>& found =
      context.environment(components::Environment::appearance);
  if (!found.has_value()) {
    return Section(labels_.header,
                   components::Text(labels_.unavailable)
                       .foreground_style(theme::ColourRole::text_secondary));
  }
  const appearance::AppearanceModel model = *found;
  static_cast<void>(context.read(model.settings()));
  const provider::DisplayMode mode =
      context.environment(components::Environment::display_mode);
  using Overrides = appearance::AppearanceOverrides;
  const auto bound = [&model]<class T>(std::type_identity<T>, auto read,
                                       auto write) {
    return composition::Binding<T>{
        [model, read] { return read(model.settings().get()); },
        [model, write](T value) {
          model.update([&write, &value](Overrides& overrides) {
            write(overrides, value);
          });
        }};
  };
  const auto theme_name = bound(
      std::type_identity<std::string>{},
      [](const appearance::AppearanceSettings& s) { return s.theme; },
      [](Overrides& o, const std::string& v) { o.theme = v; });
  const auto scheme = bound(
      std::type_identity<appearance::ColorSchemePreference>{},
      [](const appearance::AppearanceSettings& s) { return s.color_scheme; },
      [](Overrides& o, appearance::ColorSchemePreference v) {
        o.color_scheme = v;
      });
  const auto accent = bound(
      std::type_identity<std::size_t>{},
      [](const appearance::AppearanceSettings& s) { return accent_index(s); },
      [](Overrides& o, std::size_t index) {
        o.accent = index < accents.size() ? std::optional{accents[index].colour}
                                          : std::nullopt;
      });
  const auto size = bound(
      std::type_identity<int>{},
      [](const appearance::AppearanceSettings& s) {
        return static_cast<int>(s.dynamic_type_size);
      },
      [](Overrides& o, int index) {
        o.dynamic_type_size = static_cast<theme::DynamicTypeSize>(index);
      });
  const auto scale = bound(
      std::type_identity<float>{},
      [mode](const appearance::AppearanceSettings& s) {
        return s.ui_scale.resolve(mode);
      },
      [mode](Overrides& o, float value) {
        o.ui_scale = o.ui_scale.with(mode, value);
      });
  const auto flag = [&bound](bool appearance::AppearanceSettings::* field,
                             std::optional<bool> Overrides::* slot) {
    return bound(
        std::type_identity<bool>{},
        [field](const appearance::AppearanceSettings& s) { return s.*field; },
        [slot](Overrides& o, bool v) { o.*slot = v; });
  };
  const auto motion = flag(&appearance::AppearanceSettings::reduce_motion,
                           &Overrides::reduce_motion);
  const auto transparency =
      flag(&appearance::AppearanceSettings::reduce_transparency,
           &Overrides::reduce_transparency);
  const auto contrast = flag(&appearance::AppearanceSettings::increase_contrast,
                             &Overrides::increase_contrast);
  const auto feedback = flag(&appearance::AppearanceSettings::sensory_feedback,
                             &Overrides::sensory_feedback);
  const auto bold =
      flag(&appearance::AppearanceSettings::bold_text, &Overrides::bold_text);
  const auto shapes = flag(&appearance::AppearanceSettings::button_shapes,
                           &Overrides::button_shapes);
  const auto colourless =
      flag(&appearance::AppearanceSettings::differentiate_without_color,
           &Overrides::differentiate_without_color);
  const auto dimmed = flag(&appearance::AppearanceSettings::dim_flashing_lights,
                           &Overrides::dim_flashing_lights);
  const std::vector<std::string> names = model.theme_names();
  const Labels& words = labels_;
  const auto locked = [&model](appearance::AppearanceField field) {
    return model.is_locked(field);
  };
  using Field = appearance::AppearanceField;
  return Section(
      words.header,
      controls::Picker(words.theme, theme_name,
                       [names, relabelled = words.theme_names] {
                         std::vector<composition::Element> options;
                         for (const std::string& name : names) {
                           const auto shown = relabelled.find(name);
                           options.push_back(
                               components::Text(shown == relabelled.end()
                                                    ? name
                                                    : shown->second)
                                   .tag(std::string{name}));
                         }
                         return components::Group(std::move(options));
                       })
          .disabled(locked(Field::theme)),
      controls::Picker(
          words.appearance, scheme,
          [system = words.system, light = words.light, dark = words.dark] {
            return components::Group(
                components::Text(system).tag(
                    appearance::ColorSchemePreference::system),
                components::Text(light).tag(
                    appearance::ColorSchemePreference::light),
                components::Text(dark).tag(
                    appearance::ColorSchemePreference::dark));
          })
          .picker_style(controls::SegmentedPickerStyle{})
          .disabled(locked(Field::color_scheme)),
      controls::Picker(
          words.accent, accent,
          [own = words.theme_accent, named = words.accents] {
            std::vector<composition::Element> options;
            options.push_back(components::Text(own).tag(theme_accent));
            for (std::size_t index = 0; index < accents.size(); ++index) {
              options.push_back(components::Text(named[index])
                                    .foreground_style(accents[index].colour)
                                    .tag(index));
            }
            return components::Group(std::move(options));
          })
          .disabled(locked(Field::accent)),
      controls::Stepper(
          filled(words.text_size, sized(size.get())), size,
          controls::ClosedRange<int>{.lower_bound = 0,
                                     .upper_bound = static_cast<int>(
                                         theme::dynamic_type_size_count - 1)})
          .disabled(locked(Field::dynamic_type_size)),
      controls::Stepper(
          filled(words.interface_scale,
                 std::format("{}", std::lround(scale.get() * 100.0F))),
          scale,
          controls::ClosedRange<float>{
              .lower_bound = appearance::UiScales::minimum,
              .upper_bound = appearance::UiScales::maximum},
          scale_step)
          .disabled(locked(Field::ui_scale)),
      controls::Toggle(words.reduce_motion, motion)
          .disabled(locked(Field::reduce_motion)),
      controls::Toggle(words.reduce_transparency, transparency)
          .disabled(locked(Field::reduce_transparency)),
      controls::Toggle(words.increase_contrast, contrast)
          .disabled(locked(Field::increase_contrast)),
      controls::Toggle(words.bold_text, bold)
          .disabled(locked(Field::bold_text)),
      controls::Toggle(words.button_shapes, shapes)
          .disabled(locked(Field::button_shapes)),
      controls::Toggle(words.differentiate_without_colour, colourless)
          .disabled(locked(Field::differentiate_without_color)),
      controls::Toggle(words.dim_flashing_lights, dimmed)
          .disabled(locked(Field::dim_flashing_lights)),
      controls::Toggle(words.sensory_feedback, feedback)
          .disabled(locked(Field::sensory_feedback)),
      HapticStrengthSlider{words.haptic_strength});
}

}
