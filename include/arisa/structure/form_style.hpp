#pragma once

#include <arisa/components/panel_radius.hpp>
#include <arisa/components/style_box.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/environment_key.hpp>

#include <concepts>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

namespace arisa::structure {

class FormStyle {
 public:
  struct Configuration {
    composition::Element content;
  };

  static const composition::EnvironmentKey<FormStyle> key;

  static constexpr float columns_margin = 20.0F;
  static constexpr float columns_section_spacing = 20.0F;
  static constexpr float columns_row_spacing = 8.0F;
  static constexpr components::PanelRadius card_radius =
      components::PanelRadius::small;

  template <class Style>
    requires(!std::same_as<std::remove_cvref_t<Style>, FormStyle> &&
             components::StyleOf<Style, Configuration>)
  FormStyle(Style style)
      : kind_(Kind::custom),
        custom_(components::StyleBox<Configuration>::of(std::move(style))) {}

  static auto automatic() noexcept -> FormStyle;

  static auto grouped() noexcept -> FormStyle;

  static auto columns() noexcept -> FormStyle;

  static auto cards() noexcept -> FormStyle;

  auto is_columns() const noexcept -> bool { return kind_ == Kind::columns; }

  auto is_cards() const noexcept -> bool { return kind_ == Kind::cards; }

  auto is_custom() const noexcept -> bool { return kind_ == Kind::custom; }

  auto make_body(Configuration configuration) const -> composition::Element;

  friend auto operator==(const FormStyle& left, const FormStyle& right) -> bool;

 private:
  enum class Kind : std::uint8_t {
    automatic,
    grouped,
    columns,
    cards,
    custom,
  };

  explicit FormStyle(Kind kind) noexcept : kind_(kind) {}

  Kind kind_;
  std::shared_ptr<const components::StyleBox<Configuration>> custom_;
};

}

namespace arisa::composition {

template <class Style>
auto Element::form_style(Style style) && -> Element {
  return std::move(*this).environment(structure::FormStyle::key,
                                      structure::FormStyle{std::move(style)});
}

}
