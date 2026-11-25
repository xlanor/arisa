#pragma once

#include <arisa/composition/element_key.hpp>
#include <arisa/composition/namespace.hpp>
#include <arisa/theme/glass.hpp>
#include <arisa/theme/material.hpp>
#include <arisa/theme/shape.hpp>

#include <cstdint>
#include <optional>

namespace arisa::composition {

class GlassEffectTransition {
 public:
  enum class Kind : std::uint8_t { matched_geometry, materialize, identity };

  static constexpr float materialize_blur = 12.0F;

  static constexpr auto matched_geometry() noexcept -> GlassEffectTransition {
    return GlassEffectTransition{Kind::matched_geometry};
  }

  static constexpr auto materialize() noexcept -> GlassEffectTransition {
    return GlassEffectTransition{Kind::materialize};
  }

  static constexpr auto identity() noexcept -> GlassEffectTransition {
    return GlassEffectTransition{Kind::identity};
  }

  constexpr auto kind() const noexcept -> Kind { return kind_; }

  friend constexpr auto operator==(GlassEffectTransition, GlassEffectTransition)
      -> bool = default;

 private:
  constexpr explicit GlassEffectTransition(Kind kind) noexcept : kind_(kind) {}

  Kind kind_;
};

struct GlassEffect {
  struct Tag {
    Namespace::ID space;
    ElementKey id;

    friend auto operator==(const Tag&, const Tag&) -> bool = default;
  };

  std::optional<theme::Glass> glass;
  theme::Shape shape{theme::Shape::capsule()};
  std::optional<theme::GlassSlot> slot;
  std::optional<Tag> id;
  std::optional<Tag> unioned;
  std::optional<theme::Material> frost;
  GlassEffectTransition transition{GlassEffectTransition::matched_geometry()};

  friend auto operator==(const GlassEffect&, const GlassEffect&)
      -> bool = default;
};

}
