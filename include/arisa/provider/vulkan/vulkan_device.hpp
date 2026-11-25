#pragma once

#include <arisa/provider/error.hpp>
#include <arisa/provider/vulkan/memory_allocator.hpp>
#include <arisa/provider/vulkan/vulkan_instance.hpp>
#include <arisa/support/unique_handle.hpp>

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace arisa::provider::vulkan {

enum class MemoryUse : std::uint8_t {
  device_local,
  upload,
  readback,
};

struct MemoryChoice {
  std::uint32_t index{0};
  VkMemoryPropertyFlags flags{0};

  auto coherent() const noexcept -> bool {
    return (flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0;
  }
};

struct VulkanCapabilities {
  std::uint32_t api_version{0};
  bool rgb565{false};
  bool swizzle{true};
  bool portability{false};
  bool presents{false};
  bool synchronization2{false};
  bool timestamps_everywhere{false};
  std::uint32_t timestamp_bits{0};
  float timestamp_period{0.0F};
  VkMemoryPropertyFlags upload_memory{0};
  bool timeline_semaphores{false};
  bool sampler_ycbcr{false};
  bool multi_planar_nv12{false};
};

struct DeviceRequest {
  VkSurfaceKHR presents_to{VK_NULL_HANDLE};
  std::vector<std::string> extensions{};
  std::vector<std::string> optional_extensions{};
  VkPhysicalDeviceFeatures features{};
  VkPhysicalDeviceVulkan13Features vulkan13{};
};

class VulkanDevice {
 public:
  class Key {
    friend class VulkanDevice;
    Key() = default;
  };

  struct DeviceTraits {
    using handle_type = VkDevice;

    static auto empty() noexcept -> handle_type { return VK_NULL_HANDLE; }

    static auto is_valid(const handle_type& handle) noexcept -> bool {
      return handle != VK_NULL_HANDLE;
    }

    static void release(handle_type& handle) noexcept {
      vkDestroyDevice(handle, nullptr);
    }
  };

  struct Parts {
    std::shared_ptr<const VulkanInstance> instance;
    VkPhysicalDevice physical{VK_NULL_HANDLE};
    support::UniqueHandle<DeviceTraits> device;
    VkQueue queue{VK_NULL_HANDLE};
    std::uint32_t queue_family{0};
    VkPhysicalDeviceProperties properties{};
    VkPhysicalDeviceMemoryProperties memory{};
    std::string driver;
    VulkanCapabilities capabilities;
    PFN_vkCmdPipelineBarrier2 pipeline_barrier2{nullptr};
    std::vector<std::string> extensions{};
    VkPhysicalDeviceFeatures features{};
    VkPhysicalDeviceVulkan13Features vulkan13{};
  };

  [[nodiscard]] static auto create(
      std::shared_ptr<const VulkanInstance> instance,
      DeviceRequest request = {})
      -> std::expected<std::shared_ptr<VulkanDevice>, ProviderError>;

  VulkanDevice(Key key, Parts parts) noexcept;

  VulkanDevice(const VulkanDevice&) = delete;
  auto operator=(const VulkanDevice&) -> VulkanDevice& = delete;
  VulkanDevice(VulkanDevice&&) = delete;
  auto operator=(VulkanDevice&&) -> VulkanDevice& = delete;
  ~VulkanDevice();

  [[nodiscard]] static auto unsupported(
      const std::shared_ptr<const VulkanInstance>& instance,
      const DeviceRequest& request) -> std::vector<std::string>;

  auto handle() const noexcept -> VkDevice { return parts_.device.get(); }

  auto has_extension(std::string_view name) const -> bool;

  auto extensions() const noexcept -> const std::vector<std::string>& {
    return parts_.extensions;
  }

  auto features() const noexcept -> const VkPhysicalDeviceFeatures& {
    return parts_.features;
  }

  auto vulkan13() const noexcept -> const VkPhysicalDeviceVulkan13Features& {
    return parts_.vulkan13;
  }

  auto physical() const noexcept -> VkPhysicalDevice { return parts_.physical; }

  auto queue() const noexcept -> VkQueue { return parts_.queue; }

  auto queue_family() const noexcept -> std::uint32_t {
    return parts_.queue_family;
  }

  auto instance() const noexcept -> const VulkanInstance& {
    return *parts_.instance;
  }

  auto properties() const noexcept -> const VkPhysicalDeviceProperties& {
    return parts_.properties;
  }

  auto capabilities() const noexcept -> const VulkanCapabilities& {
    return parts_.capabilities;
  }

  auto driver() const -> const std::string& { return parts_.driver; }

  auto memory(std::uint32_t allowed, MemoryUse use) const noexcept
      -> std::optional<MemoryChoice>;

  auto supports(VkFormat format, VkFormatFeatureFlags features) const noexcept
      -> bool;

  auto non_coherent_atom() const noexcept -> VkDeviceSize {
    return parts_.properties.limits.nonCoherentAtomSize;
  }

  [[nodiscard]] auto allocate(const VkMemoryRequirements& requirements,
                              MemoryUse use, MemoryTiling tiling) const
      -> std::expected<MemoryAllocator::Allocation, ProviderError>;

  auto memory_statistics() const -> MemoryAllocator::Statistics;

  auto allocations() const -> std::uint64_t;

  auto pipeline_barrier2() const noexcept -> PFN_vkCmdPipelineBarrier2 {
    return parts_.pipeline_barrier2;
  }

  void wait_idle() const noexcept;

  [[nodiscard]] auto submit(std::span<const VkSubmitInfo> batches,
                            VkFence fence = VK_NULL_HANDLE) const
      -> std::expected<void, ProviderError>;

  template <class Use>
  auto with_queue(Use&& use) const -> std::invoke_result_t<Use, VkQueue> {
    const std::scoped_lock lock{queue_mutex_};
    return std::forward<Use>(use)(parts_.queue);
  }

  auto report() const -> std::string;

 private:
  struct Candidate {
    VkPhysicalDevice physical{VK_NULL_HANDLE};
    std::uint32_t family{0};
  };

  [[nodiscard]] static auto choose(VkInstance instance, VkSurfaceKHR surface)
      -> std::optional<Candidate>;

  [[nodiscard]] static auto extension_available(VkPhysicalDevice physical,
                                                const char* name) -> bool;

  static auto preference(VkMemoryPropertyFlags flags, MemoryUse use) noexcept
      -> int;

  static auto version_text(std::uint32_t version) -> std::string;

  [[nodiscard]] static auto synchronization2_available(
      VkPhysicalDevice physical) -> bool;

  static auto missing(VkPhysicalDevice physical, const DeviceRequest& request)
      -> std::vector<std::string>;

  static auto core_flags(const VkPhysicalDeviceFeatures& features)
      -> std::vector<VkBool32>;

  static auto vulkan13_flags(const VkPhysicalDeviceVulkan13Features& features)
      -> std::vector<VkBool32>;

  static void merge(VkPhysicalDeviceVulkan13Features& into,
                    const VkPhysicalDeviceVulkan13Features& wanted);

  static constexpr std::array<std::string_view, 15> vulkan13_names{
      "robustImageAccess",
      "inlineUniformBlock",
      "descriptorBindingInlineUniformBlockUpdateAfterBind",
      "pipelineCreationCacheControl",
      "privateData",
      "shaderDemoteToHelperInvocation",
      "shaderTerminateInvocation",
      "subgroupSizeControl",
      "computeFullSubgroups",
      "synchronization2",
      "textureCompressionASTC_HDR",
      "shaderZeroInitializeWorkgroupMemory",
      "dynamicRendering",
      "shaderIntegerDotProduct",
      "maintenance4"};

  Parts parts_;
  MemoryAllocator allocator_;
  mutable std::mutex queue_mutex_;
};

}
