#include <switch.h>
#include <switch2/memory/heap.h>
#include <vulkan/vulkan.h>

#include <cstdint>
#include <expected>
#include <flat_map>
#include <functional>
#include <print>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace smoke {

class VulkanInstance {
 public:
  [[nodiscard]] static auto version()
      -> std::expected<std::uint32_t, VkResult> {
    std::uint32_t version = 0;
    const VkResult result = vkEnumerateInstanceVersion(&version);
    if (result != VK_SUCCESS) {
      return std::unexpected(result);
    }
    return version;
  }

  [[nodiscard]] static auto create()
      -> std::expected<VulkanInstance, VkResult> {
    const VkApplicationInfo application{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pNext = nullptr,
        .pApplicationName = "ounce smoke",
        .applicationVersion = 1,
        .pEngineName = "arisa",
        .engineVersion = 1,
        .apiVersion = VK_API_VERSION_1_1};
    const VkInstanceCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .pApplicationInfo = &application,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = 0,
        .ppEnabledExtensionNames = nullptr};
    VkInstance instance = VK_NULL_HANDLE;
    const VkResult result = vkCreateInstance(&info, nullptr, &instance);
    if (result != VK_SUCCESS) {
      return std::unexpected(result);
    }
    return VulkanInstance{instance};
  }

  VulkanInstance(const VulkanInstance&) = delete;
  auto operator=(const VulkanInstance&) -> VulkanInstance& = delete;

  VulkanInstance(VulkanInstance&& other) noexcept
      : instance_(std::exchange(other.instance_, VK_NULL_HANDLE)) {}

  auto operator=(VulkanInstance&&) -> VulkanInstance& = delete;

  ~VulkanInstance() {
    if (instance_ != VK_NULL_HANDLE) {
      vkDestroyInstance(instance_, nullptr);
    }
  }

  [[nodiscard]] auto devices() const -> std::uint32_t {
    std::uint32_t count = 0;
    static_cast<void>(vkEnumeratePhysicalDevices(instance_, &count, nullptr));
    return count;
  }

 private:
  explicit VulkanInstance(VkInstance instance) noexcept : instance_(instance) {}

  VkInstance instance_;
};

}

auto main() -> int {
  consoleInit(nullptr);

  std::flat_map<std::string, int> table{{"a", 1}, {"b", 2}};
  std::move_only_function<int()> sum = [owned = std::vector{1, 2, 3}] {
    return owned[0] + owned[1] + owned[2];
  };
  const auto squares = std::views::iota(0, 4) |
                       std::views::transform([](int i) { return i * i; }) |
                       std::ranges::to<std::vector>();
  const auto version = smoke::VulkanInstance::version();
  const auto instance = smoke::VulkanInstance::create();
  std::println("{} {} {} vulkan {:#x} devices {} heap {}/{}", table.size(),
               sum(), squares.size(), version.value_or(0),
               instance.has_value() ? instance->devices() : 0,
               heapGetFreeSize(), heapGetTotalSize());

  consoleExit(nullptr);
  return 0;
}
