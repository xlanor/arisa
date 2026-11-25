#pragma once

#include <optional>

namespace arisa::interaction {

struct RecognizerSignals {
  bool claimed{false};
  bool failed{false};
  bool ended{false};
  bool changed{false};
  std::optional<bool> pressing;
};

}
