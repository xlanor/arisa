#pragma once

#include <arisa/provider/diagnostics/provider.hpp>
#include <arisa/provider/error.hpp>
#include <arisa/provider/horizon/deko3d_gpu.hpp>
#include <arisa/provider/horizon/gpu_options.hpp>
#include <arisa/provider/horizon/horizon_surface.hpp>
#include <arisa/provider/horizon/nx/nx_pointer.hpp>
#include <arisa/provider/horizon/nx/nx_rumble.hpp>
#include <arisa/provider/input/touch_panel.hpp>
#include <arisa/provider/surface/surface_size.hpp>
#include <arisa/provider/threading/thread_placement_policy.hpp>

#include <switch.h>

#include <expected>
#include <optional>
#include <string>
#include <string_view>

namespace arisa::provider::horizon::nx {

class NxConsole {
 public:
  using Gpu = Deko3dGpu;
  using Rumble = NxRumble;
  using Pointer = NxPointer;

  static constexpr std::string_view provider_name = "arisa horizon";

  static constexpr SurfaceSize handheld_size{.width = SurfaceWidth{1280},
                                             .height = SurfaceHeight{720}};
  static constexpr SurfaceSize docked_size{.width = SurfaceWidth{1920},
                                           .height = SurfaceHeight{1080}};
  static constexpr TouchPanel touch_panel{handheld_size};

  [[nodiscard]] static auto configure_window(
      NWindow* window, std::optional<SurfaceSize> docked_cap)
      -> std::expected<void, ProviderError>;

  static auto framebuffer(NWindow* window, AppletOperationMode mode) noexcept
      -> SurfaceSize;

  [[nodiscard]] static auto create_gpu(const HorizonSurface& surface,
                                       GpuOptions options,
                                       DiagnosticsProvider& diagnostics)
      -> std::expected<Gpu, ProviderError>;

  static auto gpu_report(const Gpu& gpu) -> std::string;

  static auto placement() noexcept -> ThreadPlacementPolicy;

  static void release_window(NWindow*) noexcept {}
};

}
