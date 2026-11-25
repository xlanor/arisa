#pragma once

#include <arisa/support/strong_value.hpp>

#include <cstdint>

namespace arisa::provider {

struct SurfaceWidthTag;
struct SurfaceHeightTag;

using SurfaceWidth = support::StrongValue<std::uint32_t, SurfaceWidthTag>;
using SurfaceHeight = support::StrongValue<std::uint32_t, SurfaceHeightTag>;

struct SurfaceSize {
  SurfaceWidth width{0};
  SurfaceHeight height{0};

  constexpr auto empty() const noexcept -> bool {
    return width.value() == 0 || height.value() == 0;
  }

  friend constexpr auto operator==(const SurfaceSize&, const SurfaceSize&)
      -> bool = default;
};

}
