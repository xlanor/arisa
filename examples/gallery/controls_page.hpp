#pragma once

#include <arisa/components/button_style.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/composition/state_handle.hpp>
#include <arisa/composition/state_key.hpp>
#include <gallery_profile.hpp>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>

namespace arisa::examples {

class ControlsPage final : public composition::Component {
 public:
  enum class Flavour : std::uint8_t {
    vanilla,
    chocolate,
    strawberry,
  };

  enum class Size : std::uint8_t {
    small,
    medium,
    large,
  };

  enum class Mood : std::uint8_t {
    star,
    favorite,
    bolt,
    rocket,
  };

  struct CapsuleStyle {
    auto make_body(components::ButtonStyle::Configuration configuration) const
        -> composition::Element;

    friend auto operator==(const CapsuleStyle&, const CapsuleStyle&)
        -> bool = default;
  };

  static constexpr float card_width = 420.0F;
  static constexpr composition::StateKey<int> presses{"controls-presses", 0};
  static constexpr composition::StateKey<bool> wifi{"controls-wifi", true};
  static constexpr composition::StateKey<bool> bluetooth{"controls-bluetooth",
                                                         false};
  static constexpr composition::StateKey<bool> sync{"controls-sync", true};
  static constexpr composition::StateKey<bool> locked{"controls-locked", false};
  static constexpr composition::StateKey<double> volume{"controls-volume", 0.4};
  static constexpr composition::StateKey<double> brightness{
      "controls-brightness", 0.7};
  static constexpr composition::StateKey<int> quantity{"controls-quantity", 2};
  static constexpr composition::StateKey<Flavour> flavour{"controls-flavour",
                                                          Flavour::chocolate};
  static constexpr composition::StateKey<Size> size{"controls-size",
                                                    Size::medium};
  static constexpr composition::StateKey<Flavour> scoop{"controls-scoop",
                                                        Flavour::vanilla};
  static constexpr composition::StateKey<Size> cup{"controls-cup", Size::large};
  static constexpr composition::StateKey<Flavour> wheel{"controls-wheel",
                                                        Flavour::chocolate};
  static constexpr composition::StateKey<Mood> mood{"controls-mood",
                                                    Mood::favorite};
  static constexpr composition::StateKey<bool> tinted{"controls-tinted", true};
  static constexpr std::string_view disabled_capsule = "controls-capsule-off";
  static constexpr std::string_view palette_picker = "controls-palette";
  static constexpr std::string_view wheel_picker = "controls-wheel";
  static constexpr std::string_view tint_demo = "controls-tint";
  static constexpr composition::StateKey<std::string_view> chosen{
      "controls-chosen", "nothing yet"};
  static constexpr composition::StateKey<double> progress{"controls-progress",
                                                          0.65};
  static const composition::StateKey<std::string> name;
  static const composition::StateKey<std::string> secret;
  static const composition::StateKey<
      std::shared_ptr<composition::Observable<GalleryProfile>>>
      profile;
  static constexpr std::string_view first_button = "controls-first";
  static constexpr std::string_view style_demos = "controls.button_styles";
  static constexpr std::string_view role_demos = "controls.button_roles";

  auto operator==(const ControlsPage&) const -> bool = default;

  auto body(composition::Context& context) const
      -> composition::Element override;

 private:
  static auto card(std::string_view title, composition::Element content)
      -> composition::Element;

  static auto flavours() -> composition::Element;

  static auto sizes() -> composition::Element;

  static auto moods() -> composition::Element;

  static auto flavour_name(Flavour value) -> std::string_view;

  static auto button_styles(composition::Context& context)
      -> composition::Element;

  static auto button_roles(composition::Context& context)
      -> composition::Element;

  static auto toggles(composition::Context& context) -> composition::Element;

  static auto sliders(composition::Context& context) -> composition::Element;

  static auto pickers(composition::Context& context) -> composition::Element;

  static auto tint_card(composition::Context& context) -> composition::Element;

  static auto inline_pickers(composition::Context& context)
      -> composition::Element;

  static auto menu_card(composition::Context& context) -> composition::Element;

  static auto progress_card(composition::Context& context)
      -> composition::Element;

  static auto labels_card() -> composition::Element;

  static auto text_fields(composition::Context& context)
      -> composition::Element;

  static auto wheel_card(composition::Context& context) -> composition::Element;
};

}
