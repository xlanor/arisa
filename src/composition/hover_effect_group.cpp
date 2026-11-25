#include <arisa/composition/hover_effect_group.hpp>

namespace arisa::composition {

auto HoverEffectGroup::system_overlays() -> HoverEffectGroup {
  static constexpr Namespace overlays{"arisa.hover.system_overlays"};
  return HoverEffectGroup{overlays.id()};
}

}
