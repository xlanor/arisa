#pragma once

#include <stage_control.hpp>

#include <arisa/provider/gpu/draw_data.hpp>
#include <arisa/provider/gpu/frame_hook.hpp>
#include <arisa/provider/gpu/gpu_surface.hpp>
#include <arisa/provider/gpu/render_pass.hpp>
#include <arisa/provider/gpu/surface_frame.hpp>
#include <arisa/provider/horizon/deko3d_gpu.hpp>
#include <arisa/provider/horizon/deko3d_memory.hpp>

#include <deko3d.hpp>

#include <cstdint>
#include <expected>
#include <span>
#include <string>

namespace arisa::examples {

class Deko3dPlasma {
 public:
  [[nodiscard]] static auto create(provider::horizon::Deko3dGpu& gpu,
                                   StageControl& control)
      -> std::expected<Deko3dPlasma, std::string>;

  struct Program {
    dk::Shader vertex;
    dk::Shader fragment;
    DkGpuAddr uniforms;
  };

  struct Target {
    const DkImageView& view;
    provider::PassRegion viewport;
    provider::ClipRect scissor;

    static auto whole(const DkImageView& view, std::uint32_t width,
                      std::uint32_t height) -> Target;

    static auto within(const DkImageView& view,
                       const provider::SurfaceFrame& frame) -> Target;
  };

  static constexpr std::uint32_t uniform_size = DK_UNIFORM_BUF_ALIGNMENT;

  auto hook() const noexcept -> const provider::FrameHook& { return hook_; }

  auto texture_surface() const noexcept -> const provider::GpuSurface& {
    return texture_;
  }

  auto underlay_surface() const noexcept -> const provider::GpuSurface& {
    return underlay_;
  }

  auto program() const noexcept -> const Program& { return program_; }

  static void draw(dk::CmdBuf& commands, const Target& target,
                   const Program& program,
                   const StageControl::Uniforms& uniforms);

 private:
  using Allocation = provider::horizon::Deko3dMemory::Allocation;
  using FrameTarget = provider::horizon::Deko3dGpu::FrameTarget;

  Deko3dPlasma(Allocation code, Allocation uniforms, Program program,
               provider::FrameHook hook, provider::GpuSurface texture,
               provider::GpuSurface underlay) noexcept;

  Allocation code_;
  Allocation uniforms_;
  Program program_;
  provider::FrameHook hook_;
  provider::GpuSurface texture_;
  provider::GpuSurface underlay_;
};

}
