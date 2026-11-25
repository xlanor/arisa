#pragma once

#include <arisa/components/button_border_shape.hpp>
#include <arisa/components/button_role.hpp>
#include <arisa/components/button_sizing.hpp>
#include <arisa/components/control_size.hpp>
#include <arisa/components/primitive_button_style.hpp>
#include <arisa/components/style_box.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/environment_key.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/shape.hpp>

#include <concepts>
#include <cstdint>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace arisa::components {

class AutomaticButtonStyle;
class BorderedButtonStyle;
class BorderedProminentButtonStyle;
class BorderlessButtonStyle;
class PlainButtonStyle;
class GlassButtonStyle;
class GlassProminentButtonStyle;
class AccessoryBarButtonStyle;
class AccessoryBarActionButtonStyle;
class LinkButtonStyle;
class CardButtonStyle;

class ButtonStyle {
 public:
  struct Configuration {
    composition::Element label;
    bool is_pressed{false};
    bool is_focused{false};
    bool is_enabled{true};
    std::optional<ButtonRole> role;
    ControlSize control_size{ControlSize::regular};
    ButtonBorderShape border_shape{ButtonBorderShape::automatic()};
    ButtonSizing sizing{ButtonSizing::automatic};
    bool shows_shape{false};
  };

  static const composition::EnvironmentKey<ButtonStyle> key;

  ButtonStyle(AutomaticButtonStyle style) noexcept;

  ButtonStyle(BorderedButtonStyle style) noexcept;

  ButtonStyle(BorderedProminentButtonStyle style) noexcept;

  ButtonStyle(BorderlessButtonStyle style) noexcept;

  ButtonStyle(PlainButtonStyle style) noexcept;

  ButtonStyle(GlassButtonStyle style) noexcept;

  ButtonStyle(GlassProminentButtonStyle style) noexcept;

  ButtonStyle(AccessoryBarButtonStyle style) noexcept;

  ButtonStyle(AccessoryBarActionButtonStyle style) noexcept;

  ButtonStyle(LinkButtonStyle style) noexcept;

  ButtonStyle(CardButtonStyle style) noexcept;

  static constexpr float accessory_horizontal_padding = 12.0F;
  static constexpr float accessory_vertical_padding = 4.0F;
  static constexpr float card_radius = 12.0F;
  static constexpr float card_padding = 16.0F;
  static constexpr float card_focus_scale = 1.08F;
  static constexpr float card_pressed_scale = 0.96F;
  static constexpr float pressed_opacity = 0.55F;
  static constexpr float prominent_pressed_opacity = 0.8F;
  static constexpr float disabled_opacity = 0.4F;
  static constexpr theme::ColourRole fill_role = theme::ColourRole::system_fill;
  static constexpr theme::ColourRole pressed_fill_role =
      theme::ColourRole::gray4;

  template <class Style>
    requires(!std::same_as<std::remove_cvref_t<Style>, ButtonStyle> &&
             !std::same_as<std::remove_cvref_t<Style>, AutomaticButtonStyle> &&
             !std::same_as<std::remove_cvref_t<Style>, BorderedButtonStyle> &&
             !std::same_as<std::remove_cvref_t<Style>,
                           BorderedProminentButtonStyle> &&
             !std::same_as<std::remove_cvref_t<Style>, BorderlessButtonStyle> &&
             !std::same_as<std::remove_cvref_t<Style>, PlainButtonStyle> &&
             !std::same_as<std::remove_cvref_t<Style>, GlassButtonStyle> &&
             !std::same_as<std::remove_cvref_t<Style>,
                           GlassProminentButtonStyle> &&
             !std::same_as<std::remove_cvref_t<Style>,
                           AccessoryBarButtonStyle> &&
             !std::same_as<std::remove_cvref_t<Style>,
                           AccessoryBarActionButtonStyle> &&
             !std::same_as<std::remove_cvref_t<Style>, LinkButtonStyle> &&
             !std::same_as<std::remove_cvref_t<Style>, CardButtonStyle> &&
             StyleOf<Style, Configuration>)
  ButtonStyle(Style style)
      : kind_(Kind::custom),
        custom_(StyleBox<Configuration>::of(std::move(style))) {}

  static auto automatic() noexcept -> ButtonStyle;

  static auto bordered() noexcept -> ButtonStyle;

  static auto bordered_prominent() noexcept -> ButtonStyle;

  static auto borderless() noexcept -> ButtonStyle;

  static auto plain() noexcept -> ButtonStyle;

  static auto toolbar() noexcept -> ButtonStyle;

  static auto glass() noexcept -> ButtonStyle;

  static auto glass(theme::Glass glass) noexcept -> ButtonStyle;

  static auto glass_prominent() noexcept -> ButtonStyle;

  static auto accessory_bar() noexcept -> ButtonStyle;

  static auto accessory_bar_action() noexcept -> ButtonStyle;

  static auto link() noexcept -> ButtonStyle;

  static auto card() noexcept -> ButtonStyle;

  auto make_body(Configuration configuration) const -> composition::Element;

  static auto fill(theme::ColourRole role) -> composition::Element;

  static auto fill(theme::ColourRole role, theme::Shape shape)
      -> composition::Element;

  auto is_automatic() const noexcept -> bool {
    return kind_ == Kind::automatic;
  }

