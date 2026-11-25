#pragma once

#include <arisa/components/style_box.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/environment_key.hpp>

#include <concepts>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

namespace arisa::controls {

class TextEditorStyle {
 public:
  struct Configuration {
    composition::Element content;
    bool shows_background{true};
  };

  static const composition::EnvironmentKey<TextEditorStyle> key;

  template <class Style>
    requires(!std::same_as<std::remove_cvref_t<Style>, TextEditorStyle> &&
             components::StyleOf<Style, Configuration>)
  TextEditorStyle(Style style)
      : kind_(Kind::custom),
        custom_(components::StyleBox<Configuration>::of(std::move(style))) {}

  static auto automatic() noexcept -> TextEditorStyle;

  static auto plain() noexcept -> TextEditorStyle;

  static auto rounded_border() noexcept -> TextEditorStyle;

  auto make_body(Configuration configuration) const -> composition::Element;

  friend auto operator==(const TextEditorStyle& left,
                         const TextEditorStyle& right) -> bool;

 private:
  enum class Kind : std::uint8_t {
    automatic,
    plain,
    rounded_border,
    custom,
  };

  explicit TextEditorStyle(Kind kind) noexcept : kind_(kind) {}

  Kind kind_;
  std::shared_ptr<const components::StyleBox<Configuration>> custom_;
};

}

namespace arisa::composition {

template <class Style>
auto Element::text_editor_style(Style style) && -> Element {
  return std::move(*this).environment(
      controls::TextEditorStyle::key,
      controls::TextEditorStyle{std::move(style)});
}

}
