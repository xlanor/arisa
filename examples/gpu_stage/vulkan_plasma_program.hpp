#pragma once

#include <arisa/provider/gpu/surface_frame.hpp>
#include <arisa/provider/vulkan/device_object.hpp>
#include <arisa/provider/vulkan/vulkan_buffer.hpp>
#include <arisa/provider/vulkan/vulkan_device.hpp>
#include <arisa/provider/vulkan/vulkan_gpu.hpp>

#include <vulkan/vulkan.h>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <flat_map>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

namespace arisa::examples {

class VulkanPlasmaProgram {
 public:
  static constexpr VkDeviceSize uniform_stride = 256;
  static constexpr std::uint32_t maximum_sets = 32;
  static constexpr VkDeviceSize uniform_range = 32;

  class Uniforms {
   public:
    Uniforms(provider::vulkan::VulkanBuffer buffer,
             std::vector<VkDescriptorSet> sets) noexcept;

    auto buffer() const noexcept -> VkBuffer { return buffer_.handle(); }

    auto set(std::size_t slot) const noexcept -> VkDescriptorSet {
      return sets_[slot];
    }

    auto slots() const noexcept -> std::size_t { return sets_.size(); }

   private:
    provider::vulkan::VulkanBuffer buffer_;
    std::vector<VkDescriptorSet> sets_;
  };

  struct Pass {
    VkFramebuffer framebuffer{VK_NULL_HANDLE};
    VkFormat format{VK_FORMAT_UNDEFINED};
    VkExtent2D extent{};
    VkRect2D area{};
    VkViewport viewport{};

    static auto covering(VkFramebuffer framebuffer, VkFormat format,
                         VkExtent2D extent) -> Pass;

    auto within(const provider::SurfaceFrame& frame) const -> Pass;
  };

  static constexpr std::uint64_t retire_after = 8;

  [[nodiscard]] static auto create(const provider::vulkan::VulkanDevice& device,
                                   std::span<const VkFormat> formats)
      -> std::expected<std::shared_ptr<VulkanPlasmaProgram>, std::string>;

  VulkanPlasmaProgram(const VulkanPlasmaProgram&) = delete;
  auto operator=(const VulkanPlasmaProgram&) -> VulkanPlasmaProgram& = delete;
  VulkanPlasmaProgram(VulkanPlasmaProgram&&) = delete;
  auto operator=(VulkanPlasmaProgram&&) -> VulkanPlasmaProgram& = delete;
  ~VulkanPlasmaProgram();

  [[nodiscard]] auto uniforms(std::uint32_t slots)
      -> std::expected<Uniforms, std::string>;

  [[nodiscard]] auto framebuffer(VkImageView view, VkFormat format,
                                 VkExtent2D extent) const
      -> std::expected<provider::vulkan::VulkanFramebufferHandle, std::string>;

  [[nodiscard]] auto target(
      const provider::vulkan::VulkanGpu::FrameTarget& target)
      -> std::expected<Pass, std::string>;

  template <class Values>
    requires std::is_trivially_copyable_v<Values> &&
             (sizeof(Values) == uniform_range)
  void draw(VkCommandBuffer commands, const Pass& pass,
            const Uniforms& uniforms, std::size_t slot,
            const Values& values) const {
    record(commands, pass, uniforms, slot,
           std::as_bytes(std::span{&values, 1}));
  }

  auto device() const noexcept -> const provider::vulkan::VulkanDevice& {
    return device_.get();
  }

 private:
  struct Variant {
    provider::vulkan::VulkanRenderPassHandle render_pass;
    provider::vulkan::VulkanPipelineHandle pipeline;
  };

  struct Shown {
    std::uint64_t generation{0};
    VkImageView view{VK_NULL_HANDLE};
    VkExtent2D extent{};
    provider::vulkan::VulkanFramebufferHandle framebuffer;
  };

  struct Retired {
    std::uint64_t at{0};
    provider::vulkan::VulkanFramebufferHandle framebuffer;
  };

  struct Layouts {
    provider::vulkan::VulkanDescriptorSetLayoutHandle empty;
    provider::vulkan::VulkanDescriptorSetLayoutHandle uniforms;
    provider::vulkan::VulkanPipelineLayoutHandle pipeline;
  };

  class Key {
    friend class VulkanPlasmaProgram;
    Key() = default;
  };

 public:
  VulkanPlasmaProgram(Key key, const provider::vulkan::VulkanDevice& device,
                      Layouts layouts,
                      provider::vulkan::VulkanDescriptorPoolHandle pool,
                      std::flat_map<VkFormat, Variant> variants) noexcept;

 private:
  [[nodiscard]] static auto layouts(VkDevice device)
      -> std::expected<Layouts, std::string>;

  [[nodiscard]] static auto pool(VkDevice device)
      -> std::expected<provider::vulkan::VulkanDescriptorPoolHandle,
                       std::string>;

  [[nodiscard]] static auto module(VkDevice device,
                                   std::span<const std::uint8_t> code)
      -> std::expected<provider::vulkan::VulkanShaderModuleHandle, std::string>;

  [[nodiscard]] static auto variant(VkDevice device, VkFormat format,
                                    VkPipelineLayout layout,
                                    VkShaderModule vertex,
                                    VkShaderModule fragment)
      -> std::expected<Variant, std::string>;

  static void uniform_barrier(VkCommandBuffer commands, VkBuffer buffer,
                              VkDeviceSize offset, bool writing) noexcept;

  void record(VkCommandBuffer commands, const Pass& pass,
              const Uniforms& uniforms, std::size_t slot,
              std::span<const std::byte> values) const;

  std::reference_wrapper<const provider::vulkan::VulkanDevice> device_;
  Layouts layouts_;
  provider::vulkan::VulkanDescriptorPoolHandle pool_;
  std::flat_map<VkFormat, Variant> variants_;
  std::flat_map<std::uint32_t, Shown> shown_;
  std::vector<Retired> retired_;
  std::uint64_t targets_{0};
};

}
