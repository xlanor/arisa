#pragma once

#include <arisa/components/control_size.hpp>

#include <arisa/components/style_box.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/environment_key.hpp>
#include <arisa/controls/text_input_border_shape.hpp>

#include <concepts>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

namespace arisa::controls {

class BorderedTextFieldStyle;
class DefaultTextFieldStyle;
class PlainTextFieldStyle;
class RoundedBorderTextFieldStyle;
class SquareBorderTextFieldStyle;

class TextFieldStyle {
 public:
  struct Configuration {
    composition::Element label;
    bool is_empty{true};
    bool is_pressed{false};
    bool is_focused{false};
    bool is_enabled{true};
    components::ControlSize control_size{components::ControlSize::regular};
    TextInputBorderShape border_shape{TextInputBorderShape::automatic()};
  };

  static const composition::EnvironmentKey<TextFieldStyle> key;

  TextFieldStyle(DefaultTextFieldStyle style) noexcept;

  TextFieldStyle(PlainTextFieldStyle style) noexcept;

  TextFieldStyle(RoundedBorderTextFieldStyle style) noexcept;

  TextFieldStyle(BorderedTextFieldStyle style) noexcept;

  TextFieldStyle(SquareBorderTextFieldStyle style) noexcept;

  template <class Style>
    requires(!std::same_as<std::remove_cvref_t<Style>, TextFieldStyle> &&
             !std::same_as<std::remove_cvref_t<Style>, DefaultTextFieldStyle> &&
             !std::same_as<std::remove_cvref_t<Style>, PlainTextFieldStyle> &&
             !std::same_as<std::remove_cvref_t<Style>,
                           RoundedBorderTextFieldStyle> &&
             !std::same_as<std::remove_cvref_t<Style>,
                           BorderedTextFieldStyle> &&
             !std::same_as<std::remove_cvref_t<Style>,
                           SquareBorderTextFieldStyle> &&
             components::StyleOf<Style, Configuration>)
  TextFieldStyle(Style style)
      : kind_(Kind::custom),
        custom_(components::StyleBox<Configuration>::of(std::move(style))) {}

  static auto automatic() noexcept -> TextFieldStyle;

  static auto plain() noexcept -> TextFieldStyle;

  static auto rounded_border() noexcept -> TextFieldStyle;

  static auto bordered() noexcept -> TextFieldStyle;

  static auto square_border() noexcept -> TextFieldStyle;

  auto make_body(Configuration configuration) const -> composition::Element;

  friend auto operator==(const TextFieldStyle& left,
                         const TextFieldStyle& right) -> bool;

 private:
  enum class Kind : std::uint8_t {
    automatic,
    plain,
    rounded_border,
    bordered,
    square_border,
    custom,
  };

  explicit TextFieldStyle(Kind kind) noexcept : kind_(kind) {}

  static auto boxed(Configuration configuration) -> composition::Element;

  static auto bare(Configuration configuration) -> composition::Element;

  Kind kind_;
  std::shared_ptr<const components::StyleBox<Configuration>> custom_;
};

class DefaultTextFieldStyle {
 public:
  auto make_body(TextFieldStyle::Configuration configuration) const
      -> composition::Element;

  friend auto operator==(const DefaultTextFieldStyle&,
                         const DefaultTextFieldStyle&) -> bool = default;
};

class PlainTextFieldStyle {
 public:
  auto make_body(TextFieldStyle::Configuration configuration) const
      -> composition::Element;

  friend auto operator==(const PlainTextFieldStyle&, const PlainTextFieldStyle&)
      -> bool = default;
};

class RoundedBorderTextFieldStyle {
 public:
  auto make_body(TextFieldStyle::Configuration configuration) const
      -> composition::Element;

  friend auto operator==(const RoundedBorderTextFieldStyle&,
                         const RoundedBorderTextFieldStyle&) -> bool = default;
};

class BorderedTextFieldStyle {
 public:
  BorderedTextFieldStyle() = default;

  auto make_body(TextFieldStyle::Configuration configuration) const
      -> composition::Element;

  friend auto operator==(const BorderedTextFieldStyle&,
                         const BorderedTextFieldStyle&) -> bool = default;
};

class SquareBorderTextFieldStyle {
 public:
  SquareBorderTextFieldStyle() = default;

  auto make_body(TextFieldStyle::Configuration configuration) const
      -> composition::Element;

  friend auto operator==(const SquareBorderTextFieldStyle&,
                         const SquareBorderTextFieldStyle&) -> bool = default;
};

}

namespace arisa::composition {

template <class Style>
auto Element::text_field_style(Style style) && -> Element {
  return std::move(*this).environment(
      controls::TextFieldStyle::key,
      controls::TextFieldStyle{std::move(style)});
}

}
