#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <span>

namespace arisa::provider::vulkan {

class PresentMode {
 public:
  static auto choose(std::uint32_t interval,
                     std::span<const VkPresentModeKHR> offered) noexcept
      -> VkPresentModeKHR;

  static auto rebuilds(std::uint32_t from, std::uint32_t to) noexcept -> bool;
};

}
