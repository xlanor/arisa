#include <arisa/provider/vulkan/vulkan_status.hpp>
#include <provider/vulkan/frame_slot.hpp>

#include <limits>
#include <utility>

namespace arisa::provider::vulkan {

FrameSlot::FrameSlot(VulkanCommandPoolHandle pool, VkCommandBuffer commands,
                     VulkanFenceHandle fence, VulkanSemaphoreHandle acquired,
                     std::optional<TimestampQueries> timestamps) noexcept
    : pool_(std::move(pool)),
      commands_(commands),
      fence_(std::move(fence)),
      acquired_(std::move(acquired)),
      timestamps_(std::move(timestamps)) {}

auto FrameSlot::create(const VulkanDevice& device)
    -> std::expected<FrameSlot, ProviderError> {
  VkDevice const handle = device.handle();
  const VkCommandPoolCreateInfo pool_info{
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .pNext = nullptr,
      .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
      .queueFamilyIndex = device.queue_family()};
  VkCommandPool pool = VK_NULL_HANDLE;
  if (const VkResult result =
          vkCreateCommandPool(handle, &pool_info, nullptr, &pool);
      result != VK_SUCCESS) {
    return std::unexpected(VulkanStatus::error(result));
  }
  VulkanCommandPoolHandle owned_pool{{.device = handle, .object = pool}};
  const VkCommandBufferAllocateInfo buffer_info{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .pNext = nullptr,
      .commandPool = pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1};
  VkCommandBuffer commands = VK_NULL_HANDLE;
  if (const VkResult result =
          vkAllocateCommandBuffers(handle, &buffer_info, &commands);
      result != VK_SUCCESS) {
    return std::unexpected(VulkanStatus::error(result));
  }
  const VkFenceCreateInfo fence_info{
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0};
  VkFence fence = VK_NULL_HANDLE;
  if (const VkResult result =
          vkCreateFence(handle, &fence_info, nullptr, &fence);
      result != VK_SUCCESS) {
    return std::unexpected(VulkanStatus::error(result));
  }
  VulkanFenceHandle owned_fence{{.device = handle, .object = fence}};
  const VkSemaphoreCreateInfo semaphore_info{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = nullptr,
      .flags = 0};
  VkSemaphore semaphore = VK_NULL_HANDLE;
  if (const VkResult result =
          vkCreateSemaphore(handle, &semaphore_info, nullptr, &semaphore);
      result != VK_SUCCESS) {
    return std::unexpected(VulkanStatus::error(result));
  }
  VulkanSemaphoreHandle owned_semaphore{
      {.device = handle, .object = semaphore}};
  std::optional<TimestampQueries> timestamps;
  if (auto created = TimestampQueries::create(device); created.has_value()) {
    timestamps.emplace(std::move(*created));
  }
  return FrameSlot{std::move(owned_pool), commands, std::move(owned_fence),
                   std::move(owned_semaphore), std::move(timestamps)};
}

auto FrameSlot::ready(const VulkanDevice& device)
    -> std::expected<bool, ProviderError> {
  if (recording_) {
    return false;
  }
  VkDevice const handle = device.handle();
  bool waited = false;
  if (pending_) {
    VkFence const fence = fence_.get().object;
    if (vkGetFenceStatus(handle, fence) != VK_SUCCESS) {
      if (const VkResult result =
              vkWaitForFences(handle, 1, &fence, VK_TRUE,
                              std::numeric_limits<std::uint64_t>::max());
          result != VK_SUCCESS) {
        return std::unexpected(VulkanStatus::error(result));
      }
      waited = true;
    }
    pending_ = false;
  }
  if (dirty_) {
    if (const VkResult result =
            vkResetCommandPool(handle, pool_.get().object, 0);
        result != VK_SUCCESS) {
      return std::unexpected(VulkanStatus::error(result));
    }
    staging_.reset();
    reset_geometry(device);
    dirty_ = false;
  }
  const VkCommandBufferBeginInfo begin{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .pNext = nullptr,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
      .pInheritanceInfo = nullptr};
  if (const VkResult result = vkBeginCommandBuffer(commands_, &begin);
      result != VK_SUCCESS) {
    return std::unexpected(VulkanStatus::error(result));
  }
  recording_ = true;
  return waited;
}

void FrameSlot::reset_geometry(const VulkanDevice& device) {
  geometry_.reset(device);
  shapes_.reset(device);
}

auto FrameSlot::submit(const VulkanDevice& device, std::span<const Wait> waits,
                       VkSemaphore signal, std::uint64_t serial)
    -> std::expected<void, ProviderError> {
  recording_ = false;
  dirty_ = true;
  if (const VkResult result = vkEndCommandBuffer(commands_);
      result != VK_SUCCESS) {
    return std::unexpected(VulkanStatus::error(result));
  }
  geometry_.flush();
  shapes_.flush();
  staging_.flush();
  VkFence const fence = fence_.get().object;
  if (const VkResult result = vkResetFences(fence_.get().device, 1, &fence);
      result != VK_SUCCESS) {
    return std::unexpected(VulkanStatus::error(result));
  }
  wait_semaphores_.clear();
  wait_stages_.clear();
  wait_values_.clear();
  bool timeline = false;
  for (const Wait& wait : waits) {
    wait_semaphores_.push_back(wait.semaphore);
    wait_stages_.push_back(wait.stage);
    wait_values_.push_back(wait.value.value_or(0));
    timeline = timeline || wait.value.has_value();
  }
  const std::uint64_t signal_value = 0;
  const std::uint32_t signals = signal != VK_NULL_HANDLE ? 1U : 0U;
  const VkTimelineSemaphoreSubmitInfo values{
      .sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO,
      .pNext = nullptr,
      .waitSemaphoreValueCount =
          static_cast<std::uint32_t>(wait_values_.size()),
      .pWaitSemaphoreValues = wait_values_.data(),
      .signalSemaphoreValueCount = signals,
      .pSignalSemaphoreValues = &signal_value};
  const VkSubmitInfo info{
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .pNext = timeline ? &values : nullptr,
      .waitSemaphoreCount = static_cast<std::uint32_t>(wait_semaphores_.size()),
      .pWaitSemaphores = wait_semaphores_.data(),
      .pWaitDstStageMask = wait_stages_.data(),
      .commandBufferCount = 1,
      .pCommandBuffers = &commands_,
      .signalSemaphoreCount = signals,
      .pSignalSemaphores = signal != VK_NULL_HANDLE ? &signal : nullptr};
  if (auto submitted = device.submit(std::span{&info, 1}, fence);
      !submitted.has_value()) {
    return std::unexpected(submitted.error());
  }
  pending_ = true;
  serial_ = serial;
  return {};
}

void FrameSlot::release_acquired(const VulkanDevice& device) noexcept {
  VkSemaphore const semaphore = acquired_.get().object;
  const VkPipelineStageFlags stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
  const VkSubmitInfo info{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
                          .pNext = nullptr,
                          .waitSemaphoreCount = 1,
                          .pWaitSemaphores = &semaphore,
                          .pWaitDstStageMask = &stage,
                          .commandBufferCount = 0,
                          .pCommandBuffers = nullptr,
                          .signalSemaphoreCount = 0,
                          .pSignalSemaphores = nullptr};
  static_cast<void>(device.submit(std::span{&info, 1}, VK_NULL_HANDLE));
}

auto FrameSlot::poll(VkDevice device) noexcept -> std::optional<std::uint64_t> {
  if (!pending_ ||
      vkGetFenceStatus(device, fence_.get().object) != VK_SUCCESS) {
    return std::nullopt;
  }
  pending_ = false;
  return serial_;
}

}
