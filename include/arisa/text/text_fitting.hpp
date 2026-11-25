#pragma once

#include <arisa/text/truncation_mode.hpp>

#include <cstddef>
#include <optional>

namespace arisa::text {

struct TextFitting {
  std::optional<std::size_t> lines;
  std::optional<float> height;
  TruncationMode truncation{TruncationMode::tail};
  float minimum_scale_factor{1.0F};
  bool allows_tightening{false};

  friend auto operator==(const TextFitting&, const TextFitting&)
      -> bool = default;
};

}
