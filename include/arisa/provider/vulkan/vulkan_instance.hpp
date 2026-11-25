#pragma once

#include <arisa/provider/error.hpp>
#include <arisa/support/unique_handle.hpp>

#include <vulkan/vulkan.h>

#include <cstdint>
#include <expected>
#include <span>

namespace arisa::provider::vulkan {

class VulkanInstance {
 public:
  static constexpr std::uint32_t requested_version = VK_API_VERSION_1_3;
  static constexpr std::uint32_t minimum_version = VK_API_VERSION_1_1;

  [[nodiscard]] static auto create(
      std::span<const char* const> required_extensions = {})
      -> std::expected<VulkanInstance, ProviderError>;

  auto handle() const noexcept -> VkInstance { return instance_.get(); }

  auto portable() const noexcept -> bool { return portable_; }

 private:
  struct InstanceTraits {
    using handle_type = VkInstance;

    static auto empty() noexcept -> handle_type { return VK_NULL_HANDLE; }

    static auto is_valid(const handle_type& handle) noexcept -> bool {
      return handle != VK_NULL_HANDLE;
    }

    static void release(handle_type& handle) noexcept {
      vkDestroyInstance(handle, nullptr);
    }
  };

  VulkanInstance(VkInstance instance, bool portable) noexcept;

  [[nodiscard]] static auto available(const char* name) -> bool;

  support::UniqueHandle<InstanceTraits> instance_;
  bool portable_;
};

}