  friend auto operator==(const ButtonStyle& left, const ButtonStyle& right)
      -> bool;

 private:
  enum class Kind : std::uint8_t {
    automatic,
    bordered,
    bordered_prominent,
    borderless,
    plain,
    toolbar,
    glass,
    glass_prominent,
    accessory_bar,
    accessory_bar_action,
    link,
    card,
    custom,
  };

  explicit ButtonStyle(Kind kind) noexcept : kind_(kind) {}

  ButtonStyle(Kind kind, theme::Glass glass) noexcept
      : kind_(kind), glass_(glass) {}

  static auto glazed(Configuration configuration, theme::Glass glass,
                     bool prominent) -> composition::Element;

  static auto filled(Configuration configuration, bool prominent)
      -> composition::Element;

  static auto bare(Configuration configuration, bool tinted)
      -> composition::Element;

  static auto tooled(Configuration configuration) -> composition::Element;

  static auto accessory(Configuration configuration, bool action)
      -> composition::Element;

  static auto linked(Configuration configuration) -> composition::Element;

  static auto carded(Configuration configuration) -> composition::Element;

  static auto finish(composition::Element element,
                     const Configuration& configuration)
      -> composition::Element;

  static auto flexed(composition::Element element,
                     const Configuration& configuration)
      -> composition::Element;

  Kind kind_;
  theme::Glass glass_{theme::Glass::regular()};
  std::shared_ptr<const StyleBox<Configuration>> custom_;
};

class AutomaticButtonStyle {
 public:
  auto make_body(ButtonStyle::Configuration configuration) const
      -> composition::Element;

  friend auto operator==(const AutomaticButtonStyle&,
                         const AutomaticButtonStyle&) -> bool = default;
};

class BorderedButtonStyle {
 public:
  auto make_body(ButtonStyle::Configuration configuration) const
      -> composition::Element;

  friend auto operator==(const BorderedButtonStyle&, const BorderedButtonStyle&)
      -> bool = default;
};

class BorderedProminentButtonStyle {
 public:
  auto make_body(ButtonStyle::Configuration configuration) const
      -> composition::Element;

  friend auto operator==(const BorderedProminentButtonStyle&,
                         const BorderedProminentButtonStyle&) -> bool = default;
};

class BorderlessButtonStyle {
 public:
  auto make_body(ButtonStyle::Configuration configuration) const
      -> composition::Element;

  friend auto operator==(const BorderlessButtonStyle&,
                         const BorderlessButtonStyle&) -> bool = default;
};

class GlassButtonStyle {
 public:
  constexpr GlassButtonStyle() noexcept = default;

  constexpr explicit GlassButtonStyle(theme::Glass glass) noexcept
      : glass_(glass) {}

  auto make_body(ButtonStyle::Configuration configuration) const
      -> composition::Element;

  constexpr auto glass() const noexcept -> theme::Glass { return glass_; }

  friend auto operator==(const GlassButtonStyle&, const GlassButtonStyle&)
      -> bool = default;

 private:
  theme::Glass glass_{theme::Glass::regular()};
};

class GlassProminentButtonStyle {
 public:
  auto make_body(ButtonStyle::Configuration configuration) const
      -> composition::Element;

  friend auto operator==(const GlassProminentButtonStyle&,
                         const GlassProminentButtonStyle&) -> bool = default;
};

class AccessoryBarButtonStyle {
 public:
  auto make_body(ButtonStyle::Configuration configuration) const
      -> composition::Element;

  friend auto operator==(const AccessoryBarButtonStyle&,
                         const AccessoryBarButtonStyle&) -> bool = default;
};

class AccessoryBarActionButtonStyle {
 public:
  auto make_body(ButtonStyle::Configuration configuration) const
      -> composition::Element;

  friend auto operator==(const AccessoryBarActionButtonStyle&,
                         const AccessoryBarActionButtonStyle&)
      -> bool = default;
};

class LinkButtonStyle {
 public:
  auto make_body(ButtonStyle::Configuration configuration) const
      -> composition::Element;

  friend auto operator==(const LinkButtonStyle&, const LinkButtonStyle&)
      -> bool = default;
};

class CardButtonStyle {
 public:
  auto make_body(ButtonStyle::Configuration configuration) const
      -> composition::Element;

  friend auto operator==(const CardButtonStyle&, const CardButtonStyle&)
      -> bool = default;
};

class PlainButtonStyle {
 public:
  auto make_body(ButtonStyle::Configuration configuration) const
      -> composition::Element;

  friend auto operator==(const PlainButtonStyle&, const PlainButtonStyle&)
      -> bool = default;
};

}

namespace arisa::composition {

template <class Style>
auto Element::button_style(Style style) && -> Element {
  if constexpr (components::StyleOf<
                    Style, components::PrimitiveButtonStyle::Configuration> &&
                !components::StyleOf<Style,
                                     components::ButtonStyle::Configuration>) {
    return std::move(*this).environment(
        components::PrimitiveButtonStyle::key,
        std::optional<components::PrimitiveButtonStyle>{
            components::PrimitiveButtonStyle{std::move(style)}});
  } else {
    return std::move(*this)
        .environment(components::ButtonStyle::key,
                     components::ButtonStyle{std::move(style)})
        .environment(components::PrimitiveButtonStyle::key, std::nullopt);
  }
}

}
