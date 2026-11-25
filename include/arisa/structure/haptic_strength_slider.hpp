#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/provider/haptics/haptic_strength.hpp>

#include <string>
#include <string_view>
#include <utility>

namespace arisa::structure {

class HapticStrengthSlider final : public composition::Component {
 public:
  static constexpr std::string_view title = "Haptic strength";
  static constexpr std::string_view slider_key = "haptic-strength";
  static constexpr double step =
      static_cast<double>(provider::HapticStrength::step_percent) / 100.0;
  HapticStrengthSlider() = default;

  explicit HapticStrengthSlider(std::string label) : label_(std::move(label)) {}

  auto body(composition::Context& context) const
      -> composition::Element override;

  static auto percentage(provider::HapticStrength strength) -> std::string;

  friend auto operator==(const HapticStrengthSlider&,
                         const HapticStrengthSlider&) -> bool = default;

 private:
  std::string label_{title};
};

}
