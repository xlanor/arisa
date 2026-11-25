#pragma once

#include <decoded_frame.hpp>
#include <frame_decoder.hpp>
#include <h264_annex_b.hpp>

#include <arisa/provider/gpu/texture.hpp>
#include <arisa/provider/horizon/deko3d_gpu.hpp>
#include <arisa/provider/horizon/deko3d_handle.hpp>

#include <deko3d.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace arisa::examples {

class Deko3dDecodedVideo {
 public:
  static constexpr std::size_t copy_ring_size = 3;
  static constexpr std::size_t held_frames = 1;
  static constexpr std::size_t samples_per_advance = 2;
  static constexpr std::uint32_t pitch_alignment = 32;

  enum class Route : std::uint8_t {
    zero_copy,
    copy,
  };

  struct Format {
    std::uint32_t width{0};
    std::uint32_t height{0};
    std::chrono::nanoseconds frame_interval{0};
    provider::ColourMatrix colour_matrix{provider::ColourMatrix::bt709};
    provider::ColourRange range{provider::ColourRange::limited};
  };

  struct Statistics {
    std::uint64_t submitted{0};
    std::uint64_t decoded{0};
    std::uint64_t shown{0};
    std::uint64_t dropped{0};
    std::uint64_t ring_waits{0};
    std::uint64_t mapped{0};
    std::chrono::nanoseconds copy_time{0};
    std::chrono::nanoseconds last_copy{0};
  };

  [[nodiscard]] static auto create(provider::horizon::Deko3dGpu& gpu,
                                   std::unique_ptr<FrameDecoder> decoder,
                                   H264AnnexB stream, Format format)
      -> std::expected<Deko3dDecodedVideo, std::string>;

  Deko3dDecodedVideo(Deko3dDecodedVideo&& other) noexcept = default;
  auto operator=(Deko3dDecodedVideo&& other) noexcept
      -> Deko3dDecodedVideo& = delete;
  Deko3dDecodedVideo(const Deko3dDecodedVideo&) = delete;
  auto operator=(const Deko3dDecodedVideo&) -> Deko3dDecodedVideo& = delete;
  ~Deko3dDecodedVideo() = default;

  [[nodiscard]] auto advance(std::chrono::nanoseconds now)
      -> std::expected<std::optional<provider::TextureId>, std::string>;

  auto route() const noexcept -> Route { return route_; }

  auto route_name() const noexcept -> std::string_view;

  auto fallback() const noexcept -> std::string_view { return fallback_; }

  auto statistics() const noexcept -> const Statistics& { return statistics_; }

  auto format() const noexcept -> const Format& { return format_; }

 private:
  using Imported = provider::horizon::Deko3dGpu::Imported;
  using MemBlockHandle =
      provider::horizon::Deko3dHandle<DkMemBlock, dkMemBlockDestroy>;

  struct Planes {
    MemBlockHandle memblock;
    dk::Image luma;
    dk::Image chroma;
    Imported texture;
  };

  struct Mapping {
    const std::byte* backing{nullptr};
    std::size_t luma_offset{0};
    std::size_t chroma_offset{0};
    Planes planes;
    std::optional<DecodedFrame> held;
  };

  struct CopySlot {
    Planes planes;
    std::size_t luma_pitch{0};
    std::size_t chroma_offset{0};
    std::size_t chroma_pitch{0};
  };

  Deko3dDecodedVideo(provider::horizon::Deko3dGpu& gpu,
                     std::unique_ptr<FrameDecoder> decoder, H264AnnexB stream,
                     Format format) noexcept;

  static auto description(const Format& format) noexcept
      -> provider::ImportDescription;

  static auto aligned(std::size_t value, std::size_t alignment) noexcept
      -> std::size_t;

  static auto block(dk::Device device, std::size_t size, void* storage)
      -> MemBlockHandle;

  static auto layout(dk::Device device, DkImageFormat format,
                     std::uint32_t width, std::uint32_t height,
                     std::size_t pitch) -> dk::ImageLayout;

  [[nodiscard]] auto planes(MemBlockHandle memblock, std::size_t luma_offset,
                            std::size_t luma_pitch, std::size_t chroma_offset,
                            std::size_t chroma_pitch)
      -> std::expected<Planes, std::string>;

  [[nodiscard]] auto map(const DecodedFrame::Resident& resident)
      -> std::expected<std::size_t, std::string>;

  [[nodiscard]] auto copy_slot() -> std::expected<CopySlot, std::string>;

  [[nodiscard]] auto feed() -> std::expected<void, std::string>;

  [[nodiscard]] auto collect() -> std::expected<void, std::string>;

  void skip_late(std::chrono::nanoseconds due);

  void reclaim();

  void fall_back(std::string why);

  [[nodiscard]] auto show_mapped(DecodedFrame frame)
      -> std::expected<std::optional<provider::TextureId>, std::string>;

  [[nodiscard]] auto show_copied(DecodedFrame& frame)
      -> std::expected<std::optional<provider::TextureId>, std::string>;

  void republish() const;

  auto sample_time(std::size_t index, std::uint64_t loop) const noexcept
      -> std::chrono::nanoseconds;

  std::reference_wrapper<provider::horizon::Deko3dGpu> gpu_;
  std::unique_ptr<FrameDecoder> decoder_;
  H264AnnexB stream_;
  Format format_;
  Route route_{Route::zero_copy};
  std::string fallback_;
  std::vector<Mapping> mappings_;
  std::vector<CopySlot> copies_;
  std::vector<DecodedFrame> waiting_;
  std::optional<std::size_t> shown_mapping_;
  std::optional<std::size_t> shown_copy_;
  std::size_t next_copy_{0};
  std::size_t next_sample_{0};
  std::uint64_t loop_{0};
  std::optional<std::chrono::nanoseconds> origin_;
  Statistics statistics_;
};

}
