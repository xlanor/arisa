#pragma once

#include <arisa/provider/vulkan/vulkan_device.hpp>

#include <vulkan/vulkan.h>

#include <span>
#include <vector>

namespace arisa::provider::vulkan {

class ImageBarriers {
 public:
  struct Access {
    VkPipelineStageFlags2 stage{VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT};
    VkAccessFlags2 access{0};
  };

  struct Transition {
    VkImage image{VK_NULL_HANDLE};
    VkImageLayout from{VK_IMAGE_LAYOUT_UNDEFINED};
    VkImageLayout to{VK_IMAGE_LAYOUT_UNDEFINED};
    Access before{};
    Access after{};
  };

  explicit ImageBarriers(const VulkanDevice& device) noexcept
      : barrier2_(device.pipeline_barrier2()) {}

  auto synchronization2() const noexcept -> bool {
    return barrier2_ != nullptr;
  }

  void record(VkCommandBuffer commands,
              std::span<const Transition> transitions);

  void record(VkCommandBuffer commands, const Transition& transition) {
    record(commands, std::span{&transition, 1});
  }

  static auto colour_range() noexcept -> VkImageSubresourceRange {
    return VkImageSubresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                   .baseMipLevel = 0,
                                   .levelCount = 1,
                                   .baseArrayLayer = 0,
                                   .layerCount = 1};
  }

 private:
  void record_classic(VkCommandBuffer commands,
                      std::span<const Transition> transitions);

  PFN_vkCmdPipelineBarrier2 barrier2_;
  std::vector<VkImageMemoryBarrier2> barriers2_;
  std::vector<VkImageMemoryBarrier> barriers_;
};

}
