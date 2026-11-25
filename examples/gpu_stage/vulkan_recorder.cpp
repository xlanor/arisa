#include <vulkan_recorder.hpp>

#include <arisa/provider/vulkan/vulkan_gpu.hpp>

#include <limits>
#include <span>
#include <utility>

namespace arisa::examples {

VulkanRecorder::VulkanRecorder(
    const provider::vulkan::VulkanDevice& device,
    provider::vulkan::VulkanCommandPoolHandle pool, VkCommandBuffer commands,
    provider::vulkan::VulkanFenceHandle done) noexcept
    : device_(device),
      pool_(std::move(pool)),
      commands_(commands),
      done_(std::move(done)) {}

VulkanRecorder::VulkanRecorder(VulkanRecorder&& other) noexcept
    : device_(other.device_),
      pool_(std::move(other.pool_)),
      commands_(std::exchange(other.commands_, VK_NULL_HANDLE)),
      done_(std::move(other.done_)),
      pending_(std::exchange(other.pending_, false)) {}

auto VulkanRecorder::operator=(VulkanRecorder&& other) noexcept
    -> VulkanRecorder& {
  if (this != &other) {
    wait();
    device_ = other.device_;
    pool_ = std::move(other.pool_);
    commands_ = std::exchange(other.commands_, VK_NULL_HANDLE);
    done_ = std::move(other.done_);
    pending_ = std::exchange(other.pending_, false);
  }
  return *this;
}

VulkanRecorder::~VulkanRecorder() { wait(); }

auto VulkanRecorder::create(const provider::vulkan::VulkanDevice& device)
    -> std::expected<VulkanRecorder, std::string> {
  VkDevice const handle = device.handle();
  const VkCommandPoolCreateInfo pool_info{
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .pNext = nullptr,
      .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
      .queueFamilyIndex = device.queue_family()};
  VkCommandPool pool = VK_NULL_HANDLE;
  if (vkCreateCommandPool(handle, &pool_info, nullptr, &pool) != VK_SUCCESS) {
    return std::unexpected(std::string{"the video command pool failed"});
  }
  provider::vulkan::VulkanCommandPoolHandle owned_pool{
      {.device = handle, .object = pool}};
  const VkCommandBufferAllocateInfo buffer_info{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .pNext = nullptr,
      .commandPool = pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1};
  VkCommandBuffer commands = VK_NULL_HANDLE;
  if (vkAllocateCommandBuffers(handle, &buffer_info, &commands) != VK_SUCCESS) {
    return std::unexpected(std::string{"the video command buffer failed"});
  }
  const VkFenceCreateInfo fence_info{
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0};
  VkFence fence = VK_NULL_HANDLE;
  if (vkCreateFence(handle, &fence_info, nullptr, &fence) != VK_SUCCESS) {
    return std::unexpected(std::string{"the video fence failed"});
  }
  return VulkanRecorder{
      device, std::move(owned_pool), commands,
      provider::vulkan::VulkanFenceHandle{{.device = handle, .object = fence}}};
}

void VulkanRecorder::wait() noexcept {
  if (!pending_ || !done_.valid()) {
    return;
  }
  VkFence const fence = done_.get().object;
  static_cast<void>(vkWaitForFences(done_.get().device, 1, &fence, VK_TRUE,
                                    std::numeric_limits<std::uint64_t>::max()));
  pending_ = false;
}

auto VulkanRecorder::begin() -> std::expected<VkCommandBuffer, std::string> {
  wait();
  VkDevice const handle = device_.get().handle();
  VkFence const fence = done_.get().object;
  if (vkResetFences(handle, 1, &fence) != VK_SUCCESS ||
      vkResetCommandPool(handle, pool_.get().object, 0) != VK_SUCCESS) {
    return std::unexpected(std::string{"the video commands could not reset"});
  }
  const VkCommandBufferBeginInfo info{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .pNext = nullptr,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
      .pInheritanceInfo = nullptr};
  if (vkBeginCommandBuffer(commands_, &info) != VK_SUCCESS) {
    return std::unexpected(std::string{"the video commands could not begin"});
  }
  return commands_;
}

auto VulkanRecorder::submit(provider::vulkan::ReadySemaphore signal)
    -> std::expected<void, std::string> {
  if (vkEndCommandBuffer(commands_) != VK_SUCCESS) {
    return std::unexpected(std::string{"the video commands could not end"});
  }
  const std::uint64_t value = signal.value.value_or(0);
  const VkTimelineSemaphoreSubmitInfo values{
      .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
      .pNext = nullptr,
      .waitSemaphoreValueCount = 0,
      .pWaitSemaphoreValues = nullptr,
      .signalSemaphoreValueCount = 1,
      .pSignalSemaphoreValues = &value};
  const VkSubmitInfo info{
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .pNext = signal.timeline() ? &values : nullptr,
      .waitSemaphoreCount = 0,
      .pWaitSemaphores = nullptr,
      .pWaitDstStageMask = nullptr,
      .commandBufferCount = 1,
      .pCommandBuffers = &commands_,
      .signalSemaphoreCount = signal.waits() ? 1U : 0U,
      .pSignalSemaphores = signal.waits() ? &signal.semaphore : nullptr};
  if (auto submitted =
          device_.get().submit(std::span{&info, 1}, done_.get().object);
      !submitted.has_value()) {
    return std::unexpected(std::string{"the video frame was not submitted: "} +
                           std::string{submitted.error().description()});
  }
  pending_ = true;
  return {};
}

void VulkanRecorder::transition(VkCommandBuffer commands, VkImage image,
                                VkImageLayout from, VkImageLayout to,
                                VkPipelineStageFlags before,
                                VkAccessFlags written,
                                VkPipelineStageFlags after,
                                VkAccessFlags reading) noexcept {
  const VkImageMemoryBarrier barrier{
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .pNext = nullptr,
      .srcAccessMask = written,
      .dstAccessMask = reading,
      .oldLayout = from,
      .newLayout = to,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = image,
      .subresourceRange =
          VkImageSubresourceRange{.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                  .baseMipLevel = 0,
                                  .levelCount = 1,
                                  .baseArrayLayer = 0,
                                  .layerCount = 1}};
  vkCmdPipelineBarrier(commands, before, after, 0, 0, nullptr, 0, nullptr, 1,
                       &barrier);
}

void VulkanRecorder::to_attachment(VkCommandBuffer commands, VkImage image,
                                   VkImageLayout from) noexcept {
  transition(commands, image, from, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
             VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
}

void VulkanRecorder::to_sampled(VkCommandBuffer commands,
                                VkImage image) noexcept {
  transition(commands, image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
             VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
             VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
}

void VulkanRecorder::to_host_sampled(VkCommandBuffer commands,
                                     VkImage image) noexcept {
  transition(commands, image, VK_IMAGE_LAYOUT_PREINITIALIZED,
             provider::vulkan::VulkanGpu::host_import_layout,
             VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_HOST_WRITE_BIT,
             VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_ACCESS_SHADER_READ_BIT);
}

}
