#pragma once

#include <animation.hpp>
#include <sample_renderer.hpp>

#include <arisa/provider/gpu/draw_data.hpp>
#include <arisa/provider/gpu/frame_hook.hpp>
#include <arisa/provider/gpu/gpu_surface.hpp>
#include <arisa/provider/gpu/render_pass.hpp>
#include <arisa/provider/horizon/deko3d_gpu.hpp>
#include <arisa/provider/horizon/deko3d_handle.hpp>
#include <arisa/provider/horizon/deko3d_memory.hpp>

#include <deko3d.hpp>

#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace arisa::examples {

class Deko3dSampleRenderer final : public SampleRenderer {
 public:
  static constexpr std::uint32_t picture_width = 480;
  static constexpr std::uint32_t picture_height = 270;
  static constexpr std::size_t ring_size = 3;
  static constexpr std::uint32_t uniform_size = DK_UNIFORM_BUF_ALIGNMENT;
  static constexpr std::uint32_t command_size = 0x4000;

  [[nodiscard]] static auto create(provider::horizon::Deko3dGpu& gpu,
                                   Animation& animation)
      -> std::expected<Deko3dSampleRenderer, std::string>;

  Deko3dSampleRenderer(Deko3dSampleRenderer&&) noexcept = default;
  ~Deko3dSampleRenderer() override;

  auto backdrop() const noexcept -> const provider::FrameHook& override {
    return backdrop_;
  }

  auto card() const noexcept -> const provider::GpuSurface& override {
    return card_;
  }

  auto picture() const noexcept -> provider::TextureId override {
    return ring_[shown_].texture.id();
  }

  auto repaint_picture() -> std::optional<provider::TextureId> override;

 private:
  using Allocation = provider::horizon::Deko3dMemory::Allocation;
  using FrameTarget = provider::horizon::Deko3dGpu::FrameTarget;
  using Queue = provider::horizon::Deko3dHandle<DkQueue, dkQueueDestroy>;
  using CmdBuf = provider::horizon::Deko3dHandle<DkCmdBuf, dkCmdBufDestroy>;

  struct Program {
    dk::Shader vertex;
    dk::Shader fragment;
  };

  struct Target {
    const DkImageView& view;
    provider::PassRegion viewport;
    provider::ClipRect scissor;
  };

  struct Frame {
    dk::Image image;
    Allocation command_memory;
    CmdBuf commands;
    dk::Fence done;
    bool pending{false};
    provider::horizon::Deko3dGpu::Imported texture;
  };

  Deko3dSampleRenderer(Animation& animation, Allocation code,
                       Allocation uniforms, Program program, Queue queue,
                       Allocation picture_uniforms, std::vector<Frame> ring,
                       provider::FrameHook backdrop,
                       provider::GpuSurface card) noexcept;

  [[nodiscard]] static auto frame(provider::horizon::Deko3dGpu& gpu)
      -> std::expected<Frame, std::string>;

  static auto whole(const DkImageView& view, std::uint32_t width,
                    std::uint32_t height) -> Target;

  static void draw(dk::CmdBuf& commands, const Target& target,
                   const Program& program, DkGpuAddr uniforms,
                   const Animation::Uniforms& values);

  std::reference_wrapper<Animation> animation_;
  Allocation code_;
  Allocation uniforms_;
  Program program_;
  Queue queue_;
  Allocation picture_uniforms_;
  std::vector<Frame> ring_;
  std::size_t shown_{0};
  provider::FrameHook backdrop_;
  provider::GpuSurface card_;
};

}
