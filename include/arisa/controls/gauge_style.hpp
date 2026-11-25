#pragma once

#include <arisa/components/style_box.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/environment_key.hpp>
#include <arisa/graphics/shape_style.hpp>

#include <concepts>
#include <cstdint>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace arisa::controls {

class GaugeStyle {
 public:
  struct Configuration {
    double value{0.0};
    composition::Element label;
    std::optional<composition::Element> current_value_label;
    std::optional<composition::Element> minimum_value_label;
    std::optional<composition::Element> maximum_value_label;
  };

  static const composition::EnvironmentKey<GaugeStyle> key;

  static constexpr float circular_extent = 58.0F;
  static constexpr float circular_thickness = 6.0F;
  static constexpr float linear_thickness = 4.0F;
  static constexpr float marker_extent = 8.0F;
  static constexpr double circular_start_degrees = 135.0;
  static constexpr double circular_sweep_degrees = 270.0;

  template <class Style>
    requires(!std::same_as<std::remove_cvref_t<Style>, GaugeStyle> &&
             components::StyleOf<Style, Configuration>)
  GaugeStyle(Style style)
      : kind_(Kind::custom),
        custom_(components::StyleBox<Configuration>::of(std::move(style))) {}

  static auto automatic() noexcept -> GaugeStyle;

  static auto linear_capacity() noexcept -> GaugeStyle;

  static auto linear_capacity(graphics::ShapeStyle tint) -> GaugeStyle;

  static auto accessory_linear() noexcept -> GaugeStyle;

  static auto accessory_linear_capacity() noexcept -> GaugeStyle;

  static auto accessory_circular() noexcept -> GaugeStyle;

  static auto accessory_circular(graphics::ShapeStyle tint) -> GaugeStyle;

  static auto accessory_circular_capacity() noexcept -> GaugeStyle;

  auto make_body(Configuration configuration) const -> composition::Element;

  friend auto operator==(const GaugeStyle& left, const GaugeStyle& right)
      -> bool;

 private:
  enum class Kind : std::uint8_t {
    automatic,
    linear_capacity,
    accessory_linear,
    accessory_linear_capacity,
    accessory_circular,
    accessory_circular_capacity,
    custom,
  };

  explicit GaugeStyle(Kind kind) noexcept : kind_(kind) {}

  GaugeStyle(Kind kind, graphics::ShapeStyle tint)
      : kind_(kind), tint_(std::move(tint)) {}

  static auto capacity(Configuration configuration, float thickness,
                       const std::optional<graphics::ShapeStyle>& tint =
                           std::nullopt) -> composition::Element;

  static auto marked(Configuration configuration) -> composition::Element;

  static auto circular(Configuration configuration, bool capacity,
                       const std::optional<graphics::ShapeStyle>& tint)
      -> composition::Element;

  static auto ends(Configuration& configuration)
      -> std::optional<composition::Element>;

  Kind kind_;
  std::optional<graphics::ShapeStyle> tint_;
  std::shared_ptr<const components::StyleBox<Configuration>> custom_;
};

}

namespace arisa::composition {

template <class Style>
auto Element::gauge_style(Style style) && -> Element {
  return std::move(*this).environment(controls::GaugeStyle::key,
                                      controls::GaugeStyle{std::move(style)});
}

}
