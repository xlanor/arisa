#pragma once

#include <arisa/composition/custom_hover_effect.hpp>
#include <arisa/composition/hover_effect_group.hpp>

#include <optional>
#include <utility>

namespace arisa::composition {

class ContentHoverEffect {
 public:
  explicit ContentHoverEffect(CustomHoverEffect::Body body)
      : body_(std::move(body)) {}

  ContentHoverEffect(std::optional<HoverEffectGroup> in, bool is_enabled,
                     CustomHoverEffect::Body body)
      : group_(std::move(in)), enabled_(is_enabled), body_(std::move(body)) {}

  auto body(CustomHoverEffect content) const -> CustomHoverEffect {
    return content.hover_effect(group_, enabled_, body_);
  }

 private:
  std::optional<HoverEffectGroup> group_;
  bool enabled_{true};
  CustomHoverEffect::Body body_;
};

}
