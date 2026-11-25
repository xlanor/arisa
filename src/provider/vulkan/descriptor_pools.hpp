#pragma once

#include <arisa/provider/error.hpp>
#include <arisa/provider/vulkan/device_object.hpp>

#include <vulkan/vulkan.h>

#include <cstdint>
#include <expected>
#include <vector>

namespace arisa::provider::vulkan {

class DescriptorPools {
 public:
  static constexpr std::uint32_t sets_per_pool = 256;

  struct Lease {
    std::uint32_t pool{0};
    VkDescriptorSet set{VK_NULL_HANDLE};
  };

  explicit DescriptorPools(VkDevice device) noexcept : device_(device) {}

  [[nodiscard]] auto allocate(VkDescriptorSetLayout layout)
      -> std::expected<Lease, ProviderError>;

  void release(Lease lease) noexcept;

 private:
  struct Pool {
    VulkanDescriptorPoolHandle pool;
    std::uint32_t live{0};
  };

  [[nodiscard]] auto grow() -> std::expected<void, ProviderError>;

  VkDevice device_;
  std::vector<Pool> pools_;
};

}
