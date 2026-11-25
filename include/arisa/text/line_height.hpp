#pragma once

#include <cstdint>
#include <optional>

namespace arisa::text {

class LineHeight {
 public:
  enum class Kind : std::uint8_t {
    normal,
    variable,
    tight,
    loose,
    multiple,
    leading,
    exact,
  };

  static constexpr float tight_factor = 1.0F;
  static constexpr float loose_factor = 1.5F;

  static const LineHeight normal;
  static const LineHeight variable;
  static const LineHeight tight;
  static const LineHeight loose;

  static constexpr auto multiple(float factor) noexcept -> LineHeight {
    return LineHeight{Kind::multiple, factor};
  }

  static constexpr auto leading(float increase) noexcept -> LineHeight {
    return LineHeight{Kind::leading, increase};
  }

  static constexpr auto exact(float points) noexcept -> LineHeight {
    return LineHeight{Kind::exact, points};
  }

  constexpr auto kind() const noexcept -> Kind { return kind_; }

  constexpr auto value() const noexcept -> float { return value_; }

  constexpr auto points(float size, float natural) const noexcept
      -> std::optional<float> {
    switch (kind_) {
      case Kind::normal:
      case Kind::variable:
        return std::nullopt;
      case Kind::tight:
        return size * tight_factor;
      case Kind::loose:
        return size * loose_factor;
      case Kind::multiple:
        return size * value_;
      case Kind::leading:
        return natural + value_;
      case Kind::exact:
        return value_;
    }
    return std::nullopt;
  }

  friend constexpr auto operator==(const LineHeight&, const LineHeight&)
      -> bool = default;

 private:
  constexpr LineHeight(Kind kind, float value) noexcept
      : kind_(kind), value_(value) {}

  Kind kind_;
  float value_;
};

inline constexpr LineHeight LineHeight::normal{Kind::normal, 0.0F};
inline constexpr LineHeight LineHeight::variable{Kind::variable, 0.0F};
inline constexpr LineHeight LineHeight::tight{Kind::tight, 0.0F};
inline constexpr LineHeight LineHeight::loose{Kind::loose, 0.0F};

}
