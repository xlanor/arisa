#include <arisa/provider/vulkan/vulkan_status.hpp>
#include <provider/vulkan/descriptor_pools.hpp>

namespace arisa::provider::vulkan {

auto DescriptorPools::grow() -> std::expected<void, ProviderError> {
  const VkDescriptorPoolSize size{
      .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
      .descriptorCount = sets_per_pool};
  const VkDescriptorPoolCreateInfo info{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .pNext = nullptr,
      .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
      .maxSets = sets_per_pool,
      .poolSizeCount = 1,
      .pPoolSizes = &size};
  VkDescriptorPool raw = VK_NULL_HANDLE;
  if (const VkResult result =
          vkCreateDescriptorPool(device_, &info, nullptr, &raw);
      result != VK_SUCCESS) {
    return std::unexpected(VulkanStatus::error(result));
  }
  pools_.push_back(Pool{
      .pool = VulkanDescriptorPoolHandle{{.device = device_, .object = raw}},
      .live = 0});
  return {};
}

auto DescriptorPools::allocate(VkDescriptorSetLayout layout)
    -> std::expected<Lease, ProviderError> {
  for (std::uint32_t index = 0;; ++index) {
    if (index == pools_.size()) {
      if (auto grown = grow(); !grown.has_value()) {
        return std::unexpected(grown.error());
      }
    }
    Pool& pool = pools_[index];
    if (pool.live >= sets_per_pool) {
      continue;
    }
    const VkDescriptorSetAllocateInfo info{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = pool.pool.get().object,
        .descriptorSetCount = 1,
        .pSetLayouts = &layout};
    VkDescriptorSet set = VK_NULL_HANDLE;
    const VkResult result = vkAllocateDescriptorSets(device_, &info, &set);
    if (result == VK_SUCCESS) {
      ++pool.live;
      return Lease{.pool = index, .set = set};
    }
    if (result != VK_ERROR_OUT_OF_POOL_MEMORY &&
        result != VK_ERROR_FRAGMENTED_POOL) {
      return std::unexpected(VulkanStatus::error(result));
    }
  }
}

void DescriptorPools::release(Lease lease) noexcept {
  if (lease.pool >= pools_.size() || lease.set == VK_NULL_HANDLE) {
    return;
  }
  Pool& pool = pools_[lease.pool];
  static_cast<void>(
      vkFreeDescriptorSets(device_, pool.pool.get().object, 1, &lease.set));
  --pool.live;
}

}
