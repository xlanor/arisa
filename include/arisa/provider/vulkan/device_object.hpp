#pragma once

#include <arisa/support/unique_handle.hpp>

#include <vulkan/vulkan.h>

namespace arisa::provider::vulkan {

template <class Handle, auto Destroy>
struct DeviceObjectTraits {
  struct handle_type {
    VkDevice device{VK_NULL_HANDLE};
    Handle object{VK_NULL_HANDLE};
  };

  static auto empty() noexcept -> handle_type { return handle_type{}; }

  static auto is_valid(const handle_type& handle) noexcept -> bool {
    return handle.object != VK_NULL_HANDLE;
  }

  static void release(handle_type& handle) noexcept {
    Destroy(handle.device, handle.object, nullptr);
  }
};

template <class Handle, auto Destroy>
using DeviceObject = support::UniqueHandle<DeviceObjectTraits<Handle, Destroy>>;

using VulkanBufferHandle = DeviceObject<VkBuffer, &vkDestroyBuffer>;
using VulkanMemoryHandle = DeviceObject<VkDeviceMemory, &vkFreeMemory>;
using VulkanImageHandle = DeviceObject<VkImage, &vkDestroyImage>;
using VulkanImageViewHandle = DeviceObject<VkImageView, &vkDestroyImageView>;
using VulkanSamplerHandle = DeviceObject<VkSampler, &vkDestroySampler>;
using VulkanFenceHandle = DeviceObject<VkFence, &vkDestroyFence>;
using VulkanSemaphoreHandle = DeviceObject<VkSemaphore, &vkDestroySemaphore>;
using VulkanCommandPoolHandle =
    DeviceObject<VkCommandPool, &vkDestroyCommandPool>;
using VulkanDescriptorPoolHandle =
    DeviceObject<VkDescriptorPool, &vkDestroyDescriptorPool>;
using VulkanDescriptorSetLayoutHandle =
    DeviceObject<VkDescriptorSetLayout, &vkDestroyDescriptorSetLayout>;
using VulkanPipelineLayoutHandle =
    DeviceObject<VkPipelineLayout, &vkDestroyPipelineLayout>;
using VulkanPipelineHandle = DeviceObject<VkPipeline, &vkDestroyPipeline>;
using VulkanShaderModuleHandle =
    DeviceObject<VkShaderModule, &vkDestroyShaderModule>;
using VulkanRenderPassHandle = DeviceObject<VkRenderPass, &vkDestroyRenderPass>;
using VulkanFramebufferHandle =
    DeviceObject<VkFramebuffer, &vkDestroyFramebuffer>;
using VulkanQueryPoolHandle = DeviceObject<VkQueryPool, &vkDestroyQueryPool>;
using VulkanSwapchainHandle =
    DeviceObject<VkSwapchainKHR, &vkDestroySwapchainKHR>;

}
