#pragma once

#include <array>
#include <cstddef>
#include <string_view>

namespace arisa::nx {

class ExitTrail {
 public:
  static void begin(std::string_view app) noexcept;

  static void mark(std::string_view stage) noexcept;

 private:
  static constexpr std::size_t path_capacity = 128;

  static void handlers_finished() noexcept;

  static inline std::array<char, path_capacity> path_{};
};

}
