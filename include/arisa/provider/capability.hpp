#pragma once

#include <cstdint>
#include <utility>

namespace arisa::provider {

enum class Capability : std::uint32_t {
  none = 0,
  threading = 1U << 0U,
  clock = 1U << 1U,
  diagnostics = 1U << 2U,
  assets = 1U << 3U,
  lifecycle = 1U << 4U,
  system_info = 1U << 5U,
  haptics = 1U << 6U,
  audio = 1U << 7U,
  clipboard = 1U << 8U,
  browser = 1U << 9U,
  fonts = 1U << 10U,
  surface = 1U << 11U,
  input = 1U << 12U,
  gpu = 1U << 13U,
  text_input = 1U << 14U,
  applets = 1U << 15U,
  speech = 1U << 16U,
  required = threading | clock | diagnostics | assets,
  optional = lifecycle | system_info | haptics | audio | clipboard | browser |
      fonts | surface | input | gpu | text_input | applets | speech,
  all = required | optional,
};

constexpr auto operator|(Capability left, Capability right) noexcept
    -> Capability {
  return static_cast<Capability>(std::to_underlying(left) |
                                 std::to_underlying(right));
}

constexpr auto operator&(Capability left, Capability right) noexcept
    -> Capability {
  return static_cast<Capability>(std::to_underlying(left) &
                                 std::to_underlying(right));
}

constexpr auto operator^(Capability left, Capability right) noexcept
    -> Capability {
  return static_cast<Capability>(std::to_underlying(left) ^
                                 std::to_underlying(right));
}

constexpr auto operator~(Capability value) noexcept -> Capability {
  return static_cast<Capability>(~std::to_underlying(value)) & Capability::all;
}

constexpr auto operator|=(Capability& left, Capability right) noexcept
    -> Capability& {
  left = left | right;
  return left;
}

constexpr auto operator&=(Capability& left, Capability right) noexcept
    -> Capability& {
  left = left & right;
  return left;
}

constexpr auto contains(Capability set, Capability wanted) noexcept -> bool {
  return wanted != Capability::none && (set & wanted) == wanted;
}

}
