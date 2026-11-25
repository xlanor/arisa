#pragma once

#include <arisa/provider/diagnostics/provider.hpp>
#include <arisa/provider/error.hpp>
#include <arisa/provider/horizon/gpu_options.hpp>
#include <arisa/provider/horizon/horizon_surface.hpp>
#include <arisa/provider/horizon/ounce/ounce_hardware.hpp>
#include <arisa/provider/vulkan/vulkan_device.hpp>
#include <arisa/provider/vulkan/vulkan_gpu.hpp>
#include <arisa/provider/vulkan/vulkan_instance.hpp>
#include <arisa/provider/vulkan/vulkan_surface.hpp>

#include <switch.h>

#include <expected>
#include <memory>
#include <string>
#include <string_view>

namespace arisa::provider::horizon::ounce {

class OunceConsole : public OunceHardware {
 public:
  using Gpu = vulkan::VulkanGpu;

  static constexpr std::string_view provider_name = "arisa horizon ounce";

  [[nodiscard]] static auto create_gpu(const HorizonSurface& surface,
                                       GpuOptions options,
                                       DiagnosticsProvider& diagnostics)
      -> std::expected<Gpu, ProviderError>;

  static auto gpu_report(const Gpu& gpu) -> std::string;

 private:
  static auto device_request(const GpuOptions& options, VkSurfaceKHR surface)
      -> vulkan::DeviceRequest;

  [[nodiscard]] static auto vi_surface(
      std::shared_ptr<const vulkan::VulkanInstance> instance, NWindow* window)
      -> std::expected<vulkan::VulkanSurface, ProviderError>;
};

}
