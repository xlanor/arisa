#include <deko3d_decoded_video.hpp>

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <span>
#include <utility>

namespace arisa::examples {

Deko3dDecodedVideo::Deko3dDecodedVideo(provider::horizon::Deko3dGpu& gpu,
                                       std::unique_ptr<FrameDecoder> decoder,
                                       H264AnnexB stream,
                                       Format format) noexcept
    : gpu_(gpu),
      decoder_(std::move(decoder)),
      stream_(std::move(stream)),
      format_(format) {}

auto Deko3dDecodedVideo::create(provider::horizon::Deko3dGpu& gpu,
                                std::unique_ptr<FrameDecoder> decoder,
                                H264AnnexB stream, Format format)
    -> std::expected<Deko3dDecodedVideo, std::string> {
  if (decoder == nullptr || format.width == 0 || format.height == 0 ||
      format.frame_interval <= std::chrono::nanoseconds{0}) {
    return std::unexpected(std::string{"the decoded video has no decoder"});
  }
  return Deko3dDecodedVideo{gpu, std::move(decoder), std::move(stream), format};
}

auto Deko3dDecodedVideo::route_name() const noexcept -> std::string_view {
  return route_ == Route::zero_copy ? "zero-copy" : "copy";
}

auto Deko3dDecodedVideo::description(const Format& format) noexcept
    -> provider::ImportDescription {
  return provider::ImportDescription{.width = format.width,
                                     .height = format.height,
                                     .format = provider::TextureFormat::nv12,
                                     .filter = provider::SamplerFilter::linear,
                                     .premultiplied = false,
                                     .colour_matrix = format.colour_matrix,
                                     .range = format.range};
}

auto Deko3dDecodedVideo::aligned(std::size_t value,
                                 std::size_t alignment) noexcept
    -> std::size_t {
  return (value + alignment - 1) / alignment * alignment;
}

auto Deko3dDecodedVideo::block(dk::Device device, std::size_t size,
                               void* storage) -> MemBlockHandle {
  DkMemBlockMaker maker;
  dkMemBlockMakerDefaults(&maker, device, static_cast<std::uint32_t>(size));
  maker.flags = DkMemBlockFlags_CpuUncached | DkMemBlockFlags_GpuCached |
                DkMemBlockFlags_Image;
  maker.storage = storage;
  return MemBlockHandle{dkMemBlockCreate(&maker)};
}

auto Deko3dDecodedVideo::layout(dk::Device device, DkImageFormat format,
                                std::uint32_t width, std::uint32_t height,
                                std::size_t pitch) -> dk::ImageLayout {
  dk::ImageLayout made;
  dk::ImageLayoutMaker{device}
      .setFlags(DkImageFlags_PitchLinear)
      .setFormat(format)
      .setDimensions(width, height)
      .setPitchStride(static_cast<std::uint32_t>(pitch))
      .initialize(made);
  return made;
}

auto Deko3dDecodedVideo::planes(MemBlockHandle memblock,
                                std::size_t luma_offset, std::size_t luma_pitch,
                                std::size_t chroma_offset,
                                std::size_t chroma_pitch)
    -> std::expected<Planes, std::string> {
  const provider::ImportDescription described = description(format_);
  const provider::TextureDescription chroma_plane = described.plane(1);
  const dk::Device device = gpu_.get().device();
  const dk::ImageLayout luma_layout =
      layout(device, DkImageFormat_R8_Unorm, format_.width, format_.height,
             luma_pitch);
  const dk::ImageLayout chroma_layout =
      layout(device, DkImageFormat_RG8_Unorm, chroma_plane.width,
             chroma_plane.height, chroma_pitch);
  if (luma_offset % luma_layout.getAlignment() != 0 ||
      chroma_offset % chroma_layout.getAlignment() != 0) {
    return std::unexpected(
        std::string{"the decoder planes are not aligned for the GPU"});
  }
  dk::Image luma;
  luma.initialize(luma_layout, memblock.get(),
                  static_cast<std::uint32_t>(luma_offset));
  dk::Image chroma;
  chroma.initialize(chroma_layout, memblock.get(),
                    static_cast<std::uint32_t>(chroma_offset));
  const std::array<const dk::Image*, 2> both{&luma, &chroma};
  auto imported = gpu_.get().import_texture(
      std::span<const dk::Image* const>{both}, described);
  if (!imported.has_value()) {
    return std::unexpected("the decoder planes could not be imported: " +
                           std::string{imported.error().description()});
  }
  return Planes{.memblock = std::move(memblock),
                .luma = luma,
                .chroma = chroma,
                .texture = std::move(*imported)};
}

auto Deko3dDecodedVideo::map(const DecodedFrame::Resident& resident)
    -> std::expected<std::size_t, std::string> {
  const auto found =
      std::ranges::find(mappings_, resident.backing, &Mapping::backing);
  if (found != mappings_.end() && found->luma_offset == resident.luma_offset &&
      found->chroma_offset == resident.chroma_offset) {
    return static_cast<std::size_t>(found - mappings_.begin());
  }
  const auto address = reinterpret_cast<std::uintptr_t>(resident.backing);
  if (address % DK_MEMBLOCK_ALIGNMENT != 0 ||
      resident.backing_size % DK_MEMBLOCK_ALIGNMENT != 0 ||
      resident.backing_size > std::numeric_limits<std::uint32_t>::max()) {
    return std::unexpected(
        std::string{"the decoder image is not page aligned"});
  }
  MemBlockHandle mapped = block(gpu_.get().device(), resident.backing_size,
                                const_cast<std::byte*>(resident.backing));
  if (!mapped.valid()) {
    return std::unexpected(
        std::string{"the decoder image could not be mapped for the GPU"});
  }
  auto made =
      planes(std::move(mapped), resident.luma_offset, resident.luma_pitch,
             resident.chroma_offset, resident.chroma_pitch);
  if (!made.has_value()) {
    return std::unexpected(made.error());
  }
  ++statistics_.mapped;
  mappings_.push_back(Mapping{.backing = resident.backing,
                              .luma_offset = resident.luma_offset,
                              .chroma_offset = resident.chroma_offset,
                              .planes = std::move(*made),
                              .held = std::nullopt});
  return mappings_.size() - 1;
}

auto Deko3dDecodedVideo::copy_slot() -> std::expected<CopySlot, std::string> {
  const provider::TextureDescription chroma_plane =
      description(format_).plane(1);
  const std::size_t luma_pitch = aligned(format_.width, pitch_alignment);
  const std::size_t chroma_pitch = aligned(
      static_cast<std::size_t>(chroma_plane.width) * 2, pitch_alignment);
  const std::size_t chroma_offset =
      aligned(luma_pitch * format_.height, DK_IMAGE_LINEAR_STRIDE_ALIGNMENT);
  const std::size_t size =
      aligned(chroma_offset + (chroma_pitch * chroma_plane.height),
              DK_MEMBLOCK_ALIGNMENT);
  if (size > std::numeric_limits<std::uint32_t>::max()) {
    return std::unexpected(std::string{"the copied frame is too large"});
  }
  MemBlockHandle owned = block(gpu_.get().device(), size, nullptr);
  if (!owned.valid()) {
    return std::unexpected(
        std::string{"the copied frame could not get GPU memory"});
  }
  auto made =
      planes(std::move(owned), 0, luma_pitch, chroma_offset, chroma_pitch);
  if (!made.has_value()) {
    return std::unexpected(made.error());
  }
  return CopySlot{.planes = std::move(*made),
                  .luma_pitch = luma_pitch,
                  .chroma_offset = chroma_offset,
                  .chroma_pitch = chroma_pitch};
}

auto Deko3dDecodedVideo::sample_time(std::size_t index,
                                     std::uint64_t loop) const noexcept
    -> std::chrono::nanoseconds {
  const std::uint64_t ordinal = (loop * stream_.access_units()) + index;
  return format_.frame_interval * static_cast<std::int64_t>(ordinal);
}

auto Deko3dDecodedVideo::feed() -> std::expected<void, std::string> {
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

auto Deko3dDecodedVideo::collect() -> std::expected<void, std::string> {
  while (waiting_.size() < held_frames + 1) {
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

void Deko3dDecodedVideo::skip_late(std::chrono::nanoseconds due) {
  while (waiting_.size() > 1 && waiting_[1].picture().timestamp <= due) {
    waiting_.erase(waiting_.begin());
    ++statistics_.dropped;
    if (!collect().has_value()) {
      return;
    }
  }
}

void Deko3dDecodedVideo::reclaim() {
  for (std::size_t index = 0; index < mappings_.size(); ++index) {
    Mapping& mapping = mappings_[index];
    if (shown_mapping_ != index && mapping.held.has_value() &&
        !mapping.planes.texture.in_use()) {
      mapping.held.reset();
    }
  }
}

void Deko3dDecodedVideo::fall_back(std::string why) {
  route_ = Route::copy;
  fallback_ = std::move(why);
  shown_mapping_.reset();
}

void Deko3dDecodedVideo::republish() const {
  if (shown_mapping_.has_value()) {
    mappings_[*shown_mapping_].planes.texture.publish();
  } else if (shown_copy_.has_value()) {
    copies_[*shown_copy_].planes.texture.publish();
  }
}

auto Deko3dDecodedVideo::show_mapped(DecodedFrame frame)
    -> std::expected<std::optional<provider::TextureId>, std::string> {
  auto index = map(*frame.resident());
  if (!index.has_value()) {
    fall_back(index.error());
    return show_copied(frame);
  }
  Mapping& target = mappings_[*index];
  if (target.held.has_value()) {
    ++statistics_.ring_waits;
    republish();
    return std::nullopt;
  }
  target.held.emplace(std::move(frame));
  target.planes.texture.publish();
  shown_mapping_ = *index;
  ++statistics_.shown;
  return target.planes.texture.id();
}

auto Deko3dDecodedVideo::show_copied(DecodedFrame& frame)
    -> std::expected<std::optional<provider::TextureId>, std::string> {
  while (copies_.size() < copy_ring_size) {
    auto made = copy_slot();
    if (!made.has_value()) {
      return std::unexpected(made.error());
    }
    copies_.push_back(std::move(*made));
  }
  CopySlot& target = copies_[next_copy_];
  if (target.planes.texture.in_use()) {
    ++statistics_.ring_waits;
    republish();
    return std::nullopt;
  }
  const auto started = std::chrono::steady_clock::now();
  auto* const base = static_cast<std::byte*>(
      dkMemBlockGetCpuAddr(target.planes.memblock.get()));
  const std::size_t size = dkMemBlockGetSize(target.planes.memblock.get());
  const DecodedFrame::Planes destination{
      .luma = std::span<std::byte>{base, target.chroma_offset},
      .luma_pitch = target.luma_pitch,
      .chroma = std::span<std::byte>{base + target.chroma_offset,
                                     size - target.chroma_offset},
      .chroma_pitch = target.chroma_pitch};
  if (auto copied = frame.copy_to(destination); !copied.has_value()) {
    return std::unexpected(copied.error());
  }
  const auto took = std::chrono::duration_cast<std::chrono::nanoseconds>(
      std::chrono::steady_clock::now() - started);
  statistics_.copy_time += took;
  statistics_.last_copy = took;
  target.planes.texture.publish();
  shown_copy_ = next_copy_;
  next_copy_ = (next_copy_ + 1) % copies_.size();
  ++statistics_.shown;
  return target.planes.texture.id();
}

auto Deko3dDecodedVideo::advance(std::chrono::nanoseconds now)
    -> std::expected<std::optional<provider::TextureId>, std::string> {
  reclaim();
  if (auto fed = feed(); !fed.has_value()) {
    return std::unexpected(fed.error());
  }
  if (auto collected = collect(); !collected.has_value()) {
    return std::unexpected(collected.error());
  }
  if (waiting_.empty()) {
    republish();
    return std::nullopt;
  }
  if (!origin_.has_value()) {
    origin_ = now - waiting_.front().picture().timestamp;
  }
  const std::chrono::nanoseconds due = now - *origin_;
  skip_late(due);
  DecodedFrame& next = waiting_.front();
  if (next.picture().timestamp > due) {
    republish();
    return std::nullopt;
  }
  if (next.picture().width != format_.width ||
      next.picture().height != format_.height) {
    return std::unexpected(
        std::string{"the decoder produced a frame of another size"});
  }
  if (route_ == Route::zero_copy && !next.resident().has_value()) {
    fall_back("the decoder frame is not resident in CPU memory");
  }
  std::expected<std::optional<provider::TextureId>, std::string> shown =
      std::nullopt;
  if (route_ == Route::zero_copy) {
    DecodedFrame taken = std::move(next);
    waiting_.erase(waiting_.begin());
    shown = show_mapped(std::move(taken));
  } else {
    shown = show_copied(next);
    if (shown.has_value() && shown->has_value()) {
      waiting_.erase(waiting_.begin());
    }
  }
  return shown;
}

}
