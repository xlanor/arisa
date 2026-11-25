#pragma once

#include <arisa/composition/element.hpp>
#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/graphics/path.hpp>
#include <arisa/theme/shape.hpp>

#include <cstdint>
#include <utility>

namespace arisa::controls {

class TextInputBorderShape {
 public:
  enum class Kind : std::uint8_t {
    automatic,
    capsule,
    rounded_rectangle,
  };

  static const composition::EnvironmentKey<TextInputBorderShape> key;

  static constexpr auto automatic() noexcept -> TextInputBorderShape {
    return TextInputBorderShape{Kind::automatic, 0.0F};
  }

  static constexpr auto text_input_border() noexcept -> TextInputBorderShape {
    return automatic();
  }

  static constexpr auto capsule() noexcept -> TextInputBorderShape {
    return TextInputBorderShape{Kind::capsule, 0.0F};
  }

  static constexpr auto rounded_rectangle() noexcept -> TextInputBorderShape {
    return TextInputBorderShape{Kind::rounded_rectangle, 0.0F};
  }

  constexpr auto kind() const noexcept -> Kind { return kind_; }

  constexpr auto inset_amount() const noexcept -> float { return inset_; }

  constexpr auto inset(float amount) const noexcept -> TextInputBorderShape {
    return TextInputBorderShape{kind_, inset_ + amount};
  }

  auto shape() const noexcept -> theme::Shape;

  auto path(composition::Rect rect) const -> graphics::Path;

  friend constexpr auto operator==(const TextInputBorderShape&,
                                   const TextInputBorderShape&)
      -> bool = default;

 private:
  constexpr TextInputBorderShape(Kind kind, float inset) noexcept
      : kind_(kind), inset_(inset) {}

  Kind kind_;
  float inset_;
};

}

namespace arisa::composition {

template <class Shape>
auto Element::text_input_border_shape(Shape shape) && -> Element {
  return std::move(*this).environment(controls::TextInputBorderShape::key,
                                      controls::TextInputBorderShape{shape});
}

}
