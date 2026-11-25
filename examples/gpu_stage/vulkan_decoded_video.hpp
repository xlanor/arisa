#pragma once

#include <decoded_frame.hpp>
#include <frame_decoder.hpp>
#include <h264_annex_b.hpp>

#include <arisa/provider/gpu/texture.hpp>
#include <arisa/provider/vulkan/vulkan_gpu.hpp>
#include <arisa/provider/vulkan/vulkan_host_image.hpp>

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

class VulkanDecodedVideo {
 public:
  static constexpr std::size_t ring_size = 3;
  static constexpr std::size_t held_frames = 2;
  static constexpr std::size_t samples_per_advance = 2;
  static constexpr std::string_view route = "copy";

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
    std::chrono::nanoseconds copy_time{0};
    std::chrono::nanoseconds last_copy{0};
  };

  [[nodiscard]] static auto create(provider::vulkan::VulkanGpu& gpu,
                                   std::unique_ptr<FrameDecoder> decoder,
                                   H264AnnexB stream, Format format)
      -> std::expected<VulkanDecodedVideo, std::string>;

  VulkanDecodedVideo(VulkanDecodedVideo&& other) noexcept = default;
  auto operator=(VulkanDecodedVideo&& other) noexcept
      -> VulkanDecodedVideo& = delete;
  VulkanDecodedVideo(const VulkanDecodedVideo&) = delete;
  auto operator=(const VulkanDecodedVideo&) -> VulkanDecodedVideo& = delete;
  ~VulkanDecodedVideo();

  [[nodiscard]] auto advance(std::chrono::nanoseconds now)
      -> std::expected<std::optional<provider::TextureId>, std::string>;

  auto route_name() const noexcept -> std::string_view { return route; }

  auto fallback() const noexcept -> std::string_view { return {}; }

  auto statistics() const noexcept -> const Statistics& { return statistics_; }

  auto format() const noexcept -> const Format& { return format_; }

 private:
  struct Slot {
    provider::vulkan::VulkanHostImage luma;
    provider::vulkan::VulkanHostImage chroma;
    provider::vulkan::VulkanGpu::Imported texture;
  };

  VulkanDecodedVideo(provider::vulkan::VulkanGpu& gpu,
                     std::unique_ptr<FrameDecoder> decoder, H264AnnexB stream,
                     Format format, std::vector<Slot> ring) noexcept;

  [[nodiscard]] static auto slot(provider::vulkan::VulkanGpu& gpu,
                                 const Format& format)
      -> std::expected<Slot, std::string>;

  [[nodiscard]] static auto prepare(
      const provider::vulkan::VulkanDevice& device,
      const std::vector<Slot>& ring) -> std::expected<void, std::string>;

  static auto description(const Format& format) noexcept
      -> provider::ImportDescription;

  [[nodiscard]] auto feed() -> std::expected<void, std::string>;

  [[nodiscard]] auto collect() -> std::expected<void, std::string>;

  void skip_late(std::chrono::nanoseconds due);

  [[nodiscard]] auto show(Slot& target, DecodedFrame& frame)
      -> std::expected<void, std::string>;

  auto sample_time(std::size_t index, std::uint64_t loop) const noexcept
      -> std::chrono::nanoseconds;

  std::reference_wrapper<provider::vulkan::VulkanGpu> gpu_;
  std::unique_ptr<FrameDecoder> decoder_;
  H264AnnexB stream_;
  Format format_;
  std::vector<Slot> ring_;
  std::vector<DecodedFrame> waiting_;
  std::size_t next_sample_{0};
  std::uint64_t loop_{0};
  std::size_t next_slot_{0};
  std::optional<std::size_t> shown_slot_;
  std::optional<std::chrono::nanoseconds> origin_;
  Statistics statistics_;
};

}
