#pragma once

#include <arisa/components/control_size.hpp>
#include <arisa/components/style_box.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/environment_key.hpp>
#include <arisa/theme/metrics.hpp>

#include <concepts>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

namespace arisa::controls {

class DefaultToggleStyle;
class SwitchToggleStyle;
class CheckboxToggleStyle;
class ButtonToggleStyle;

class ToggleStyle {
 public:
  struct Configuration {
    composition::Element label;
    bool is_on{false};
    bool is_pressed{false};
    bool is_focused{false};
    bool is_enabled{true};
    components::ControlSize control_size{components::ControlSize::regular};
    bool is_mixed{false};
    composition::Binding<bool> is_on_binding{
        composition::Binding<bool>::constant(false)};
  };

  static const composition::EnvironmentKey<ToggleStyle> key;

  ToggleStyle(DefaultToggleStyle style) noexcept;

  ToggleStyle(SwitchToggleStyle style) noexcept;

  ToggleStyle(CheckboxToggleStyle style) noexcept;

  ToggleStyle(ButtonToggleStyle style) noexcept;

  static constexpr float track_width = theme::metrics::toggle_switch.width;
  static constexpr float track_height = theme::metrics::toggle_switch.height;
  static constexpr float knob_width = theme::metrics::toggle_switch.knob_width;
  static constexpr float knob_height =
      theme::metrics::toggle_switch.knob_height;
  static constexpr float knob_inset = theme::metrics::toggle_switch.knob_inset;
  static constexpr float box_extent = theme::metrics::checkbox.extent;
  static constexpr float box_radius = theme::metrics::checkbox.corner_radius;
  static constexpr float outline_width = 1.0F;
  static constexpr float knob_shadow_opacity = 0.4F;
  static constexpr float knob_shadow_radius = 1.0F;

  template <class Style>
    requires(!std::same_as<std::remove_cvref_t<Style>, ToggleStyle> &&
             !std::same_as<std::remove_cvref_t<Style>, DefaultToggleStyle> &&
             !std::same_as<std::remove_cvref_t<Style>, SwitchToggleStyle> &&
             !std::same_as<std::remove_cvref_t<Style>, CheckboxToggleStyle> &&
             !std::same_as<std::remove_cvref_t<Style>, ButtonToggleStyle> &&
             components::StyleOf<Style, Configuration>)
  ToggleStyle(Style style)
      : kind_(Kind::custom),
        custom_(components::StyleBox<Configuration>::of(std::move(style))) {}

  static auto automatic() noexcept -> ToggleStyle;

  static auto checkbox() noexcept -> ToggleStyle;

  static auto button() noexcept -> ToggleStyle;

  auto make_body(Configuration configuration) const -> composition::Element;

  friend auto operator==(const ToggleStyle& left, const ToggleStyle& right)
      -> bool;

 private:
  enum class Kind : std::uint8_t {
    automatic,
    switch_toggle,
    checkbox,
    button,
    custom,
  };

  explicit ToggleStyle(Kind kind) noexcept : kind_(kind) {}

  static auto track(const Configuration& configuration) -> composition::Element;

  static auto box(const Configuration& configuration) -> composition::Element;

  static auto switched(Configuration configuration) -> composition::Element;

  static auto checked(Configuration configuration) -> composition::Element;

  static auto pushed(Configuration configuration) -> composition::Element;

  static auto finish(composition::Element element,
                     const Configuration& configuration)
      -> composition::Element;

  Kind kind_;
  std::shared_ptr<const components::StyleBox<Configuration>> custom_;
};

class DefaultToggleStyle {
 public:
  auto make_body(ToggleStyle::Configuration configuration) const
      -> composition::Element;

  friend auto operator==(const DefaultToggleStyle&, const DefaultToggleStyle&)
      -> bool = default;
};

class SwitchToggleStyle {
 public:
  auto make_body(ToggleStyle::Configuration configuration) const
      -> composition::Element;

  friend auto operator==(const SwitchToggleStyle&, const SwitchToggleStyle&)
      -> bool = default;
};

class CheckboxToggleStyle {
 public:
  auto make_body(ToggleStyle::Configuration configuration) const
      -> composition::Element;

  friend auto operator==(const CheckboxToggleStyle&, const CheckboxToggleStyle&)
      -> bool = default;
};

class ButtonToggleStyle {
 public:
  auto make_body(ToggleStyle::Configuration configuration) const
      -> composition::Element;

  friend auto operator==(const ButtonToggleStyle&, const ButtonToggleStyle&)
      -> bool = default;
};

}

namespace arisa::composition {

template <class Style>
auto Element::toggle_style(Style style) && -> Element {
  return std::move(*this).environment(controls::ToggleStyle::key,
                                      controls::ToggleStyle{std::move(style)});
}

}
