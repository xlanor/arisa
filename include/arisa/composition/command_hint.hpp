#pragma once

#include <arisa/provider/input/buttons.hpp>

#include <string>

namespace arisa::composition {

struct CommandHint {
  provider::Button button{provider::Button::a};
  std::string label;

  friend auto operator==(const CommandHint&, const CommandHint&)
      -> bool = default;
};

}
