#pragma once

#include <arisa/composition/element.hpp>
#include <arisa/composition/element_type.hpp>

#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>

namespace arisa::components {

template <class Style, class Configuration>
concept StyleOf =
    std::is_class_v<Style> &&
    requires(const Style& style, Configuration configuration) {
      {
        style.make_body(std::move(configuration))
      } -> std::convertible_to<composition::Element>;
    } && std::copy_constructible<Style> && std::equality_comparable<Style>;

template <class Configuration>
class StyleBox {
 public:
  template <class Style>
  class Typed;

  StyleBox() = default;
  StyleBox(const StyleBox&) = delete;
  auto operator=(const StyleBox&) -> StyleBox& = delete;
  StyleBox(StyleBox&&) = delete;
  auto operator=(StyleBox&&) -> StyleBox& = delete;
  virtual ~StyleBox() = default;

  template <StyleOf<Configuration> Style>
  static auto of(Style style) -> std::shared_ptr<const StyleBox> {
    return std::make_shared<const Typed<Style>>(std::move(style));
  }

  virtual auto make_body(Configuration configuration) const
      -> composition::Element = 0;

  virtual auto kind() const noexcept -> composition::ElementType = 0;

  virtual auto equals(const StyleBox& other) const -> bool = 0;
};

template <class Configuration>
template <class Style>
class StyleBox<Configuration>::Typed final : public StyleBox<Configuration> {
 public:
  explicit Typed(Style style) : style_(std::move(style)) {}

  auto make_body(Configuration configuration) const
      -> composition::Element override {
    return composition::Element{style_.make_body(std::move(configuration))};
  }

  auto kind() const noexcept -> composition::ElementType override {
    return composition::ElementType::of<Typed>();
  }

  auto equals(const StyleBox<Configuration>& other) const -> bool override {
    return other.kind() == kind() &&
           static_cast<const Typed&>(other).style_ == style_;
  }

 private:
  Style style_;
};

}
