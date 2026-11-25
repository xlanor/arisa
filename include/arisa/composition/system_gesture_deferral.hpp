#pragma once

#include <arisa/composition/preference_key.hpp>
#include <arisa/theme/space.hpp>

#include <cstdint>
#include <utility>

namespace arisa::composition {

class SystemGestureDeferral {
 public:
  static const PreferenceKey<std::uint8_t> key;

  static constexpr auto bits(theme::Edge edges) noexcept -> std::uint8_t {
    return std::to_underlying(edges);
  }
};

}
