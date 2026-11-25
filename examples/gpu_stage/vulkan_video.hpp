#pragma once

#include <stage_control.hpp>
#include <vulkan_plasma.hpp>
#include <vulkan_plasma_program.hpp>
#include <vulkan_recorder.hpp>
#include <vulkan_timeline.hpp>

#include <arisa/provider/vulkan/device_object.hpp>
#include <arisa/provider/vulkan/vulkan_gpu.hpp>
#include <arisa/provider/vulkan/vulkan_image.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <string>
#include <vector>

namespace arisa::examples {

class VulkanVideo {
 public:
  static constexpr std::size_t ring_size = 3;
  static constexpr std::uint32_t width = 640;
  static constexpr std::uint32_t height = 360;
  static constexpr VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;

  [[nodiscard]] static auto create(provider::vulkan::VulkanGpu& gpu,
                                   const VulkanPlasma& plasma)
      -> std::expected<VulkanVideo, std::string>;

  VulkanVideo(VulkanVideo&& other) noexcept = default;
  auto operator=(VulkanVideo&& other) noexcept -> VulkanVideo& = delete;
  VulkanVideo(const VulkanVideo&) = delete;
  auto operator=(const VulkanVideo&) -> VulkanVideo& = delete;
  ~VulkanVideo();

  auto produce(StageControl& control, std::chrono::nanoseconds now) -> bool;

 private:
  struct Frame {
    provider::vulkan::VulkanImage image;
    provider::vulkan::VulkanFramebufferHandle framebuffer;
    VulkanPlasmaProgram::Uniforms uniforms;
    VulkanRecorder recorder;
    bool drawn{false};
    provider::vulkan::VulkanGpu::Imported texture;
  };

  VulkanVideo(std::shared_ptr<VulkanPlasmaProgram> program,
              VulkanTimeline timeline, std::vector<Frame> ring) noexcept;

  std::shared_ptr<VulkanPlasmaProgram> program_;
  VulkanTimeline timeline_;
  std::vector<Frame> ring_;
  std::size_t next_{0};
};

}
