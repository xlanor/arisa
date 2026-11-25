#pragma once

#include <arisa/provider/error.hpp>
#include <arisa/provider/gpu/texture.hpp>
#include <arisa/provider/vulkan/device_object.hpp>
#include <arisa/provider/vulkan/vulkan_device.hpp>

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <expected>
#include <span>
#include <vector>

namespace arisa::provider::vulkan {

class ShapePipelines {
 public:
  enum class Kind : std::uint8_t {
    textured,
    shape,
  };

  enum class Destination : std::uint8_t {
    screen,
    offscreen,
  };

  static constexpr VkFormat offscreen_format = VK_FORMAT_R8G8B8A8_UNORM;

  struct VertexConstants {
    std::array<float, 2> scale{};
    std::array<float, 2> translation{};
    std::array<float, 4> placement{};

    friend constexpr auto operator==(const VertexConstants&,
                                     const VertexConstants&) -> bool = default;
  };

  struct FragmentConstants {
    std::array<float, 4> trim{};

    friend constexpr auto operator==(const FragmentConstants&,
                                     const FragmentConstants&)
        -> bool = default;
  };

  static constexpr std::uint32_t fragment_offset = sizeof(VertexConstants);

  [[nodiscard]] static auto create(const VulkanDevice& device, VkFormat target)
      -> std::expected<ShapePipelines, ProviderError>;

  [[nodiscard]] auto retarget(const VulkanDevice& device, VkFormat target)
      -> std::expected<void, ProviderError>;

  auto format() const noexcept -> VkFormat { return format_; }

  auto render_pass(Destination destination = Destination::screen) const noexcept
      -> VkRenderPass {
    return target(destination).render_pass.get().object;
  }

  auto layout() const noexcept -> VkPipelineLayout {
    return layout_.get().object;
  }

  auto set_layout() const noexcept -> VkDescriptorSetLayout {
    return set_layout_.get().object;
  }

  auto pipeline(Kind kind,
                Destination destination = Destination::screen) const noexcept
      -> VkPipeline {
    const Target& chosen = target(destination);
    return kind == Kind::shape ? chosen.shape.get().object
                               : chosen.textured.get().object;
  }

  auto sampler(SamplerFilter filter) const noexcept -> VkSampler {
    return filter == SamplerFilter::nearest ? nearest_.get().object
                                            : linear_.get().object;
  }

 private:
  struct Stage {
    std::span<const std::uint8_t> vertex;
    std::span<const std::uint8_t> fragment;
  };

  struct VertexLayout {
    std::uint32_t stride{0};
    std::vector<VkVertexInputAttributeDescription> attributes;
  };

  struct Target {
    VulkanRenderPassHandle render_pass;
    VulkanPipelineHandle textured;
    VulkanPipelineHandle shape;
  };

  ShapePipelines() = default;

  auto target(Destination destination) const noexcept -> const Target& {
    return destination == Destination::offscreen ? offscreen_ : screen_;
  }

  [[nodiscard]] auto build_target(VkDevice device, VkFormat format) const
      -> std::expected<Target, ProviderError>;

  [[nodiscard]] static auto module(VkDevice device,
                                   std::span<const std::uint8_t> code)
      -> std::expected<VulkanShaderModuleHandle, ProviderError>;

  [[nodiscard]] static auto build(VkDevice device, VkRenderPass pass,
                                  VkPipelineLayout layout, Stage stage,
                                  const VertexLayout& vertices)
      -> std::expected<VulkanPipelineHandle, ProviderError>;

  [[nodiscard]] static auto make_sampler(VkDevice device, VkFilter filter)
      -> std::expected<VulkanSamplerHandle, ProviderError>;

  static auto attribute(std::uint32_t location, VkFormat format,
                        std::size_t offset) noexcept
      -> VkVertexInputAttributeDescription;

  static auto textured_layout() -> VertexLayout;

  static auto shape_layout() -> VertexLayout;

  VkFormat format_{VK_FORMAT_UNDEFINED};
  VulkanDescriptorSetLayoutHandle set_layout_;
  VulkanPipelineLayoutHandle layout_;
  VulkanSamplerHandle linear_;
  VulkanSamplerHandle nearest_;
  Target screen_;
  Target offscreen_;
};

}
