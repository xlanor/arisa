#pragma once

#include <arisa/components/style_box.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/environment_key.hpp>

#include <concepts>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

namespace arisa::structure {

class GroupBoxStyle {
 public:
  struct Configuration {
    composition::Element label;
    composition::Element content;
  };

  static const composition::EnvironmentKey<GroupBoxStyle> key;

  static constexpr float padding = 16.0F;
  static constexpr float label_spacing = 8.0F;

  template <class Style>
    requires(!std::same_as<std::remove_cvref_t<Style>, GroupBoxStyle> &&
             components::StyleOf<Style, Configuration>)
  GroupBoxStyle(Style style)
      : kind_(Kind::custom),
        custom_(components::StyleBox<Configuration>::of(std::move(style))) {}

  static auto automatic() noexcept -> GroupBoxStyle;

  auto make_body(Configuration configuration) const -> composition::Element;

  friend auto operator==(const GroupBoxStyle& left, const GroupBoxStyle& right)
      -> bool;

 private:
  enum class Kind : std::uint8_t {
    automatic,
    custom,
  };

  explicit GroupBoxStyle(Kind kind) noexcept : kind_(kind) {}

  Kind kind_;
  std::shared_ptr<const components::StyleBox<Configuration>> custom_;
};

}

namespace arisa::composition {

template <class Style>
auto Element::group_box_style(Style style) && -> Element {
  return std::move(*this).environment(
      structure::GroupBoxStyle::key,
      structure::GroupBoxStyle{std::move(style)});
}

}
