#pragma once

#include <arisa/provider/diagnostics/provider.hpp>
#include <arisa/provider/error.hpp>
#include <arisa/provider/horizon/deko3d_gpu.hpp>
#include <arisa/provider/horizon/gpu_options.hpp>
#include <arisa/provider/horizon/horizon_surface.hpp>
#include <arisa/provider/horizon/ounce/ounce_hardware.hpp>

#include <expected>
#include <string>
#include <string_view>

namespace arisa::provider::horizon::ounce {

class DrakoConsole : public OunceHardware {
 public:
  using Gpu = Deko3dGpu;

  static constexpr std::string_view provider_name =
      "arisa horizon ounce drako3d";

  [[nodiscard]] static auto create_gpu(const HorizonSurface& surface,
                                       GpuOptions options,
                                       DiagnosticsProvider& diagnostics)
      -> std::expected<Gpu, ProviderError>;

  static auto gpu_report(const Gpu& gpu) -> std::string;
};

}
