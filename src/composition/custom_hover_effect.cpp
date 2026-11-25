#include <arisa/composition/custom_hover_effect.hpp>

#include <algorithm>
#include <utility>

namespace arisa::composition {

CustomHoverEffect::CustomHoverEffect(motion::HoverEffect effect)
    : layers_{Layer{.kind = Layer::Kind::system,
                    .system = effect,
                    .body = nullptr,
                    .group = std::nullopt,
                    .enabled = true,
                    .phase = std::nullopt}} {}

auto CustomHoverEffect::empty() -> CustomHoverEffect {
  return CustomHoverEffect{};
}

auto CustomHoverEffect::hover_effect(CustomHoverEffect effect,
                                     std::optional<HoverEffectGroup> in,
                                     bool is_enabled) const
    -> CustomHoverEffect {
  CustomHoverEffect combined = *this;
  for (Layer layer : effect.layers_) {
    if (in.has_value() && !layer.group.has_value()) {
      layer.group = in;
    }
    layer.enabled = layer.enabled && is_enabled;
    combined.layers_.push_back(std::move(layer));
  }
  return combined;
}

auto CustomHoverEffect::hover_effect(Body body) const -> CustomHoverEffect {
  return hover_effect(std::nullopt, true, std::move(body));
}

auto CustomHoverEffect::hover_effect(std::optional<HoverEffectGroup> in,
                                     bool is_enabled, Body body) const
    -> CustomHoverEffect {
  CustomHoverEffect combined = *this;
  combined.layers_.push_back(
      Layer{.kind = Layer::Kind::content,
            .system = motion::HoverEffect::automatic,
            .body = std::make_shared<const Body>(std::move(body)),
            .group = std::move(in),
            .enabled = is_enabled,
            .phase = std::nullopt});
  return combined;
}

auto CustomHoverEffect::hover_effect_group(
    std::optional<HoverEffectGroup> group) const -> CustomHoverEffect {
  CustomHoverEffect grouped = *this;
  if (grouped.layers_.empty()) {
    if (group.has_value()) {
      grouped.layers_.push_back(Layer{.kind = Layer::Kind::group,
                                      .system = motion::HoverEffect::automatic,
                                      .body = nullptr,
                                      .group = std::move(group),
                                      .enabled = true,
                                      .phase = std::nullopt});
    }
    return grouped;
  }
  for (Layer& layer : grouped.layers_) {
    if (!group.has_value()) {
      layer.group.reset();
    } else if (!layer.group.has_value()) {
      layer.group = group;
    }
  }
  return grouped;
}

auto CustomHoverEffect::hover_effect_group(
    std::optional<std::string> id, Namespace::ID space,
    HoverEffectGroup::Behavior behavior) const -> CustomHoverEffect {
  return hover_effect_group(HoverEffectGroup{std::move(id), space, behavior});
}

auto CustomHoverEffect::hover_effect_disabled(bool disabled) const
    -> CustomHoverEffect {
  CustomHoverEffect changed = *this;
  if (disabled) {
    for (Layer& layer : changed.layers_) {
      layer.enabled = false;
    }
  }
  return changed;
}

auto CustomHoverEffect::hover_effect_phase_override(
    std::optional<HoverEffectPhaseOverride> override) const
    -> CustomHoverEffect {
  CustomHoverEffect changed = *this;
  for (Layer& layer : changed.layers_) {
    layer.phase = override;
  }
  return changed;
}

auto CustomHoverEffect::is_empty() const noexcept -> bool {
  return std::ranges::none_of(layers_, [](const Layer& layer) {
    return layer.enabled && layer.kind != Layer::Kind::group;
  });
}

auto CustomHoverEffect::resolved(
    const std::optional<CustomHoverEffect>& fallback) const
    -> CustomHoverEffect {
  if (!fallback.has_value()) {
    return *this;
  }
  CustomHoverEffect substituted;
  for (const Layer& layer : layers_) {
    if (layer.kind != Layer::Kind::system ||
        layer.system.kind() != motion::HoverEffect::Kind::automatic) {
      substituted.layers_.push_back(layer);
      continue;
    }
    for (Layer inherited : fallback->layers_) {
      if (layer.group.has_value() && !inherited.group.has_value()) {
        inherited.group = layer.group;
      }
      inherited.enabled = inherited.enabled && layer.enabled;
      if (layer.phase.has_value()) {
        inherited.phase = layer.phase;
      }
      substituted.layers_.push_back(std::move(inherited));
    }
  }
  return substituted;
}

}
