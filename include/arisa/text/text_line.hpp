#pragma once

#include <cstdint>

namespace arisa::text {

struct TextLine {
  std::uint32_t text_start{0};
  std::uint32_t text_end{0};
  std::uint32_t first_run{0};
  std::uint32_t run_count{0};
  float x{0.0F};
  float top{0.0F};
  float baseline{0.0F};
  float width{0.0F};
  float height{0.0F};
  bool right_to_left{false};
};

}
