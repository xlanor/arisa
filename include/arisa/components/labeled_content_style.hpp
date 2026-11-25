#pragma once

#include <arisa/components/style_box.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/environment_key.hpp>

#include <concepts>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

namespace arisa::components {

class LabelColumn;

class LabeledContentStyle {
 public:
  struct Configuration {
    composition::Element label;
    composition::Element content;
  };

  static const composition::EnvironmentKey<LabeledContentStyle> key;

  template <class Style>
    requires(!std::same_as<std::remove_cvref_t<Style>, LabeledContentStyle> &&
             StyleOf<Style, Configuration>)
  LabeledContentStyle(Style style)
      : kind_(Kind::custom),
        custom_(StyleBox<Configuration>::of(std::move(style))) {}

  static auto automatic() noexcept -> LabeledContentStyle;

  auto is_automatic() const noexcept -> bool {
    return kind_ == Kind::automatic;
  }

  auto make_body(Configuration configuration) const -> composition::Element;

  static auto columned(Configuration configuration, const LabelColumn& column)
      -> composition::Element;

  friend auto operator==(const LabeledContentStyle& left,
                         const LabeledContentStyle& right) -> bool;

 private:
  enum class Kind : std::uint8_t {
    automatic,
    custom,
  };

  explicit LabeledContentStyle(Kind kind) noexcept : kind_(kind) {}

  Kind kind_;
  std::shared_ptr<const StyleBox<Configuration>> custom_;
};

}

namespace arisa::composition {

template <class Style>
auto Element::labeled_content_style(Style style) && -> Element {
  return std::move(*this).environment(
      components::LabeledContentStyle::key,
      components::LabeledContentStyle{std::move(style)});
}

}
