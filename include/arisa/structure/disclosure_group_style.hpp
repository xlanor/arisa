#pragma once

#include <arisa/components/style_box.hpp>
#include <arisa/composition/binding.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/environment_key.hpp>

#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace arisa::structure {

class DisclosureGroupStyle {
 public:
  struct Configuration {
    composition::Element label;
    std::function<composition::Element()> content;
    composition::Binding<bool> is_expanded;
  };

  static const composition::EnvironmentKey<DisclosureGroupStyle> key;

  static constexpr float indent = 20.0F;
  static constexpr float chevron_extent = 13.0F;
  static constexpr double expanded_degrees = 90.0;

  template <class Style>
    requires(!std::same_as<std::remove_cvref_t<Style>, DisclosureGroupStyle> &&
             components::StyleOf<Style, Configuration>)
  DisclosureGroupStyle(Style style)
      : kind_(Kind::custom),
        custom_(components::StyleBox<Configuration>::of(std::move(style))) {}

  static auto automatic() noexcept -> DisclosureGroupStyle;

  auto make_body(Configuration configuration) const -> composition::Element;

  friend auto operator==(const DisclosureGroupStyle& left,
                         const DisclosureGroupStyle& right) -> bool;

 private:
  enum class Kind : std::uint8_t {
    automatic,
    custom,
  };

  explicit DisclosureGroupStyle(Kind kind) noexcept : kind_(kind) {}

  static auto disclosed(Configuration configuration) -> composition::Element;

  Kind kind_;
  std::shared_ptr<const components::StyleBox<Configuration>> custom_;
};

}

namespace arisa::composition {

template <class Style>
auto Element::disclosure_group_style(Style style) && -> Element {
  return std::move(*this).environment(
      structure::DisclosureGroupStyle::key,
      structure::DisclosureGroupStyle{std::move(style)});
}

}
