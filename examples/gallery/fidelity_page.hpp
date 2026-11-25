#pragma once

#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/theme/color_scheme.hpp>

#include <cstdint>
#include <string>
#include <string_view>

namespace arisa::examples {

class FidelityPage final : public composition::Component {
 public:
  enum class Choice : std::uint8_t {
    one,
    two,
    three,
  };

  struct Reference {
    std::string_view name;
    float width{0.0F};
    float height{0.0F};
  };

  static constexpr std::string_view directory = "arisa/swiftui_reference/";
  static constexpr std::string_view sample = "The quick brown fox";
  static constexpr float control_width = 200.0F;
  static constexpr float list_width = 360.0F;
  static constexpr composition::StateKey<bool> switch_on{"fidelity-switch",
                                                         true};
  static constexpr composition::StateKey<bool> switch_off{"fidelity-switch-off",
                                                          false};
  static constexpr composition::StateKey<bool> box_on{"fidelity-box", true};
  static constexpr composition::StateKey<double> level{"fidelity-level", 0.5};
  static constexpr composition::StateKey<int> count{"fidelity-count", 3};
  static constexpr composition::StateKey<Choice> choice{"fidelity-choice",
                                                        Choice::two};
  static const composition::StateKey<std::string> typed;

  auto operator==(const FidelityPage&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

  static auto key(std::string_view element) -> std::string;

 private:
  static auto section(std::string_view title, composition::Element content)
      -> composition::Element;

  static auto pair(std::string_view element, composition::Element arisa,
                   Reference reference, theme::ColorScheme scheme)
      -> composition::Element;

  static auto picture(Reference reference, theme::ColorScheme scheme)
      -> composition::Element;

  static auto choices() -> composition::Element;

  static auto text_styles(composition::Context& context,
                          theme::ColorScheme scheme) -> composition::Element;

  static auto stacks(composition::Context& context, theme::ColorScheme scheme)
      -> composition::Element;

  static auto buttons(composition::Context& context, theme::ColorScheme scheme)
      -> composition::Element;

  static auto toggles(composition::Context& context, theme::ColorScheme scheme)
      -> composition::Element;

  static auto controls(composition::Context& context, theme::ColorScheme scheme)
      -> composition::Element;

  static auto progress(composition::Context& context, theme::ColorScheme scheme)
      -> composition::Element;

  static auto lists(theme::ColorScheme scheme) -> composition::Element;

  static auto alert(composition::Context& context, theme::ColorScheme scheme)
      -> composition::Element;
};

}
