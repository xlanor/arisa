#pragma once

#include <arisa/composition/alignment.hpp>
#include <arisa/composition/overlay_input.hpp>
#include <arisa/motion/transition.hpp>
#include <arisa/theme/space.hpp>

#include <chrono>
#include <optional>

namespace arisa::runtime {

struct OverlayBehaviour {
  composition::OverlayInput input{composition::OverlayInput::passthrough};
  composition::Alignment alignment{composition::Alignment::bottom};
  motion::Transition transition{motion::Transition::move(theme::Edge::bottom)
                                    .combined(motion::Transition::opacity())};
  std::optional<std::chrono::nanoseconds> duration{};
  bool dismiss_on_back{false};
  bool dismiss_on_tap_outside{false};
};

}
