#pragma once

#include <arisa/composition/custom_hover_effect.hpp>
#include <arisa/composition/hover_effect_group.hpp>
#include <arisa/composition/namespace.hpp>

#include <optional>
#include <string>
#include <utility>

namespace arisa::composition {

class GroupHoverEffect {
 public:
  explicit GroupHoverEffect(std::optional<HoverEffectGroup> group)
      : group_(std::move(group)) {}

  GroupHoverEffect(std::optional<std::string> id, Namespace::ID space,
                   HoverEffectGroup::Behavior behavior =
                       HoverEffectGroup::Behavior::activates_group)
      : group_(HoverEffectGroup{std::move(id), space, behavior}) {}

  auto body(CustomHoverEffect content) const -> CustomHoverEffect {
    return content.hover_effect_group(group_);
  }

 private:
  std::optional<HoverEffectGroup> group_;
};

}
