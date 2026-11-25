#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>

namespace arisa::composition {

class PresentationDetent {
 public:
  struct Context {
    float max_detent_value{0.0F};
  };

  using Height = std::optional<float> (*)(const Context& context);

  static const PresentationDetent medium;
  static const PresentationDetent large;

  template <class Custom>
    requires requires(const Context& context) {
      { Custom::height(context) } -> std::convertible_to<std::optional<float>>;
    }
  static constexpr auto custom() noexcept -> PresentationDetent {
    return PresentationDetent{
        Kind::custom, 0.0F, [](const Context& context) -> std::optional<float> {
          return Custom::height(context);
        }};
  }

  constexpr auto available(float largest) const -> bool {
    return kind_ != Kind::custom ||
           height_(Context{.max_detent_value = largest}).has_value();
  }

  static constexpr auto fraction(float value) noexcept -> PresentationDetent {
    return PresentationDetent{Kind::fraction, std::clamp(value, 0.0F, 1.0F)};
  }

  static constexpr auto height(float points) noexcept -> PresentationDetent {
    return PresentationDetent{Kind::height, std::max(points, 0.0F)};
  }

  constexpr auto resolve(float largest) const -> float {
    switch (kind_) {
      case Kind::medium:
        return largest * medium_fraction;
      case Kind::fraction:
        return largest * value_;
      case Kind::height:
        return std::min(value_, largest);
      case Kind::custom:
        return std::clamp(
            height_(Context{.max_detent_value = largest}).value_or(largest),
            0.0F, largest);
      case Kind::large:
        break;
    }
    return largest;
  }

  friend constexpr auto operator==(const PresentationDetent&,
                                   const PresentationDetent&) -> bool = default;

 private:
  enum class Kind : std::uint8_t {
    medium,
    large,
    fraction,
    height,
    custom,
  };

  static constexpr float medium_fraction = 0.5F;

  constexpr PresentationDetent(Kind kind, float value,
                               Height measured = nullptr) noexcept
      : kind_(kind), value_(value), height_(measured) {}

  Kind kind_;
  float value_;
  Height height_;
};

inline constexpr PresentationDetent PresentationDetent::medium{
    PresentationDetent::Kind::medium, 0.0F};
inline constexpr PresentationDetent PresentationDetent::large{
    PresentationDetent::Kind::large, 0.0F};

}
