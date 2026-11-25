#pragma once

#include <deko3d_plasma.hpp>
#include <stage_control.hpp>

#include <arisa/provider/gpu/texture.hpp>
#include <arisa/provider/horizon/deko3d_gpu.hpp>
#include <arisa/provider/horizon/deko3d_handle.hpp>
#include <arisa/provider/horizon/deko3d_memory.hpp>

#include <deko3d.hpp>

#include <array>
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

class Deko3dNv12Video {
 public:
  static constexpr std::size_t ring_size = 3;
  static constexpr std::uint32_t width = 640;
  static constexpr std::uint32_t height = 360;
  static constexpr std::uint32_t command_size = 0x4000;

  [[nodiscard]] static auto create(provider::horizon::Deko3dGpu& gpu,
                                   const Deko3dPlasma& plasma,
                                   const StageControl& control)
      -> std::expected<Deko3dNv12Video, std::string>;

  Deko3dNv12Video(Deko3dNv12Video&& other) noexcept = default;
  auto operator=(Deko3dNv12Video&& other) noexcept
      -> Deko3dNv12Video& = default;
  Deko3dNv12Video(const Deko3dNv12Video&) = delete;
  auto operator=(const Deko3dNv12Video&) -> Deko3dNv12Video& = delete;
  ~Deko3dNv12Video();

  auto produce(StageControl& control, std::chrono::nanoseconds now) -> bool;

 private:
  using Allocation = provider::horizon::Deko3dMemory::Allocation;
  using QueueHandle = provider::horizon::Deko3dHandle<DkQueue, dkQueueDestroy>;
  using CmdBufHandle =
      provider::horizon::Deko3dHandle<DkCmdBuf, dkCmdBufDestroy>;

  class Planes {
   public:
    static auto create(provider::horizon::Deko3dGpu& gpu)
        -> std::expected<std::shared_ptr<Planes>, std::string>;

    Planes(Allocation luma_memory, Allocation chroma_memory, dk::Image luma,
           dk::Image chroma) noexcept;

    auto luma() const noexcept -> const dk::Image& { return luma_; }

    auto chroma() const noexcept -> const dk::Image& { return chroma_; }

    auto natives() const noexcept -> std::array<const dk::Image*, 2> {
      return {&luma_, &chroma_};
    }

   private:
    static auto layout(dk::Device device, DkImageFormat format,
                       std::uint32_t plane_width, std::uint32_t plane_height)
        -> dk::ImageLayout;

    Allocation luma_memory_;
    Allocation chroma_memory_;
    dk::Image luma_;
    dk::Image chroma_;
  };

  struct Frame {
    std::shared_ptr<Planes> planes;
    provider::ImportDescription description;
    Allocation command_memory;
    CmdBufHandle commands;
    dk::Fence done;
    bool pending{false};
    provider::horizon::Deko3dGpu::Imported texture;
  };

  Deko3dNv12Video(provider::horizon::Deko3dGpu& gpu, QueueHandle queue,
                  Allocation uniforms, Deko3dPlasma::Program program,
                  std::vector<Frame> ring) noexcept;

  static auto import(provider::horizon::Deko3dGpu& gpu,
                     const std::shared_ptr<Planes>& planes,
                     const provider::ImportDescription& description)
      -> std::optional<provider::horizon::Deko3dGpu::Imported>;

  std::reference_wrapper<provider::horizon::Deko3dGpu> gpu_;
  QueueHandle queue_;
  Allocation uniforms_;
  Deko3dPlasma::Program program_;
  std::vector<Frame> ring_;
  std::size_t next_{0};
};

}
