#include <arisa/provider/vulkan/present_mode.hpp>

#include <algorithm>

namespace arisa::provider::vulkan {

auto PresentMode::choose(std::uint32_t interval,
                         std::span<const VkPresentModeKHR> offered) noexcept
    -> VkPresentModeKHR {
  if (interval != 0) {
    return VK_PRESENT_MODE_FIFO_KHR;
  }
  for (const VkPresentModeKHR preferred :
       {VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR}) {
    if (std::ranges::contains(offered, preferred)) {
      return preferred;
    }
  }
  return VK_PRESENT_MODE_FIFO_KHR;
}

auto PresentMode::rebuilds(std::uint32_t from, std::uint32_t to) noexcept
    -> bool {
  return (from == 0) != (to == 0);
}

}
