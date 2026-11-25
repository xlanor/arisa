#pragma once

#include <stage_control.hpp>
#include <vulkan_plasma_program.hpp>

#include <arisa/provider/gpu/frame_hook.hpp>
#include <arisa/provider/gpu/gpu_surface.hpp>
#include <arisa/provider/vulkan/vulkan_gpu.hpp>

#include <expected>
#include <memory>
#include <optional>
#include <string>

namespace arisa::examples {

class VulkanPlasma {
 public:
  [[nodiscard]] static auto create(provider::vulkan::VulkanGpu& gpu,
                                   StageControl& control)
      -> std::expected<VulkanPlasma, std::string>;

  VulkanPlasma(VulkanPlasma&& other) noexcept = default;
  auto operator=(VulkanPlasma&& other) noexcept -> VulkanPlasma& = delete;
  VulkanPlasma(const VulkanPlasma&) = delete;
  auto operator=(const VulkanPlasma&) -> VulkanPlasma& = delete;
  ~VulkanPlasma();

  auto hook() const noexcept -> const provider::FrameHook& { return *hook_; }

  auto texture_surface() const noexcept -> const provider::GpuSurface& {
    return *texture_;
  }

  auto underlay_surface() const noexcept -> const provider::GpuSurface& {
    return *underlay_;
  }

  auto program() const noexcept -> const std::shared_ptr<VulkanPlasmaProgram>& {
    return program_;
  }

 private:
  struct Painting {
    std::shared_ptr<VulkanPlasmaProgram> program;
    std::shared_ptr<const VulkanPlasmaProgram::Uniforms> uniforms;
  };

  VulkanPlasma(Painting screen, Painting surface, provider::FrameHook hook,
               provider::GpuSurface texture,
               provider::GpuSurface underlay) noexcept;

  [[nodiscard]] static auto painting(const provider::vulkan::VulkanGpu& gpu,
                                     std::uint32_t slots)
      -> std::expected<Painting, std::string>;

  std::shared_ptr<VulkanPlasmaProgram> program_;
  std::shared_ptr<const VulkanPlasmaProgram::Uniforms> uniforms_;
  std::shared_ptr<VulkanPlasmaProgram> surface_program_;
  std::shared_ptr<const VulkanPlasmaProgram::Uniforms> surface_uniforms_;
  std::optional<provider::FrameHook> hook_;
  std::optional<provider::GpuSurface> texture_;
  std::optional<provider::GpuSurface> underlay_;
};

}
