#include <arisa/provider/horizon/nx/nx_console.hpp>

namespace arisa::provider::horizon::nx {

auto NxConsole::create_gpu(const HorizonSurface& surface, GpuOptions options,
                           DiagnosticsProvider& diagnostics)
    -> std::expected<Gpu, ProviderError> {
  return Deko3dGpu::create(surface, options, diagnostics);
}

auto NxConsole::gpu_report(const Gpu& gpu) -> std::string {
  return gpu.report();
}

auto NxConsole::placement() noexcept -> ThreadPlacementPolicy { return {}; }

auto NxConsole::configure_window(NWindow*, std::optional<SurfaceSize>)
    -> std::expected<void, ProviderError> {
  return {};
}

auto NxConsole::framebuffer(NWindow*, AppletOperationMode mode) noexcept
    -> SurfaceSize {
  return mode == AppletOperationMode_Console ? docked_size : handheld_size;
}

}
