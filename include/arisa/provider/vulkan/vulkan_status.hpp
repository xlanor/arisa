#pragma once

#include <arisa/provider/error.hpp>

#include <vulkan/vulkan.h>

#include <cstdint>
#include <expected>

namespace arisa::provider::vulkan {

class VulkanStatus {
 public:
  static auto error(VkResult result) noexcept -> ProviderError {
    if (result == VK_ERROR_DEVICE_LOST) {
      return ProviderError{ProviderErrorCode::platform_failure,
                           ProviderErrorDetail::device_lost};
    }
    const bool exhausted = result == VK_ERROR_OUT_OF_HOST_MEMORY ||
                           result == VK_ERROR_OUT_OF_DEVICE_MEMORY ||
                           result == VK_ERROR_TOO_MANY_OBJECTS ||
                           result == VK_ERROR_FRAGMENTED_POOL ||
                           result == VK_ERROR_OUT_OF_POOL_MEMORY;
    return ProviderError::vulkan(exhausted
                                     ? ProviderErrorCode::resource_exhausted
                                     : ProviderErrorCode::platform_failure,
                                 static_cast<std::int32_t>(result));
  }

  [[nodiscard]] static auto check(VkResult result) noexcept
      -> std::expected<void, ProviderError> {
    if (result != VK_SUCCESS) {
      return std::unexpected(error(result));
    }
    return {};
  }
};

}
