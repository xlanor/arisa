#include <arisa/appearance/appearance_model.hpp>
#include <arisa/components/environment.hpp>
#include <arisa/components/group.hpp>
#include <arisa/components/labeled_content.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/controls/closed_range.hpp>
#include <arisa/controls/slider.hpp>
#include <arisa/structure/haptic_strength_slider.hpp>
#include <arisa/theme/space.hpp>

#include <format>
#include <optional>
#include <vector>

namespace arisa::structure {

auto HapticStrengthSlider::percentage(provider::HapticStrength strength)
    -> std::string {
  return std::format("{}%", strength.percent());
}

auto HapticStrengthSlider::body(composition::Context& context) const
    -> composition::Element {
  const std::optional<appearance::AppearanceModel>& found =
      context.environment(components::Environment::appearance);
  if (!found.has_value()) {
    return components::Group(std::vector<composition::Element>{});
  }
  const appearance::AppearanceModel model = *found;
  const appearance::AppearanceSettings& settings =
      context.read(model.settings());
  const provider::HapticStrength stored = settings.haptic_strength;
  const composition::Binding<double> strength{
      [model] {
        return static_cast<double>(
            model.settings().get().haptic_strength.value());
      },
      [model](double value) {
        const provider::HapticStrength chosen{static_cast<float>(value)};
        if (chosen == model.settings().get().haptic_strength) {
          return;
        }
        model.update([chosen](appearance::AppearanceOverrides& overrides) {
          overrides.haptic_strength = chosen;
        });
      }};
  const bool enabled =
      settings.sensory_feedback &&
      !model.is_locked(appearance::AppearanceField::haptic_strength);
  return composition::Element{
      components::VStack(
          components::VStackOptions{
              .alignment = components::HorizontalAlignment::leading,
              .spacing = theme::space::xs},
          components::LabeledContent(label_, percentage(stored)),
          composition::Element{
              controls::Slider(
                  strength,
                  controls::ClosedRange<double>{
                      .lower_bound = provider::HapticStrength::minimum,
                      .upper_bound = provider::HapticStrength::maximum},
                  step)}
              .frame({.max_width = composition::infinity})
              .id(std::string{slider_key}))}
      .frame({.max_width = composition::infinity})
      .disabled(!enabled);
}

}
