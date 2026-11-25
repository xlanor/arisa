#pragma once

#include <arisa/composition/custom_hover_effect.hpp>

namespace arisa::composition {

class EmptyHoverEffect : public CustomHoverEffect {
 public:
  EmptyHoverEffect() : CustomHoverEffect(CustomHoverEffect::empty()) {}
};

}
