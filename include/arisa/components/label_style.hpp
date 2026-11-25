#pragma once

#include <arisa/components/style_box.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/environment_key.hpp>

#include <concepts>
#include <cstdint>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace arisa::components {

class DefaultLabelStyle;
class TitleAndIconLabelStyle;
class TitleOnlyLabelStyle;
class IconOnlyLabelStyle;

class LabelStyle {
 public:
  struct Configuration {
    composition::Element icon;
    composition::Element title;
    std::optional<float> icon_to_title_spacing{};
    std::optional<float> reserved_icon_width{};
  };

  static const composition::EnvironmentKey<LabelStyle> key;

  LabelStyle(DefaultLabelStyle style) noexcept;

  LabelStyle(TitleAndIconLabelStyle style) noexcept;

  LabelStyle(TitleOnlyLabelStyle style) noexcept;

  LabelStyle(IconOnlyLabelStyle style) noexcept;

  template <class Style>
    requires(!std::same_as<std::remove_cvref_t<Style>, LabelStyle> &&
             !std::same_as<std::remove_cvref_t<Style>, DefaultLabelStyle> &&
             !std::same_as<std::remove_cvref_t<Style>,
                           TitleAndIconLabelStyle> &&
             !std::same_as<std::remove_cvref_t<Style>, TitleOnlyLabelStyle> &&
             !std::same_as<std::remove_cvref_t<Style>, IconOnlyLabelStyle> &&
             StyleOf<Style, Configuration>)
  LabelStyle(Style style)
      : kind_(Kind::custom),
        custom_(StyleBox<Configuration>::of(std::move(style))) {}

  static auto automatic() noexcept -> LabelStyle;

  static auto title_and_icon() noexcept -> LabelStyle;

  static auto title_only() noexcept -> LabelStyle;

  static auto icon_only() noexcept -> LabelStyle;

  auto make_body(Configuration configuration) const -> composition::Element;

  friend auto operator==(const LabelStyle& left, const LabelStyle& right)
      -> bool;

 private:
  enum class Kind : std::uint8_t {
    automatic,
    title_and_icon,
    title_only,
    icon_only,
    custom,
  };

  explicit LabelStyle(Kind kind) noexcept : kind_(kind) {}

  Kind kind_;
  std::shared_ptr<const StyleBox<Configuration>> custom_;
};

class DefaultLabelStyle {
 public:
  auto make_body(LabelStyle::Configuration configuration) const
      -> composition::Element;

  friend auto operator==(const DefaultLabelStyle&, const DefaultLabelStyle&)
      -> bool = default;
};

class TitleAndIconLabelStyle {
 public:
  auto make_body(LabelStyle::Configuration configuration) const
      -> composition::Element;

  friend auto operator==(const TitleAndIconLabelStyle&,
                         const TitleAndIconLabelStyle&) -> bool = default;
};

class TitleOnlyLabelStyle {
 public:
  auto make_body(LabelStyle::Configuration configuration) const
      -> composition::Element;

  friend auto operator==(const TitleOnlyLabelStyle&, const TitleOnlyLabelStyle&)
      -> bool = default;
};

class IconOnlyLabelStyle {
 public:
  auto make_body(LabelStyle::Configuration configuration) const
      -> composition::Element;

  friend auto operator==(const IconOnlyLabelStyle&, const IconOnlyLabelStyle&)
      -> bool = default;
};

}

namespace arisa::composition {

template <class Style>
auto Element::label_style(Style style) && -> Element {
  return std::move(*this).environment(components::LabelStyle::key,
                                      components::LabelStyle{std::move(style)});
}

}
