#pragma once

#include <arisa/components/button_style.hpp>
#include <arisa/components/style_box.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/environment_key.hpp>

#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <string_view>
#include <type_traits>
#include <utility>

namespace arisa::structure {

class ControlGroupStyle {
 public:
  struct Configuration {
    composition::Element label;
    std::function<composition::Element()> content;
  };

  class SegmentStyle;

  static const composition::EnvironmentKey<ControlGroupStyle> key;

  static constexpr float divider_height = 14.0F;
  static constexpr std::string_view overflow_symbol = "more_horiz";

  template <class Style>
    requires(!std::same_as<std::remove_cvref_t<Style>, ControlGroupStyle> &&
             components::StyleOf<Style, Configuration>)
  ControlGroupStyle(Style style)
      : kind_(Kind::custom),
        custom_(components::StyleBox<Configuration>::of(std::move(style))) {}

  static auto automatic() noexcept -> ControlGroupStyle;

  static auto navigation() noexcept -> ControlGroupStyle;

  static auto palette() noexcept -> ControlGroupStyle;

  static auto menu() noexcept -> ControlGroupStyle;

  static auto compact_menu() noexcept -> ControlGroupStyle;

  auto make_body(Configuration configuration) const -> composition::Element;

  friend auto operator==(const ControlGroupStyle& left,
                         const ControlGroupStyle& right) -> bool;

 private:
  enum class Kind : std::uint8_t {
    automatic,
    navigation,
    palette,
    menu,
    compact_menu,
    custom,
  };

  explicit ControlGroupStyle(Kind kind) noexcept : kind_(kind) {}

  static auto joined(Configuration configuration) -> composition::Element;

  static auto spread(Configuration configuration) -> composition::Element;

  static auto menued(Configuration configuration, bool compact)
      -> composition::Element;

  Kind kind_;
  std::shared_ptr<const components::StyleBox<Configuration>> custom_;
};

class ControlGroupStyle::SegmentStyle {
 public:
  auto make_body(components::ButtonStyle::Configuration configuration) const
      -> composition::Element;

  friend auto operator==(const SegmentStyle&, const SegmentStyle&)
      -> bool = default;
};

}

namespace arisa::composition {

template <class Style>
auto Element::control_group_style(Style style) && -> Element {
  return std::move(*this).environment(
      structure::ControlGroupStyle::key,
      structure::ControlGroupStyle{std::move(style)});
}

}
