#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <optional>

namespace arisa::provider::vulkan {

struct ReadySemaphore {
  VkSemaphore semaphore{VK_NULL_HANDLE};
  std::optional<std::uint64_t> value{};

  auto waits() const noexcept -> bool { return semaphore != VK_NULL_HANDLE; }

  auto timeline() const noexcept -> bool { return value.has_value(); }

  friend auto operator==(const ReadySemaphore&, const ReadySemaphore&)
      -> bool = default;
};

}
