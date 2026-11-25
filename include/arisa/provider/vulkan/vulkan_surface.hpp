#pragma once

#include <arisa/provider/vulkan/vulkan_instance.hpp>
#include <arisa/support/unique_handle.hpp>

#include <vulkan/vulkan.h>

#include <memory>

namespace arisa::provider::vulkan {

class VulkanSurface {
 public:
  VulkanSurface(std::shared_ptr<const VulkanInstance> instance,
                VkSurfaceKHR surface) noexcept;

  auto handle() const noexcept -> VkSurfaceKHR {
    return surface_.get().surface;
  }

 private:
  struct SurfaceTraits {
    struct handle_type {
      VkInstance instance{VK_NULL_HANDLE};
      VkSurfaceKHR surface{VK_NULL_HANDLE};
    };

    static auto empty() noexcept -> handle_type { return handle_type{}; }

    static auto is_valid(const handle_type& handle) noexcept -> bool {
      return handle.surface != VK_NULL_HANDLE;
    }

    static void release(handle_type& handle) noexcept {
      vkDestroySurfaceKHR(handle.instance, handle.surface, nullptr);
    }
  };

  std::shared_ptr<const VulkanInstance> instance_;
  support::UniqueHandle<SurfaceTraits> surface_;
};

}
