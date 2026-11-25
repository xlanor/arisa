#pragma once

#include <arisa/components/style_box.hpp>
#include <arisa/composition/axes.hpp>
#include <arisa/composition/axis.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/size.hpp>

#include <concepts>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

namespace arisa::components {

class AutomaticArrangementViewStyle;
class SplitArrangementViewStyle;
class OverlayArrangementViewStyle;

class ArrangementViewStyle {
 public:
  enum class Kind : std::uint8_t {
    automatic,
    split,
    overlay,
    custom,
  };

  struct Configuration {
    composition::Element primary;
    composition::Element secondary;
  };

  static const composition::EnvironmentKey<ArrangementViewStyle> key;

  ArrangementViewStyle(AutomaticArrangementViewStyle style) noexcept;

  ArrangementViewStyle(SplitArrangementViewStyle style) noexcept;

  ArrangementViewStyle(OverlayArrangementViewStyle style) noexcept;

  template <class Style>
    requires(!std::same_as<std::remove_cvref_t<Style>, ArrangementViewStyle> &&
             !std::same_as<std::remove_cvref_t<Style>,
                           AutomaticArrangementViewStyle> &&
             !std::same_as<std::remove_cvref_t<Style>,
                           SplitArrangementViewStyle> &&
             !std::same_as<std::remove_cvref_t<Style>,
                           OverlayArrangementViewStyle> &&
             StyleOf<Style, Configuration>)
  ArrangementViewStyle(Style style)
      : kind_(Kind::custom),
        custom_(StyleBox<Configuration>::of(std::move(style))) {}

  static auto automatic() noexcept -> AutomaticArrangementViewStyle;

  static auto split() noexcept -> SplitArrangementViewStyle;

  static auto overlay() noexcept -> OverlayArrangementViewStyle;

  auto kind() const noexcept -> Kind { return kind_; }

  auto axes() const noexcept -> composition::Axes { return axes_; }

  auto make_body(Configuration configuration) const -> composition::Element;

  static auto arranged_axis(composition::Axes allowed, composition::Size space)
      -> composition::Axis;

  friend auto operator==(const ArrangementViewStyle& left,
                         const ArrangementViewStyle& right) -> bool;

 private:
  class SplitLayout;
  class OverlayLayout;

  ArrangementViewStyle(Kind kind, composition::Axes allowed) noexcept
      : kind_(kind), axes_(allowed) {}

  static auto split_body(Configuration configuration, composition::Axes allowed)
      -> composition::Element;

  static auto overlay_body(Configuration configuration,
                           composition::Axes allowed) -> composition::Element;

  Kind kind_;
  composition::Axes axes_{composition::Axes::all};
  std::shared_ptr<const StyleBox<Configuration>> custom_;
};

class AutomaticArrangementViewStyle {
 public:
  auto make_body(ArrangementViewStyle::Configuration configuration) const
      -> composition::Element;

  friend auto operator==(const AutomaticArrangementViewStyle&,
                         const AutomaticArrangementViewStyle&)
      -> bool = default;
};

class SplitArrangementViewStyle {
 public:
  constexpr SplitArrangementViewStyle() noexcept = default;

  constexpr auto axes(composition::Axes allowed) const noexcept
      -> SplitArrangementViewStyle {
    SplitArrangementViewStyle changed = *this;
    changed.axes_ = allowed;
    return changed;
  }

  constexpr auto axes() const noexcept -> composition::Axes { return axes_; }

  auto make_body(ArrangementViewStyle::Configuration configuration) const
      -> composition::Element;

  friend constexpr auto operator==(const SplitArrangementViewStyle&,
                                   const SplitArrangementViewStyle&)
      -> bool = default;

 private:
  composition::Axes axes_{composition::Axes::all};
};

class OverlayArrangementViewStyle {
 public:
  constexpr OverlayArrangementViewStyle() noexcept = default;

  constexpr auto axes(composition::Axes allowed) const noexcept
      -> OverlayArrangementViewStyle {
    OverlayArrangementViewStyle changed = *this;
    changed.axes_ = allowed;
    return changed;
  }

  constexpr auto axes() const noexcept -> composition::Axes { return axes_; }

  auto make_body(ArrangementViewStyle::Configuration configuration) const
      -> composition::Element;

  friend constexpr auto operator==(const OverlayArrangementViewStyle&,
                                   const OverlayArrangementViewStyle&)
      -> bool = default;

 private:
  composition::Axes axes_{composition::Axes::all};
};

}

namespace arisa::composition {

template <class Style>
auto Element::arrangement_view_style(Style style) && -> Element {
  return std::move(*this).environment(
      components::ArrangementViewStyle::key,
      components::ArrangementViewStyle{std::move(style)});
}

}
