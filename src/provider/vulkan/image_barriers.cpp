#include <provider/vulkan/image_barriers.hpp>

#include <cstdint>

namespace arisa::provider::vulkan {

void ImageBarriers::record(VkCommandBuffer commands,
                           std::span<const Transition> transitions) {
  if (transitions.empty()) {
    return;
  }
  if (barrier2_ == nullptr) {
    record_classic(commands, transitions);
    return;
  }
  std::vector<VkImageMemoryBarrier2>& barriers = barriers2_;
  barriers.clear();
  for (const Transition& each : transitions) {
    barriers.push_back(
        VkImageMemoryBarrier2{.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                              .pNext = nullptr,
                              .srcStageMask = each.before.stage,
                              .srcAccessMask = each.before.access,
                              .dstStageMask = each.after.stage,
                              .dstAccessMask = each.after.access,
                              .oldLayout = each.from,
                              .newLayout = each.to,
                              .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                              .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                              .image = each.image,
                              .subresourceRange = colour_range()});
  }
  const VkDependencyInfo dependency{
      .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
      .pNext = nullptr,
      .dependencyFlags = 0,
      .memoryBarrierCount = 0,
      .pMemoryBarriers = nullptr,
      .bufferMemoryBarrierCount = 0,
      .pBufferMemoryBarriers = nullptr,
      .imageMemoryBarrierCount = static_cast<std::uint32_t>(barriers.size()),
      .pImageMemoryBarriers = barriers.data()};
  barrier2_(commands, &dependency);
}

void ImageBarriers::record_classic(VkCommandBuffer commands,
                                   std::span<const Transition> transitions) {
  std::vector<VkImageMemoryBarrier>& barriers = barriers_;
  barriers.clear();
  VkPipelineStageFlags source = 0;
  VkPipelineStageFlags destination = 0;
  for (const Transition& each : transitions) {
    source |= static_cast<VkPipelineStageFlags>(each.before.stage);
    destination |= static_cast<VkPipelineStageFlags>(each.after.stage);
    barriers.push_back(VkImageMemoryBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = static_cast<VkAccessFlags>(each.before.access),
        .dstAccessMask = static_cast<VkAccessFlags>(each.after.access),
        .oldLayout = each.from,
        .newLayout = each.to,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = each.image,
        .subresourceRange = colour_range()});
  }
  vkCmdPipelineBarrier(commands, source, destination, 0, 0, nullptr, 0, nullptr,
                       static_cast<std::uint32_t>(barriers.size()),
                       barriers.data());
}

}
