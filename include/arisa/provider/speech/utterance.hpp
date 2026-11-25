#pragma once

#include <string>

namespace arisa::provider {

struct Utterance {
  std::string text;
  double pitch{1.0};
  bool spells_out_characters{false};
  bool includes_punctuation{false};
  bool queued{false};

  friend auto operator==(const Utterance&, const Utterance&) -> bool = default;
};

}
