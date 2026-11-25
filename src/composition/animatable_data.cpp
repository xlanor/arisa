#include <arisa/composition/animatable_data.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/state_key.hpp>
#include <arisa/motion/animated_value.hpp>

namespace arisa::composition {

auto AnimatableData::present(Context& context,
                             const motion::AnimatableVector& target)
    -> motion::AnimatableVector {
  static const StateKey<motion::AnimatedValue> value{"animatable_data"};
  motion::AnimatedValue& animated = context.retained(value);
  const motion::AnimatableVector shown =
      animated.animate(target, context.motion_moment());
  if (animated.animating()) {
    context.request_frame();
  }
  return shown;
}

}
