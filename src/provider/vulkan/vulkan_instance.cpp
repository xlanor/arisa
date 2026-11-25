#include <arisa/provider/vulkan/vulkan_instance.hpp>
#include <arisa/provider/vulkan/vulkan_status.hpp>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

namespace arisa::provider::vulkan {

VulkanInstance::VulkanInstance(VkInstance instance, bool portable) noexcept
    : instance_(instance), portable_(portable) {}

auto VulkanInstance::available(const char* name) -> bool {
  std::uint32_t count = 0;
  if (vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr) !=
      VK_SUCCESS) {
    return false;
  }
  std::vector<VkExtensionProperties> extensions(count);
  if (vkEnumerateInstanceExtensionProperties(nullptr, &count,
                                             extensions.data()) != VK_SUCCESS) {
    return false;
  }
  return std::ranges::any_of(
      extensions, [name](const VkExtensionProperties& extension) {
        return std::strcmp(extension.extensionName, name) == 0;
      });
}

auto VulkanInstance::create(std::span<const char* const> required_extensions)
    -> std::expected<VulkanInstance, ProviderError> {
  std::vector<const char*> extensions;
  for (const char* const name : required_extensions) {
    if (!available(name)) {
      return std::unexpected(ProviderError::unsupported_capability());
    }
    extensions.push_back(name);
  }
  const bool portable =
      available(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
  if (portable) {
    extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
  }
  const VkApplicationInfo application{
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pNext = nullptr,
      .pApplicationName = "arisa",
      .applicationVersion = 1,
      .pEngineName = "arisa",
      .engineVersion = 1,
      .apiVersion = requested_version};
  const VkInstanceCreateInfo info{
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pNext = nullptr,
      .flags =
          portable
              ? VkInstanceCreateFlags{VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR}
              : VkInstanceCreateFlags{0},
      .pApplicationInfo = &application,
      .enabledLayerCount = 0,
      .ppEnabledLayerNames = nullptr,
      .enabledExtensionCount = static_cast<std::uint32_t>(extensions.size()),
      .ppEnabledExtensionNames = extensions.data()};
  VkInstance instance = VK_NULL_HANDLE;
  if (const VkResult result = vkCreateInstance(&info, nullptr, &instance);
      result != VK_SUCCESS) {
    return std::unexpected(VulkanStatus::error(result));
  }
  return VulkanInstance{instance, portable};
}

}
