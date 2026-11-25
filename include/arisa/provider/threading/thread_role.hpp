#pragma once

#include <array>
#include <cstdint>

namespace arisa::provider {

enum class ThreadRole : std::uint8_t {
  ui,
  background,
  blocking,
  secondary_root,
  app,
  render,
};

inline constexpr std::array all_thread_roles{
    ThreadRole::ui,       ThreadRole::background,
    ThreadRole::blocking, ThreadRole::secondary_root,
    ThreadRole::app,      ThreadRole::render,
};

}
