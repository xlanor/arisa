#include <vulkan_decoded_video.hpp>
#include <vulkan_recorder.hpp>

#include <algorithm>
#include <array>
#include <span>
#include <utility>

namespace arisa::examples {

VulkanDecodedVideo::VulkanDecodedVideo(provider::vulkan::VulkanGpu& gpu,
                                       std::unique_ptr<FrameDecoder> decoder,
                                       H264AnnexB stream, Format format,
                                       std::vector<Slot> ring) noexcept
    : gpu_(gpu),
      decoder_(std::move(decoder)),
      stream_(std::move(stream)),
      format_(format),
      ring_(std::move(ring)) {}

VulkanDecodedVideo::~VulkanDecodedVideo() {
  if (decoder_ != nullptr) {
    gpu_.get().device().wait_idle();
  }
}

auto VulkanDecodedVideo::description(const Format& format) noexcept
    -> provider::ImportDescription {
  return provider::ImportDescription{.width = format.width,
                                     .height = format.height,
                                     .format = provider::TextureFormat::nv12,
                                     .filter = provider::SamplerFilter::linear,
                                     .premultiplied = false,
                                     .colour_matrix = format.colour_matrix,
                                     .range = format.range};
}

auto VulkanDecodedVideo::slot(provider::vulkan::VulkanGpu& gpu,
                              const Format& format)
    -> std::expected<Slot, std::string> {
  const provider::ImportDescription described = description(format);
  const provider::TextureDescription luma_plane = described.plane(0);
  const provider::TextureDescription chroma_plane = described.plane(1);
  auto luma = provider::vulkan::VulkanHostImage::create(
      gpu.device(), {.width = luma_plane.width,
                     .height = luma_plane.height,
                     .format = VK_FORMAT_R8_UNORM});
  if (!luma.has_value()) {
    return std::unexpected("the luma plane got no host image: " +
                           std::string{luma.error().description()});
  }
  auto chroma = provider::vulkan::VulkanHostImage::create(
      gpu.device(), {.width = chroma_plane.width,
                     .height = chroma_plane.height,
                     .format = VK_FORMAT_R8G8_UNORM});
  if (!chroma.has_value()) {
    return std::unexpected("the chroma plane got no host image: " +
                           std::string{chroma.error().description()});
  }
  std::ranges::fill(luma->bytes(), std::byte{16});
  std::ranges::fill(chroma->bytes(), std::byte{128});
  luma->flush();
  chroma->flush();
  const std::array planes{luma->image(), chroma->image()};
  auto imported =
      gpu.import_texture(std::span<const VkImage>{planes}, described,
                         provider::vulkan::VulkanGpu::host_import_layout);
  if (!imported.has_value()) {
    return std::unexpected("the decoder planes could not be imported: " +
                           std::string{imported.error().description()});
  }
  return Slot{.luma = std::move(*luma),
              .chroma = std::move(*chroma),
              .texture = std::move(*imported)};
}

auto VulkanDecodedVideo::prepare(const provider::vulkan::VulkanDevice& device,
                                 const std::vector<Slot>& ring)
    -> std::expected<void, std::string> {
  auto recorder = VulkanRecorder::create(device);
  if (!recorder.has_value()) {
    return std::unexpected(recorder.error());
  }
  auto commands = recorder->begin();
  if (!commands.has_value()) {
    return std::unexpected(commands.error());
  }
  for (const Slot& target : ring) {
    VulkanRecorder::to_host_sampled(*commands, target.luma.image());
    VulkanRecorder::to_host_sampled(*commands, target.chroma.image());
  }
  return recorder->submit(provider::vulkan::ReadySemaphore{});
}

auto VulkanDecodedVideo::create(provider::vulkan::VulkanGpu& gpu,
                                std::unique_ptr<FrameDecoder> decoder,
                                H264AnnexB stream, Format format)
    -> std::expected<VulkanDecodedVideo, std::string> {
  if (decoder == nullptr || format.width == 0 || format.height == 0 ||
      format.frame_interval <= std::chrono::nanoseconds{0}) {
    return std::unexpected(std::string{"the decoded video has no decoder"});
  }
  std::vector<Slot> ring;
  ring.reserve(ring_size);
  for (std::size_t index = 0; index < ring_size; ++index) {
    auto made = slot(gpu, format);
    if (!made.has_value()) {
      return std::unexpected(made.error());
    }
    ring.push_back(std::move(*made));
  }
  if (auto prepared = prepare(gpu.device(), ring); !prepared.has_value()) {
    return std::unexpected(prepared.error());
  }
  return VulkanDecodedVideo{gpu, std::move(decoder), std::move(stream), format,
                            std::move(ring)};
}

auto VulkanDecodedVideo::sample_time(std::size_t index,
                                     std::uint64_t loop) const noexcept
    -> std::chrono::nanoseconds {
  const std::uint64_t ordinal = (loop * stream_.access_units()) + index;
  return format_.frame_interval * static_cast<std::int64_t>(ordinal);
}

auto VulkanDecodedVideo::feed() -> std::expected<void, std::string> {
  for (std::size_t count = 0; count < samples_per_advance; ++count) {
    auto submitted = decoder_->submit(stream_.access_unit(next_sample_),
                                      sample_time(next_sample_, loop_));
    if (!submitted.has_value()) {
      return std::unexpected(submitted.error());
    }
    if (*submitted == FrameDecoder::Submission::busy) {
      return {};
    }
    ++statistics_.submitted;
    if (++next_sample_ == stream_.access_units()) {
      next_sample_ = 0;
      ++loop_;
    }
  }
  return {};
}

auto VulkanDecodedVideo::collect() -> std::expected<void, std::string> {
  while (waiting_.size() < held_frames) {
    auto frame = decoder_->next_frame();
    if (!frame.has_value()) {
      return std::unexpected(frame.error());
    }
    if (!frame->has_value()) {
      return {};
    }
    ++statistics_.decoded;
    waiting_.push_back(std::move(**frame));
  }
  return {};
}

void VulkanDecodedVideo::skip_late(std::chrono::nanoseconds due) {
  while (waiting_.size() > 1 && waiting_[1].picture().timestamp <= due) {
    waiting_.erase(waiting_.begin());
    ++statistics_.dropped;
    if (!collect().has_value()) {
      return;
    }
  }
}

auto VulkanDecodedVideo::show(Slot& target, DecodedFrame& frame)
    -> std::expected<void, std::string> {
  if (frame.picture().width != format_.width ||
      frame.picture().height != format_.height) {
    return std::unexpected(
        std::string{"the decoder produced a frame of another size"});
  }
  const auto started = std::chrono::steady_clock::now();
  const DecodedFrame::Planes planes{.luma = target.luma.bytes(),
                                    .luma_pitch = target.luma.row_pitch(),
                                    .chroma = target.chroma.bytes(),
                                    .chroma_pitch = target.chroma.row_pitch()};
  if (auto copied = frame.copy_to(planes); !copied.has_value()) {
    return std::unexpected(copied.error());
  }
  target.luma.flush();
  target.chroma.flush();
  const auto took = std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::steady_clock::now() - started);
  statistics_.copy_time += took;
  statistics_.last_copy = took;
  target.texture.publish();
  return {};
}

