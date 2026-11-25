#pragma once

#include <arisa/composition/element.hpp>
#include <arisa/composition/environment_key.hpp>
#include <arisa/composition/symbol_variants.hpp>

#include <cstdint>
#include <utility>

namespace arisa::controls {

class PaletteSelectionEffect {
 public:
  enum class Kind : std::uint8_t { automatic, custom, symbol_variant };

  static const PaletteSelectionEffect automatic;
  static const PaletteSelectionEffect custom;

  static const composition::EnvironmentKey<PaletteSelectionEffect> key;

  static constexpr auto symbol_variant(
      composition::SymbolVariants variant) noexcept -> PaletteSelectionEffect {
    return PaletteSelectionEffect{Kind::symbol_variant, variant};
  }

  constexpr auto kind() const noexcept -> Kind { return kind_; }

  constexpr auto variant() const noexcept -> composition::SymbolVariants {
    return variant_;
  }

  friend constexpr auto operator==(const PaletteSelectionEffect&,
                                   const PaletteSelectionEffect&)
      -> bool = default;

 private:
  constexpr explicit PaletteSelectionEffect(
      Kind kind, composition::SymbolVariants variant =
                     composition::SymbolVariants::none) noexcept
      : kind_(kind), variant_(variant) {}

  Kind kind_;
  composition::SymbolVariants variant_;
};

inline constexpr PaletteSelectionEffect PaletteSelectionEffect::automatic{
    PaletteSelectionEffect::Kind::automatic};
inline constexpr PaletteSelectionEffect PaletteSelectionEffect::custom{
    PaletteSelectionEffect::Kind::custom};

}

namespace arisa::composition {

template <class Effect>
auto Element::palette_selection_effect(Effect effect) && -> Element {
  return std::move(*this).environment(controls::PaletteSelectionEffect::key,
                                      controls::PaletteSelectionEffect{effect});
}

}
