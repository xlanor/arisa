#pragma once

#include <arisa/components/button_style.hpp>
#include <arisa/components/style_box.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/environment_key.hpp>

#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <type_traits>
#include <utility>

namespace arisa::controls {

class MenuStyle {
 public:
  struct Configuration {
    composition::Element label;
    std::function<void()> open;
    std::optional<components::ButtonStyle> button_style{};
  };

  static const composition::EnvironmentKey<MenuStyle> key;

  template <class Style>
    requires(!std::same_as<std::remove_cvref_t<Style>, MenuStyle> &&
             components::StyleOf<Style, Configuration>)
  MenuStyle(Style style)
      : kind_(Kind::custom),
        custom_(components::StyleBox<Configuration>::of(std::move(style))) {}

  static auto automatic() noexcept -> MenuStyle;

  static auto button() noexcept -> MenuStyle;

  static auto borderless_button() noexcept -> MenuStyle;

  static auto pull_down() noexcept -> MenuStyle;

  static auto borderless_pull_down() noexcept -> MenuStyle;

  auto pulls_down() const noexcept -> bool {
    return kind_ == Kind::pull_down || kind_ == Kind::borderless_pull_down;
  }

  auto make_body(Configuration configuration) const -> composition::Element;

  friend auto operator==(const MenuStyle& left, const MenuStyle& right) -> bool;

 private:
  enum class Kind : std::uint8_t {
    automatic,
    button,
    borderless_button,
    pull_down,
    borderless_pull_down,
    custom,
  };

  explicit MenuStyle(Kind kind) noexcept : kind_(kind) {}

  Kind kind_;
  std::shared_ptr<const components::StyleBox<Configuration>> custom_;
};

}

namespace arisa::composition {

template <class Style>
auto Element::menu_style(Style style) && -> Element {
  return std::move(*this).environment(controls::MenuStyle::key,
                                      controls::MenuStyle{std::move(style)});
}

}
