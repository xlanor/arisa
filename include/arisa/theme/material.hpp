#pragma once

#include <arisa/theme/adaptive.hpp>
#include <arisa/theme/colour.hpp>
#include <arisa/theme/elevation.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace arisa::theme {

class Material {
 public:
  enum class Token : std::uint8_t {
    ultra_thin,
    thin,
    regular,
    thick,
    ultra_thick,
    sidebar,
    menu,
    hud,
    bar,
  };

  static constexpr std::size_t count = 9;

  static const Material ultra_thin;
  static const Material thin;
  static const Material regular;
  static const Material thick;
  static const Material ultra_thick;
  static const Material sidebar;
  static const Material menu;
  static const Material hud;
  static const Material bar;

  static constexpr std::array<std::string_view, count> names{
      "ultra_thin", "thin", "regular", "thick", "ultra_thick",
      "sidebar",    "menu", "hud",     "bar"};

  constexpr auto token() const noexcept -> Token { return token_; }

  constexpr auto index() const noexcept -> std::size_t {
    return static_cast<std::size_t>(token_);
  }

  constexpr auto name() const noexcept -> std::string_view {
    return names[index()];
  }

  static constexpr auto parse(std::string_view text) noexcept
      -> std::optional<Material> {
    for (std::size_t index = 0; index < count; ++index) {
      if (names[index] == text) {
        return Material{static_cast<Token>(index)};
      }
    }
    return std::nullopt;
  }

  static constexpr auto all() noexcept -> std::array<Material, count> {
    return {Material{Token::ultra_thin},  Material{Token::thin},
            Material{Token::regular},     Material{Token::thick},
            Material{Token::ultra_thick}, Material{Token::sidebar},
            Material{Token::menu},        Material{Token::hud},
            Material{Token::bar}};
  }

  friend constexpr auto operator==(const Material&, const Material&)
      -> bool = default;

 private:
  constexpr explicit Material(Token token) noexcept : token_(token) {}

  Token token_;
};

inline constexpr Material Material::ultra_thin{Material::Token::ultra_thin};
inline constexpr Material Material::thin{Material::Token::thin};
inline constexpr Material Material::regular{Material::Token::regular};
inline constexpr Material Material::thick{Material::Token::thick};
inline constexpr Material Material::ultra_thick{Material::Token::ultra_thick};
inline constexpr Material Material::sidebar{Material::Token::sidebar};
inline constexpr Material Material::menu{Material::Token::menu};
inline constexpr Material Material::hud{Material::Token::hud};
inline constexpr Material Material::bar{Material::Token::bar};

struct MaterialStyle {
  float blur{24.0F};
  float saturation{1.8F};
  Adaptive<Colour> tint{};
  Adaptive<float> luminosity{};
  float noise{0.015F};

  friend constexpr auto operator==(const MaterialStyle&, const MaterialStyle&)
      -> bool = default;
};

struct LiquidGlassStyle {
  float refraction{43.0F};
  float rim{0.3F};
  float blur{4.6F};
  float clear_blur{0.0F};
  float highlight{1.0F};
  float rim_width{0.0F};
  Adaptive<Colour> tint{};
  ElevationStyle shadow{};

  friend constexpr auto operator==(const LiquidGlassStyle&,
                                   const LiquidGlassStyle&) -> bool = default;
};

}
