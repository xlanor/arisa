#pragma once

#include <arisa/composition/axes.hpp>
#include <arisa/composition/environment_values.hpp>

namespace arisa::composition {

struct ScrollTargetBehaviorProperties {
  bool limits_scrolls{false};

  friend constexpr auto operator==(const ScrollTargetBehaviorProperties&,
                                   const ScrollTargetBehaviorProperties&)
      -> bool = default;
};

struct ScrollTargetBehaviorPropertiesContext {
  Axes axes{Axes::vertical};
  bool horizontally_compact{true};
  EnvironmentValues environment{};
};

}
