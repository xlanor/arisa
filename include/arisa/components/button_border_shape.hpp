#pragma once

#include <arisa/components/control_size.hpp>
#include <arisa/composition/rect.hpp>
#include <arisa/graphics/path.hpp>
#include <arisa/theme/shape.hpp>

#include <cstdint>
#include <optional>

namespace arisa::components {

class ButtonBorderShape {
 public:
  enum class Kind : std::uint8_t {
    automatic,
    capsule,
    circle,
    rounded_rectangle,
  };

  static auto automatic() noexcept -> ButtonBorderShape {
    return ButtonBorderShape{Kind::automatic, std::nullopt};
  }

  static auto capsule() noexcept -> ButtonBorderShape {
    return ButtonBorderShape{Kind::capsule, std::nullopt};
  }

  static auto circle() noexcept -> ButtonBorderShape {
    return ButtonBorderShape{Kind::circle, std::nullopt};
  }

  static auto rounded_rectangle() noexcept -> ButtonBorderShape {
    return ButtonBorderShape{Kind::rounded_rectangle, std::nullopt};
  }

  static auto rounded_rectangle(float radius) noexcept -> ButtonBorderShape {
    return ButtonBorderShape{Kind::rounded_rectangle, radius};
  }

  auto kind() const noexcept -> Kind { return kind_; }

  auto shape(ControlSize size) const noexcept -> theme::Shape;

  auto inset(float amount) const noexcept -> ButtonBorderShape {
    ButtonBorderShape inset = *this;
    inset.inset_ += amount;
    return inset;
  }

  auto inset_amount() const noexcept -> float { return inset_; }

  auto path(composition::Rect rect,
            ControlSize size = ControlSize::regular) const -> graphics::Path;

  friend auto operator==(const ButtonBorderShape&, const ButtonBorderShape&)
      -> bool = default;

 private:
  ButtonBorderShape(Kind kind, std::optional<float> radius) noexcept
      : kind_(kind), radius_(radius) {}

  Kind kind_;
  std::optional<float> radius_;
  float inset_{0.0F};
};

}
