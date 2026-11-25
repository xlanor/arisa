#include <arisa/provider/horizon/ounce/drako_console.hpp>

#include <format>

namespace arisa::provider::horizon::ounce {

auto DrakoConsole::create_gpu(const HorizonSurface& surface, GpuOptions options,
                              DiagnosticsProvider& diagnostics)
    -> std::expected<Gpu, ProviderError> {
  return Deko3dGpu::create(surface, options, diagnostics);
}

auto DrakoConsole::gpu_report(const Gpu& gpu) -> std::string {
  return std::format("drako3d {}", gpu.report());
}

}
