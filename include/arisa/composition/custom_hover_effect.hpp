#pragma once

#include <arisa/composition/geometry_proxy.hpp>
#include <arisa/composition/hover_effect_group.hpp>
#include <arisa/composition/hover_effect_phase_override.hpp>
#include <arisa/composition/namespace.hpp>
#include <arisa/composition/visual_effect.hpp>
#include <arisa/motion/hover_effect.hpp>

#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace arisa::composition {

class CustomHoverEffect;

template <class T>
concept HoverEffectBody =
    !std::derived_from<std::remove_cvref_t<T>, CustomHoverEffect> &&
    requires(const T& effect, CustomHoverEffect content) {
      { effect.body(content) } -> std::convertible_to<CustomHoverEffect>;
    };

class CustomHoverEffect {
 public:
  using Body = std::function<VisualEffect(VisualEffect content, bool is_active,
                                          const GeometryProxy& proxy)>;

  struct Layer {
    enum class Kind : std::uint8_t {
      system,
      content,
      group,
    };

    Kind kind{Kind::system};
    motion::HoverEffect system{motion::HoverEffect::automatic};
    std::shared_ptr<const Body> body;
    std::optional<HoverEffectGroup> group;
    bool enabled{true};
    std::optional<HoverEffectPhaseOverride> phase;

    friend auto operator==(const Layer&, const Layer&) -> bool = default;
  };

  CustomHoverEffect(motion::HoverEffect effect);

  template <HoverEffectBody T>
  CustomHoverEffect(const T& effect)
      : CustomHoverEffect(effect.body(CustomHoverEffect::empty())) {}

  static auto empty() -> CustomHoverEffect;

  auto hover_effect(CustomHoverEffect effect,
                    std::optional<HoverEffectGroup> in = std::nullopt,
                    bool is_enabled = true) const -> CustomHoverEffect;

  auto hover_effect(Body body) const -> CustomHoverEffect;

  auto hover_effect(std::optional<HoverEffectGroup> in, bool is_enabled,
                    Body body) const -> CustomHoverEffect;

  auto hover_effect_group(std::optional<HoverEffectGroup> group) const
      -> CustomHoverEffect;

  auto hover_effect_group(std::optional<std::string> id, Namespace::ID space,
                          HoverEffectGroup::Behavior behavior =
                              HoverEffectGroup::Behavior::activates_group) const
      -> CustomHoverEffect;

  auto hover_effect_disabled(bool disabled = true) const -> CustomHoverEffect;

  auto hover_effect_phase_override(
      std::optional<HoverEffectPhaseOverride> override) const
      -> CustomHoverEffect;

  auto layers() const noexcept -> std::span<const Layer> { return layers_; }

  auto is_empty() const noexcept -> bool;

  auto resolved(const std::optional<CustomHoverEffect>& fallback) const
      -> CustomHoverEffect;

  friend auto operator==(const CustomHoverEffect&, const CustomHoverEffect&)
      -> bool = default;

 private:
  CustomHoverEffect() = default;

  std::vector<Layer> layers_;
};

}
