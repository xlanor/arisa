#pragma once

#include <stage_control.hpp>
#include <vulkan_plasma.hpp>
#include <vulkan_plasma_program.hpp>
#include <vulkan_recorder.hpp>
#include <vulkan_timeline.hpp>

#include <arisa/provider/gpu/texture.hpp>
#include <arisa/provider/vulkan/device_object.hpp>
#include <arisa/provider/vulkan/vulkan_gpu.hpp>
#include <arisa/provider/vulkan/vulkan_image.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace arisa::examples {

class VulkanNv12Video {
 public:
  static constexpr std::size_t ring_size = 3;
  static constexpr std::uint32_t width = 640;
  static constexpr std::uint32_t height = 360;

  [[nodiscard]] static auto create(provider::vulkan::VulkanGpu& gpu,
                                   const VulkanPlasma& plasma,
                                   const StageControl& control)
      -> std::expected<VulkanNv12Video, std::string>;

  VulkanNv12Video(VulkanNv12Video&& other) noexcept = default;
  auto operator=(VulkanNv12Video&& other) noexcept -> VulkanNv12Video& = delete;
  VulkanNv12Video(const VulkanNv12Video&) = delete;
  auto operator=(const VulkanNv12Video&) -> VulkanNv12Video& = delete;
  ~VulkanNv12Video();

  auto produce(StageControl& control, std::chrono::nanoseconds now) -> bool;

 private:
  struct Plane {
    provider::vulkan::VulkanImage image;
    provider::vulkan::VulkanFramebufferHandle framebuffer;
    VkFormat format{VK_FORMAT_R8_UNORM};
    VkExtent2D extent{};
  };

  struct Frame {
    Plane luma;
    Plane chroma;
    VulkanPlasmaProgram::Uniforms uniforms;
    VulkanRecorder recorder;
    bool drawn{false};
    provider::ImportDescription description;
    provider::vulkan::VulkanGpu::Imported texture;
  };

  VulkanNv12Video(provider::vulkan::VulkanGpu& gpu,
                  std::shared_ptr<VulkanPlasmaProgram> program,
                  VulkanTimeline timeline, std::vector<Frame> ring) noexcept;

  [[nodiscard]] static auto plane(const VulkanPlasmaProgram& program,
                                  VkFormat format,
                                  const provider::TextureDescription& described)
      -> std::expected<Plane, std::string>;

  [[nodiscard]] static auto import(
      provider::vulkan::VulkanGpu& gpu, const Frame& frame,
      const provider::ImportDescription& description)
      -> std::optional<provider::vulkan::VulkanGpu::Imported>;

  void draw(VkCommandBuffer commands, const Frame& frame, const Plane& plane,
            std::size_t slot, const StageControl::Uniforms& values) const;

  std::reference_wrapper<provider::vulkan::VulkanGpu> gpu_;
  std::shared_ptr<VulkanPlasmaProgram> program_;
  VulkanTimeline timeline_;
  std::vector<Frame> ring_;
  std::size_t next_{0};
};

}