auto VulkanDecodedVideo::advance(std::chrono::nanoseconds now)
    -> std::expected<std::optional<provider::TextureId>, std::string> {
  const Slot& current = ring_[shown_slot_.value_or(0)];
  if (auto fed = feed(); !fed.has_value()) {
    return std::unexpected(fed.error());
  }
  if (auto collected = collect(); !collected.has_value()) {
    return std::unexpected(collected.error());
  }
  if (waiting_.empty()) {
    current.texture.publish();
    return std::nullopt;
  }
  if (!origin_.has_value()) {
    origin_ = now - waiting_.front().picture().timestamp;
  }
  const std::chrono::nanoseconds due = now - *origin_;
  skip_late(due);
  if (waiting_.front().picture().timestamp > due) {
    current.texture.publish();
    return std::nullopt;
  }
  Slot& target = ring_[next_slot_];
  if (target.texture.in_use()) {
    ++statistics_.ring_waits;
    current.texture.publish();
    return std::nullopt;
  }
  auto shown = show(target, waiting_.front());
  waiting_.erase(waiting_.begin());
  if (!shown.has_value()) {
    return std::unexpected(shown.error());
  }
  shown_slot_ = next_slot_;
  next_slot_ = (next_slot_ + 1) % ring_.size();
  ++statistics_.shown;
  return target.texture.id();
}

}
