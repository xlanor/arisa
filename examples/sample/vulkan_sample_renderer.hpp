#pragma once

#include <animation.hpp>
#include <sample_renderer.hpp>
#include <vulkan_plasma_program.hpp>
#include <vulkan_recorder.hpp>
#include <vulkan_timeline.hpp>

#include <arisa/provider/gpu/frame_hook.hpp>
#include <arisa/provider/gpu/gpu_surface.hpp>
#include <arisa/provider/vulkan/device_object.hpp>
#include <arisa/provider/vulkan/vulkan_gpu.hpp>
#include <arisa/provider/vulkan/vulkan_image.hpp>

#include <vulkan/vulkan.h>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace arisa::examples {

class VulkanSampleRenderer final : public SampleRenderer {
 public:
  static constexpr std::uint32_t picture_width = 480;
  static constexpr std::uint32_t picture_height = 270;
  static constexpr std::size_t ring_size = 3;
  static constexpr VkFormat picture_format = VK_FORMAT_R8G8B8A8_UNORM;

  [[nodiscard]] static auto create(provider::vulkan::VulkanGpu& gpu,
                                   Animation& animation)
      -> std::expected<VulkanSampleRenderer, std::string>;

  VulkanSampleRenderer(VulkanSampleRenderer&&) noexcept = default;
  ~VulkanSampleRenderer() override;

  auto backdrop() const noexcept -> const provider::FrameHook& override {
    return *backdrop_;
  }

  auto card() const noexcept -> const provider::GpuSurface& override {
    return *card_;
  }

  auto picture() const noexcept -> provider::TextureId override {
    return ring_[shown_].texture.id();
  }

  auto repaint_picture() -> std::optional<provider::TextureId> override;

 private:
  using Program = std::shared_ptr<VulkanPlasmaProgram>;
  using Slots = std::shared_ptr<const VulkanPlasmaProgram::Uniforms>;

  struct Painting {
    Program program;
    Slots slots;
  };

  struct Frame {
    provider::vulkan::VulkanImage image;
    provider::vulkan::VulkanFramebufferHandle framebuffer;
    VulkanRecorder recorder;
    bool painted{false};
    provider::vulkan::VulkanGpu::Imported texture;
  };

  VulkanSampleRenderer(Animation& animation, Painting screen, Painting card,
                       VulkanTimeline timeline, std::vector<Frame> ring,
                       provider::FrameHook backdrop,
                       provider::GpuSurface surface) noexcept;

  [[nodiscard]] static auto painting(const provider::vulkan::VulkanGpu& gpu,
                                     std::uint32_t slots)
      -> std::expected<Painting, std::string>;

  [[nodiscard]] static auto frame(provider::vulkan::VulkanGpu& gpu,
                                  const VulkanPlasmaProgram& program)
      -> std::expected<Frame, std::string>;

  std::reference_wrapper<Animation> animation_;
  Painting screen_;
  Painting card_painting_;
  VulkanTimeline timeline_;
  std::vector<Frame> ring_;
  std::size_t shown_{0};
  std::optional<provider::FrameHook> backdrop_;
  std::optional<provider::GpuSurface> card_;
};

}
