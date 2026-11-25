#pragma once

#include <arisa/composition/implicit_animation.hpp>
#include <arisa/composition/matched_geometry.hpp>
#include <arisa/motion/animation.hpp>
#include <arisa/motion/transition.hpp>

#include <optional>
#include <vector>

namespace arisa::composition {

struct MotionDeclarations {
  std::optional<motion::Transition> transition;
  std::optional<motion::Animation> transition_animation;
  std::optional<MatchedGeometry> matched;
  std::optional<MatchedGeometry::Key> zoom;
  std::vector<ImplicitAnimation> animations;
  std::optional<std::optional<motion::Animation>> scoped_animation;
};

}
