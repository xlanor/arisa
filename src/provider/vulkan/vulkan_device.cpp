#include <arisa/provider/vulkan/vulkan_device.hpp>
#include <arisa/provider/vulkan/vulkan_status.hpp>

#include <vulkan/vulkan_beta.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <format>
#include <string_view>
#include <utility>
#include <vector>

namespace arisa::provider::vulkan {

VulkanDevice::VulkanDevice(Key, Parts parts) noexcept
    : parts_(std::move(parts)),
      allocator_(parts_.device.get(),
                 parts_.properties.limits.nonCoherentAtomSize) {}

VulkanDevice::~VulkanDevice() { wait_idle(); }

auto VulkanDevice::extension_available(VkPhysicalDevice physical,
                                       const char* name) -> bool {
  std::uint32_t count = 0;
  if (vkEnumerateDeviceExtensionProperties(physical, nullptr, &count,
                                           nullptr) != VK_SUCCESS) {
    return false;
  }
  std::vector<VkExtensionProperties> extensions(count);
  if (vkEnumerateDeviceExtensionProperties(physical, nullptr, &count,
                                           extensions.data()) != VK_SUCCESS) {
    return false;
  }
  return std::ranges::any_of(
      extensions, [name](const VkExtensionProperties& extension) {
        return std::strcmp(extension.extensionName, name) == 0;
      });
}

auto VulkanDevice::synchronization2_available(VkPhysicalDevice physical)
    -> bool {
  VkPhysicalDeviceProperties properties{};
  vkGetPhysicalDeviceProperties(physical, &properties);
  if (properties.apiVersion < VK_API_VERSION_1_3) {
    return false;
  }
  VkPhysicalDeviceVulkan13Features vulkan13{};
  vulkan13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
  VkPhysicalDeviceFeatures2 features{};
  features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  features.pNext = &vulkan13;
  vkGetPhysicalDeviceFeatures2(physical, &features);
  return vulkan13.synchronization2 == VK_TRUE;
}

auto VulkanDevice::core_flags(const VkPhysicalDeviceFeatures& features)
    -> std::vector<VkBool32> {
  std::vector<VkBool32> flags(sizeof(VkPhysicalDeviceFeatures) /
                              sizeof(VkBool32));
  std::memcpy(flags.data(), &features, flags.size() * sizeof(VkBool32));
  return flags;
}

auto VulkanDevice::vulkan13_flags(
    const VkPhysicalDeviceVulkan13Features& features) -> std::vector<VkBool32> {
  std::vector<VkBool32> flags(vulkan13_names.size());
  std::memcpy(flags.data(), &features.robustImageAccess,
              flags.size() * sizeof(VkBool32));
  return flags;
}

void VulkanDevice::merge(VkPhysicalDeviceVulkan13Features& into,
                         const VkPhysicalDeviceVulkan13Features& wanted) {
  std::vector<VkBool32> merged = vulkan13_flags(into);
  const std::vector<VkBool32> asked = vulkan13_flags(wanted);
  for (std::size_t index = 0; index < merged.size(); ++index) {
    if (asked[index] == VK_TRUE) {
      merged[index] = VK_TRUE;
    }
  }
  std::memcpy(&into.robustImageAccess, merged.data(),
              merged.size() * sizeof(VkBool32));
}

auto VulkanDevice::missing(VkPhysicalDevice physical,
                           const DeviceRequest& request)
    -> std::vector<std::string> {
  std::vector<std::string> lacking;
  for (const std::string& name : request.extensions) {
    if (!extension_available(physical, name.c_str())) {
      lacking.push_back(name);
    }
  }
  VkPhysicalDeviceFeatures core{};
  vkGetPhysicalDeviceFeatures(physical, &core);
  const std::vector<VkBool32> offered = core_flags(core);
  const std::vector<VkBool32> asked = core_flags(request.features);
  for (std::size_t index = 0; index < asked.size(); ++index) {
    if (asked[index] == VK_TRUE && offered[index] != VK_TRUE) {
      lacking.push_back(
          std::format("VkPhysicalDeviceFeatures field {}", index));
    }
  }
  const std::vector<VkBool32> wanted = vulkan13_flags(request.vulkan13);
  if (std::ranges::none_of(wanted,
                           [](VkBool32 flag) { return flag == VK_TRUE; })) {
    return lacking;
  }
  VkPhysicalDeviceProperties properties{};
  vkGetPhysicalDeviceProperties(physical, &properties);
  VkPhysicalDeviceVulkan13Features vulkan13{};
  vulkan13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
  if (properties.apiVersion >= VK_API_VERSION_1_3) {
    VkPhysicalDeviceFeatures2 features{};
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features.pNext = &vulkan13;
    vkGetPhysicalDeviceFeatures2(physical, &features);
  }
  const std::vector<VkBool32> available = vulkan13_flags(vulkan13);
  for (std::size_t index = 0; index < wanted.size(); ++index) {
    if (wanted[index] == VK_TRUE && available[index] != VK_TRUE) {
      lacking.push_back(
          properties.apiVersion >= VK_API_VERSION_1_3
              ? std::string{vulkan13_names[index]}
              : std::format("{} (needs Vulkan 1.3, the device has {})",
                            vulkan13_names[index],
                            version_text(properties.apiVersion)));
    }
  }
  return lacking;
}

auto VulkanDevice::unsupported(
    const std::shared_ptr<const VulkanInstance>& instance,
    const DeviceRequest& request) -> std::vector<std::string> {
  if (instance == nullptr) {
    return {"no Vulkan instance"};
  }
  const std::optional<Candidate> candidate =
      choose(instance->handle(), request.presents_to);
  if (!candidate.has_value()) {
    return {"no Vulkan device that can draw and present"};
  }
  return missing(candidate->physical, request);
}

auto VulkanDevice::has_extension(std::string_view name) const -> bool {
  return std::ranges::find(parts_.extensions, name) != parts_.extensions.end();
}

auto VulkanDevice::choose(VkInstance instance, VkSurfaceKHR surface)
    -> std::optional<Candidate> {
  std::uint32_t count = 0;
  if (vkEnumeratePhysicalDevices(instance, &count, nullptr) != VK_SUCCESS ||
      count == 0) {
    return std::nullopt;
  }
  std::vector<VkPhysicalDevice> devices(count);
  if (vkEnumeratePhysicalDevices(instance, &count, devices.data()) !=
      VK_SUCCESS) {
    return std::nullopt;
  }
  const auto rank = [](VkPhysicalDeviceType type) {
    switch (type) {
      case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
      case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        return 0;
      case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
        return 1;
      default:
        return 2;
    }
  };
  std::optional<Candidate> best;
  int best_rank = 3;
  for (VkPhysicalDevice physical : devices) {
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(physical, &properties);
    if (properties.apiVersion < VulkanInstance::minimum_version ||
        (surface != VK_NULL_HANDLE &&
         !extension_available(physical, VK_KHR_SWAPCHAIN_EXTENSION_NAME))) {
      continue;
    }
    std::uint32_t families = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical, &families, nullptr);
    std::vector<VkQueueFamilyProperties> family_properties(families);
    vkGetPhysicalDeviceQueueFamilyProperties(physical, &families,
                                             family_properties.data());
    for (std::uint32_t family = 0; family < families; ++family) {
      if ((family_properties[family].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) {
        continue;
      }
      if (surface != VK_NULL_HANDLE) {
        VkBool32 presents = VK_FALSE;
        if (vkGetPhysicalDeviceSurfaceSupportKHR(physical, family, surface,
                                                 &presents) != VK_SUCCESS ||
            presents != VK_TRUE) {
          continue;
        }
      }
      const int ranked = rank(properties.deviceType);
      if (ranked < best_rank) {
        best = Candidate{.physical = physical, .family = family};
        best_rank = ranked;
      }
      break;
    }
  }
  return best;
}

auto VulkanDevice::create(std::shared_ptr<const VulkanInstance> instance,
                          DeviceRequest request)
    -> std::expected<std::shared_ptr<VulkanDevice>, ProviderError> {
  if (instance == nullptr) {
    return std::unexpected(ProviderError{ProviderErrorCode::invalid_argument});
  }
  const std::optional<Candidate> candidate =
      choose(instance->handle(), request.presents_to);
  if (!candidate.has_value()) {
    return std::unexpected(ProviderError::unsupported_capability());
  }
  VkPhysicalDevice const physical = candidate->physical;
  if (const std::vector<std::string> lacking = missing(physical, request);
      !lacking.empty()) {
    const bool extension =
        std::ranges::any_of(lacking, [&request](const std::string& name) {
          return std::ranges::find(request.extensions, name) !=
                 request.extensions.end();
        });
    return std::unexpected(
        ProviderError{ProviderErrorCode::unsupported_capability,
                      extension ? ProviderErrorDetail::missing_extension
                                : ProviderErrorDetail::missing_feature});
  }
  std::vector<const char*> extensions;
  const auto enable = [&extensions](const char* name) {
    if (std::ranges::none_of(extensions, [name](const char* present) {
          return std::strcmp(present, name) == 0;
        })) {
      extensions.push_back(name);
    }
  };
  const bool presents = request.presents_to != VK_NULL_HANDLE;
  if (presents) {
    enable(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
  }
  for (const std::string& name : request.extensions) {
    enable(name.c_str());
  }
  for (const std::string& name : request.optional_extensions) {
    if (extension_available(physical, name.c_str())) {
      enable(name.c_str());
    }
  }
  VkPhysicalDevicePortabilitySubsetFeaturesKHR portability{};
  portability.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PORTABILITY_SUBSET_FEATURES_KHR;
  const bool portable =
      extension_available(physical, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
  if (portable) {
    enable(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
    VkPhysicalDeviceFeatures2 features{};
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features.pNext = &portability;
    vkGetPhysicalDeviceFeatures2(physical, &features);
    portability.pNext = nullptr;
  }
  const bool synchronization2 = synchronization2_available(physical);
  VkPhysicalDeviceVulkan13Features vulkan13{};
  vulkan13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
  vulkan13.synchronization2 = synchronization2 ? VK_TRUE : VK_FALSE;
  merge(vulkan13, request.vulkan13);
  const bool vulkan13_used = std::ranges::any_of(
      vulkan13_flags(vulkan13), [](VkBool32 flag) { return flag == VK_TRUE; });
  VkPhysicalDeviceProperties described{};
  vkGetPhysicalDeviceProperties(physical, &described);
  const bool timeline_core = described.apiVersion >= VK_API_VERSION_1_2;
  const bool timeline_extension =
      !timeline_core &&
      extension_available(physical, VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME);
  VkPhysicalDeviceTimelineSemaphoreFeatures timeline{};
  timeline.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
  VkPhysicalDeviceSamplerYcbcrConversionFeatures ycbcr{};
  ycbcr.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES;
  if (timeline_core || timeline_extension) {
    ycbcr.pNext = &timeline;
  }
  VkPhysicalDeviceFeatures2 offered{};
  offered.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  offered.pNext = &ycbcr;
  vkGetPhysicalDeviceFeatures2(physical, &offered);
  const bool timelines = timeline.timelineSemaphore == VK_TRUE;
  const bool conversions = ycbcr.samplerYcbcrConversion == VK_TRUE;
  if (timelines && timeline_extension) {
    enable(VK_KHR_TIMELINE_SEMAPHORE_EXTENSION_NAME);
  }
  void* chain = nullptr;
  if (vulkan13_used) {
    chain = &vulkan13;
  }
  VkPhysicalDeviceSamplerYcbcrConversionFeatures ycbcr_enabled{};
  ycbcr_enabled.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SAMPLER_YCBCR_CONVERSION_FEATURES;
  ycbcr_enabled.samplerYcbcrConversion = VK_TRUE;
  if (conversions) {
    ycbcr_enabled.pNext = chain;
    chain = &ycbcr_enabled;
  }
  VkPhysicalDeviceTimelineSemaphoreFeatures timeline_enabled{};
  timeline_enabled.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_TIMELINE_SEMAPHORE_FEATURES;
  timeline_enabled.timelineSemaphore = VK_TRUE;
  if (timelines) {
    timeline_enabled.pNext = chain;
    chain = &timeline_enabled;
  }
  if (portable) {
    portability.pNext = chain;
    chain = &portability;
  }
  const float priority = 1.0F;
  const VkDeviceQueueCreateInfo queue_info{
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0,
      .queueFamilyIndex = candidate->family,
      .queueCount = 1,
      .pQueuePriorities = &priority};
  const VkDeviceCreateInfo info{
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .pNext = chain,
      .flags = 0,
      .queueCreateInfoCount = 1,
      .pQueueCreateInfos = &queue_info,
      .enabledLayerCount = 0,
      .ppEnabledLayerNames = nullptr,
      .enabledExtensionCount = static_cast<std::uint32_t>(extensions.size()),
      .ppEnabledExtensionNames = extensions.data(),
      .pEnabledFeatures = &request.features};
  VkDevice created = VK_NULL_HANDLE;
  if (const VkResult result =
          vkCreateDevice(physical, &info, nullptr, &created);
      result != VK_SUCCESS) {
    return std::unexpected(VulkanStatus::error(result));
  }
  Parts parts{.instance = std::move(instance),
              .physical = physical,
              .device = support::UniqueHandle<DeviceTraits>{created},
              .queue = VK_NULL_HANDLE,
              .queue_family = candidate->family,
              .properties = {},
              .memory = {},
              .driver = {},
              .capabilities = {},
              .pipeline_barrier2 = nullptr,
              .extensions = {extensions.begin(), extensions.end()},
              .features = request.features,
              .vulkan13 = vulkan13};
  parts.vulkan13.pNext = nullptr;
  vkGetDeviceQueue(created, candidate->family, 0, &parts.queue);
  if (synchronization2) {
    parts.pipeline_barrier2 = reinterpret_cast<PFN_vkCmdPipelineBarrier2>(
        vkGetDeviceProcAddr(created, "vkCmdPipelineBarrier2"));
  }
  std::uint32_t families = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(physical, &families, nullptr);
  std::vector<VkQueueFamilyProperties> family_properties(families);
  vkGetPhysicalDeviceQueueFamilyProperties(physical, &families,
                                           family_properties.data());
  const std::uint32_t timestamp_bits =
      candidate->family < families
          ? family_properties[candidate->family].timestampValidBits
          : 0;
  vkGetPhysicalDeviceProperties(physical, &parts.properties);
  vkGetPhysicalDeviceMemoryProperties(physical, &parts.memory);
  if (parts.properties.apiVersion >= VK_API_VERSION_1_2) {
    VkPhysicalDeviceDriverProperties driver{};
    driver.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES;
    VkPhysicalDeviceProperties2 properties{};
    properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    properties.pNext = &driver;
    vkGetPhysicalDeviceProperties2(physical, &properties);
    parts.driver = std::format("{} {}", std::string_view{driver.driverName},
                               std::string_view{driver.driverInfo});
  } else {
    parts.driver = version_text(parts.properties.driverVersion);
  }
  parts.capabilities = VulkanCapabilities{
      .api_version = parts.properties.apiVersion,
      .rgb565 = false,
      .swizzle = !portable || portability.imageViewFormatSwizzle == VK_TRUE,
      .portability = portable,
      .presents = presents,
      .synchronization2 = parts.pipeline_barrier2 != nullptr,
      .timestamps_everywhere =
          parts.properties.limits.timestampComputeAndGraphics == VK_TRUE,
      .timestamp_bits = timestamp_bits,
      .timestamp_period = parts.properties.limits.timestampPeriod,
      .upload_memory = 0,
      .timeline_semaphores = timelines,
      .sampler_ycbcr = conversions,
      .multi_planar_nv12 = false};
  auto device = std::make_shared<VulkanDevice>(Key{}, std::move(parts));
  device->parts_.capabilities.rgb565 =
      device->supports(VK_FORMAT_R5G6B5_UNORM_PACK16,
                       VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT |
                           VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT |
                           VK_FORMAT_FEATURE_TRANSFER_DST_BIT);
  device->parts_.capabilities.multi_planar_nv12 =
      conversions && device->supports(VK_FORMAT_G8_B8R8_2PLANE_420_UNORM,
                                      VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);
  const std::optional<MemoryChoice> upload =
      device->memory(~std::uint32_t{0}, MemoryUse::upload);
  if (!upload.has_value() ||
      !device->memory(~std::uint32_t{0}, MemoryUse::readback).has_value()) {
    return std::unexpected(ProviderError::unsupported_capability());
  }
  device->parts_.capabilities.upload_memory = upload->flags;
  return device;
}

auto VulkanDevice::preference(VkMemoryPropertyFlags flags,
                              MemoryUse use) noexcept -> int {
  const bool local = (flags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0;
  const bool visible = (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;
  const bool coherent = (flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;
  const bool cached = (flags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) != 0;
  const bool lazy = (flags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) != 0;
  if (lazy) {
    return -1;
  }
  switch (use) {
    case MemoryUse::device_local:
      return (local ? 4 : 0) + (visible ? 0 : 1);
    case MemoryUse::upload:
      if (!visible) {
        return -1;
      }
      return (coherent ? 4 : 0) + (local ? 2 : 0) + (cached ? 0 : 1);
    case MemoryUse::readback:
      if (!visible) {
        return -1;
      }
      return (cached ? 4 : 0) + (coherent ? 2 : 0);
  }
  return -1;
}

auto VulkanDevice::memory(std::uint32_t allowed, MemoryUse use) const noexcept
    -> std::optional<MemoryChoice> {
  std::optional<MemoryChoice> best;
  int best_score = -1;
  for (std::uint32_t index = 0; index < parts_.memory.memoryTypeCount;
       ++index) {
    if ((allowed & (1U << index)) == 0) {
      continue;
    }
    const VkMemoryPropertyFlags flags =
        parts_.memory.memoryTypes[index].propertyFlags;
    const int score = preference(flags, use);
    if (score > best_score) {
      best_score = score;
      best = MemoryChoice{.index = index, .flags = flags};
    }
  }
  return best;
}

auto VulkanDevice::supports(VkFormat format,
                            VkFormatFeatureFlags features) const noexcept
    -> bool {
  VkFormatProperties properties{};
  vkGetPhysicalDeviceFormatProperties(parts_.physical, format, &properties);
  return (properties.optimalTilingFeatures & features) == features;
}

auto VulkanDevice::allocate(const VkMemoryRequirements& requirements,
                            MemoryUse use, MemoryTiling tiling) const
    -> std::expected<MemoryAllocator::Allocation, ProviderError> {
  const std::optional<MemoryChoice> choice =
      memory(requirements.memoryTypeBits, use);
  if (!choice.has_value()) {
    return std::unexpected(ProviderError::unsupported_capability());
  }
  return allocator_.allocate(
      MemoryAllocator::Request{.requirements = requirements,
                               .type = choice->index,
                               .flags = choice->flags,
                               .tiling = tiling});
}

auto VulkanDevice::memory_statistics() const -> MemoryAllocator::Statistics {
  return allocator_.statistics();
}

auto VulkanDevice::allocations() const -> std::uint64_t {
  return allocator_.statistics().blocks_created;
}

void VulkanDevice::wait_idle() const noexcept {
  if (parts_.device.valid()) {
    const std::scoped_lock lock{queue_mutex_};
    static_cast<void>(vkDeviceWaitIdle(parts_.device.get()));
  }
}

auto VulkanDevice::submit(std::span<const VkSubmitInfo> batches,
                          VkFence fence) const
    -> std::expected<void, ProviderError> {
  const VkResult result = with_queue([&](VkQueue queue) {
    return vkQueueSubmit(queue, static_cast<std::uint32_t>(batches.size()),
                         batches.data(), fence);
  });
  if (result != VK_SUCCESS) {
    return std::unexpected(VulkanStatus::error(result));
  }
  return {};
}

auto VulkanDevice::version_text(std::uint32_t version) -> std::string {
  return std::format("{}.{}.{}", VK_API_VERSION_MAJOR(version),
                     VK_API_VERSION_MINOR(version),
                     VK_API_VERSION_PATCH(version));
}

auto VulkanDevice::report() const -> std::string {
  const VulkanCapabilities& capabilities = parts_.capabilities;
  const VkMemoryPropertyFlags upload = capabilities.upload_memory;
  const auto flag = [upload](VkMemoryPropertyFlags bit, std::string_view name) {
    return (upload & bit) != 0 ? std::format(" {}", name) : std::string{};
  };
  const std::string timestamps =
      capabilities.timestamp_bits == 0
          ? std::string{"none"}
          : std::format("{} bits at {} ns{}", capabilities.timestamp_bits,
                        capabilities.timestamp_period,
                        capabilities.timestamps_everywhere
                            ? ""
                            : ", not on every queue");
  return std::format(
      "{}, Vulkan {}, driver {}; upload memory{}{}{}{}; rgb565 {}; alpha8 "
      "{}; portability subset {}; presents {}; synchronization2 {}; "
      "timeline semaphores {}; multi-planar nv12 {}; "
      "timestamps {}; {} memory blocks for {} resources of {} allowed",
      std::string_view{parts_.properties.deviceName},
      version_text(parts_.properties.apiVersion), parts_.driver,
      flag(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "device-local"),
      flag(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, "host-visible"),
      flag(VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, "coherent"),
      flag(VK_MEMORY_PROPERTY_HOST_CACHED_BIT, "cached"),
      capabilities.rgb565 ? "native" : "converted to rgba8",
      capabilities.swizzle ? "swizzled r8" : "converted to rgba8",
      capabilities.portability ? "yes" : "no",
      capabilities.presents ? "yes" : "no",
      capabilities.synchronization2 ? "yes" : "no",
      capabilities.timeline_semaphores ? "yes" : "no",
      capabilities.multi_planar_nv12 ? "yes" : "no", timestamps,
      memory_statistics().blocks_live, memory_statistics().allocations_live,
      parts_.properties.limits.maxMemoryAllocationCount);
}

}
