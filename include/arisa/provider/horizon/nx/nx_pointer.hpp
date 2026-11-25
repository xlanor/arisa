#pragma once

#include <arisa/provider/error.hpp>
#include <arisa/provider/horizon/pointer_reading.hpp>
#include <arisa/provider/input/provider.hpp>
#include <arisa/provider/surface/surface_size.hpp>

#include <switch.h>

#include <cstddef>
#include <cstdint>
#include <optional>

namespace arisa::provider::horizon::nx {

class NxPointer {
 public:
  static constexpr SurfaceSize mouse_area{.width = SurfaceWidth{1280},
                                          .height = SurfaceHeight{720}};
  static constexpr std::size_t history = 17;

  explicit NxPointer(SurfaceSize surface);

  auto sample(SurfaceSize surface) -> PointerReading;

  auto failure() const noexcept -> std::optional<ProviderError> {
    return std::nullopt;
  }

  static auto position(const HidMouseState& state, SurfaceSize surface) noexcept
      -> PixelPoint;

 private:
  std::optional<PointerState> state_;
  std::uint64_t sampled_{0};
};

}
