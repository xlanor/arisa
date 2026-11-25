#pragma once

#include <deko3d_plasma.hpp>
#include <stage_control.hpp>

#include <arisa/provider/horizon/deko3d_gpu.hpp>
#include <arisa/provider/horizon/deko3d_handle.hpp>
#include <arisa/provider/horizon/deko3d_memory.hpp>

#include <deko3d.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <vector>

namespace arisa::examples {

class Deko3dVideo {
 public:
  static constexpr std::size_t ring_size = 3;
  static constexpr std::uint32_t width = 640;
  static constexpr std::uint32_t height = 360;
  static constexpr std::uint32_t command_size = 0x4000;

  [[nodiscard]] static auto create(provider::horizon::Deko3dGpu& gpu,
                                   const Deko3dPlasma& plasma)
      -> std::expected<Deko3dVideo, std::string>;

  Deko3dVideo(Deko3dVideo&& other) noexcept = default;
  auto operator=(Deko3dVideo&& other) noexcept -> Deko3dVideo& = default;
  Deko3dVideo(const Deko3dVideo&) = delete;
  auto operator=(const Deko3dVideo&) -> Deko3dVideo& = delete;
  ~Deko3dVideo();

  auto produce(StageControl& control, std::chrono::nanoseconds now) -> bool;

 private:
  using Allocation = provider::horizon::Deko3dMemory::Allocation;
  using QueueHandle = provider::horizon::Deko3dHandle<DkQueue, dkQueueDestroy>;
  using CmdBufHandle =
      provider::horizon::Deko3dHandle<DkCmdBuf, dkCmdBufDestroy>;

  struct Frame {
    dk::Image image;
    Allocation command_memory;
    CmdBufHandle commands;
    dk::Fence done;
    bool pending{false};
    provider::horizon::Deko3dGpu::Imported texture;
  };

  Deko3dVideo(QueueHandle queue, Allocation uniforms,
              Deko3dPlasma::Program program, std::vector<Frame> ring) noexcept;

  QueueHandle queue_;
  Allocation uniforms_;
  Deko3dPlasma::Program program_;
  std::vector<Frame> ring_;
  std::size_t next_{0};
};

}
