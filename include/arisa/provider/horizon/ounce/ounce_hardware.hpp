#pragma once

#include <arisa/provider/error.hpp>
#include <arisa/provider/horizon/ounce/ounce_pointer.hpp>
#include <arisa/provider/horizon/ounce/ounce_rumble.hpp>
#include <arisa/provider/input/touch_panel.hpp>
#include <arisa/provider/surface/surface_size.hpp>
#include <arisa/provider/threading/thread_placement_policy.hpp>

#include <switch.h>

#include <expected>
#include <optional>

namespace arisa::provider::horizon::ounce {

class OunceHardware {
 public:
  using Rumble = OunceRumble;
  using Pointer = OuncePointer;

  static constexpr SurfaceSize handheld_size{.width = SurfaceWidth{1920},
                                             .height = SurfaceHeight{1080}};
  static constexpr TouchPanel touch_panel{
      SurfaceSize{.width = SurfaceWidth{1280}, .height = SurfaceHeight{720}}};

  [[nodiscard]] static auto configure_window(
      NWindow* window, std::optional<SurfaceSize> docked_cap)
      -> std::expected<void, ProviderError>;

  static auto framebuffer(NWindow* window, AppletOperationMode mode) noexcept
      -> SurfaceSize;

  static auto placement() noexcept -> ThreadPlacementPolicy;

  static void release_window(NWindow*) noexcept {}
};

}
