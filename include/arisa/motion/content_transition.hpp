#pragma once

#include <cstdint>
#include <optional>

namespace arisa::motion {

class ContentTransition {
 public:
  enum class Kind : std::uint8_t {
    identity,
    opacity,
    interpolate,
    numeric_text,
    symbol_effect,
  };

  enum class SymbolReplace : std::uint8_t {
    down_up,
    up_up,
    off_up,
  };

  static const ContentTransition identity;
  static const ContentTransition opacity;
  static const ContentTransition interpolate;
  static const ContentTransition symbol_effect;

  static constexpr auto numeric_text(bool counts_down = false) noexcept
      -> ContentTransition {
    return ContentTransition{Kind::numeric_text, counts_down};
  }

  static constexpr auto numeric_text_value(double value) noexcept
      -> ContentTransition {
    ContentTransition made{Kind::numeric_text, false};
    made.value_ = value;
    return made;
  }

  static constexpr auto symbol_effect_replace(SymbolReplace replace,
                                              double speed = 1.0) noexcept
      -> ContentTransition {
    ContentTransition made{Kind::symbol_effect};
    made.replace_ = replace;
    made.speed_ = speed > 0.0 ? speed : 1.0;
    return made;
  }

  constexpr auto kind() const noexcept -> Kind { return kind_; }

  constexpr auto replace() const noexcept -> SymbolReplace { return replace_; }

  constexpr auto speed() const noexcept -> double { return speed_; }

  constexpr auto counts_down() const noexcept -> bool { return counts_down_; }

  constexpr auto value() const noexcept -> std::optional<double> {
    return value_;
  }

  constexpr auto moves() const noexcept -> bool {
    return kind_ == Kind::numeric_text || kind_ == Kind::interpolate ||
           kind_ == Kind::symbol_effect;
  }

  friend constexpr auto operator==(const ContentTransition&,
                                   const ContentTransition&) -> bool = default;

 private:
  constexpr explicit ContentTransition(Kind kind,
                                       bool counts_down = false) noexcept
      : kind_(kind), counts_down_(counts_down) {}

  Kind kind_;
  bool counts_down_;
  SymbolReplace replace_{SymbolReplace::down_up};
  double speed_{1.0};
  std::optional<double> value_{};
};

inline constexpr ContentTransition ContentTransition::identity{
    ContentTransition::Kind::identity};
inline constexpr ContentTransition ContentTransition::opacity{
    ContentTransition::Kind::opacity};
inline constexpr ContentTransition ContentTransition::interpolate{
    ContentTransition::Kind::interpolate};
inline constexpr ContentTransition ContentTransition::symbol_effect{
    ContentTransition::Kind::symbol_effect};

}
