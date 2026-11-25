#include <arisa/provider/horizon/ounce/ounce_console.hpp>

#include <arisa/provider/vulkan/swapchain_target.hpp>
#include <arisa/provider/vulkan/vulkan_device.hpp>
#include <arisa/provider/vulkan/vulkan_status.hpp>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_vi.h>

#include <array>
#include <cstdint>
#include <format>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace arisa::provider::horizon::ounce {

auto OunceConsole::vi_surface(
    std::shared_ptr<const vulkan::VulkanInstance> instance, NWindow* window)
    -> std::expected<vulkan::VulkanSurface, ProviderError> {
  if (instance == nullptr || window == nullptr || !nwindowIsValid(window)) {
    return std::unexpected(ProviderError{ProviderErrorCode::platform_failure});
  }
  const VkViSurfaceCreateInfoNN info{
      .sType = VK_STRUCTURE_TYPE_VI_SURFACE_CREATE_INFO_NN,
      .pNext = nullptr,
      .flags = 0,
      .window = window};
  VkSurfaceKHR surface = VK_NULL_HANDLE;
  if (const VkResult result =
          vkCreateViSurfaceNN(instance->handle(), &info, nullptr, &surface);
      result != VK_SUCCESS) {
    return std::unexpected(vulkan::VulkanStatus::error(result));
  }
  return vulkan::VulkanSurface{std::move(instance), surface};
}

auto OunceConsole::device_request(const GpuOptions& options,
                                  VkSurfaceKHR surface)
    -> vulkan::DeviceRequest {
  vulkan::DeviceRequest request{
      .presents_to = surface,
      .extensions = options.vulkan.extensions,
      .optional_extensions = options.vulkan.optional_extensions,
      .features = {},
      .vulkan13 = {}};
  for (const VulkanFeature feature : options.vulkan.features) {
    switch (feature) {
      case VulkanFeature::dynamic_rendering:
        request.vulkan13.dynamicRendering = VK_TRUE;
        break;
      case VulkanFeature::synchronization2:
        request.vulkan13.synchronization2 = VK_TRUE;
        break;
      case VulkanFeature::maintenance4:
        request.vulkan13.maintenance4 = VK_TRUE;
        break;
    }
  }
  return request;
}

auto OunceConsole::create_gpu(const HorizonSurface& surface, GpuOptions options,
                              DiagnosticsProvider& diagnostics)
    -> std::expected<Gpu, ProviderError> {
  static constexpr std::array<const char*, 2> extensions{
      VK_KHR_SURFACE_EXTENSION_NAME, VK_NN_VI_SURFACE_EXTENSION_NAME};
  auto created = vulkan::VulkanInstance::create(extensions);
  if (!created.has_value()) {
    return std::unexpected(created.error());
  }
  auto instance =
      std::make_shared<const vulkan::VulkanInstance>(std::move(*created));
  auto window = vi_surface(instance, surface.window());
  if (!window.has_value()) {
    return std::unexpected(window.error());
  }
  const vulkan::DeviceRequest request =
      device_request(options, window->handle());
  auto device = vulkan::VulkanDevice::create(instance, request);
  if (!device.has_value()) {
    if (const std::vector<std::string> lacking =
            vulkan::VulkanDevice::unsupported(instance, request);
        !lacking.empty()) {
      std::string named;
      for (const std::string& each : lacking) {
        named += named.empty() ? each : std::format(", {}", each);
      }
      diagnostics.report(Diagnostic{
          .severity = Severity::error,
          .category = "arisa.vulkan",
          .message = std::format("the Vulkan device lacks what the app asked "
                                 "for: {}",
                                 named)});
    }
    return std::unexpected(device.error());
  }
  auto target = vulkan::SwapchainTarget::create(
      *device, std::move(*window), surface,
      vulkan::SwapchainTarget::Options{
          .images = options.image_count(),
          .interval = options.swap_interval.value(),
          .pace = [window = surface.window()](std::uint32_t interval) {
            static_cast<void>(nwindowSetSwapInterval(window, interval));
          }});
  if (!target.has_value()) {
    return std::unexpected(target.error());
  }
  return Gpu::create(
      std::move(*device),
      std::make_unique<vulkan::SwapchainTarget>(std::move(*target)),
      std::ref(diagnostics));
}

auto OunceConsole::gpu_report(const Gpu& gpu) -> std::string {
  return std::format("vulkan: {}; {} frames, {} missed vsyncs",
                     gpu.device().report(), gpu.frames_presented(),
                     gpu.missed_vsyncs());
}

}
