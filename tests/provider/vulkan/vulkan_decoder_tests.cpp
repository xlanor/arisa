#include <arisa/provider/gpu/colour.hpp>
#include <arisa/provider/gpu/draw_data.hpp>
#include <arisa/provider/gpu/shape_vertex.hpp>
#include <arisa/provider/gpu/texture.hpp>
#include <arisa/provider/gpu/vertex.hpp>
#include <arisa/provider/gpu/yuv_conversion.hpp>
#include <arisa/provider/surface/surface_size.hpp>
#include <arisa/provider/vulkan/ready_semaphore.hpp>
#include <arisa/provider/vulkan/vulkan_device.hpp>
#include <arisa/provider/vulkan/vulkan_gpu.hpp>
#include <arisa/provider/vulkan/vulkan_host_image.hpp>
#include <arisa/support/scoped_callback.hpp>

#include <decoded_frame.hpp>
#include <frame_decoder.hpp>
#include <h264_annex_b.hpp>
#include <vulkan_decoded_video.hpp>
#include <vulkan_recorder.hpp>

#include "vulkan_test.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <initializer_list>
#include <memory>
#include <optional>
#include <print>
#include <span>
#include <thread>
#include <utility>
#include <vector>

namespace arisa::provider::vulkan {

using namespace std::chrono_literals;

class VulkanDecoderTest : public VulkanTest {
 protected:
  static constexpr SurfaceSize small{.width = SurfaceWidth{64},
                                     .height = SurfaceHeight{48}};
  static constexpr std::uint32_t frame_width = 64;
  static constexpr std::uint32_t frame_height = 48;
  static constexpr std::chrono::nanoseconds interval{33'333'333};
  static constexpr std::array<std::uint8_t, 4> luma_codes{16, 81, 145, 235};
  static constexpr std::chrono::seconds completion_wait{5};

  struct Ledger {
    std::size_t outstanding{0};
    std::size_t most_outstanding{0};
    std::size_t released{0};
    std::vector<std::size_t> sample_sizes;
  };

  class FakeDecoder final : public examples::FrameDecoder {
   public:
    FakeDecoder(std::size_t capacity, std::shared_ptr<Ledger> ledger)
        : capacity_(capacity), ledger_(std::move(ledger)) {}

    auto submit(std::span<const std::byte> sample,
                std::chrono::nanoseconds timestamp)
        -> std::expected<Submission, std::string> override {
      if (ledger_->outstanding + queued_.size() >= capacity_) {
        return Submission::busy;
      }
      ledger_->sample_sizes.push_back(sample.size());
      queued_.push_back(timestamp);
      return Submission::accepted;
    }

    auto next_frame() -> std::expected<std::optional<examples::DecodedFrame>,
                                       std::string> override {
      if (queued_.empty()) {
        return std::nullopt;
      }
      const std::chrono::nanoseconds timestamp = queued_.front();
      queued_.pop_front();
      const std::uint8_t code = luma_codes[produced_++ % luma_codes.size()];
      ++ledger_->outstanding;
      ledger_->most_outstanding =
          std::max(ledger_->most_outstanding, ledger_->outstanding);
      examples::DecodedFrame::Copy copy =
          [code](const examples::DecodedFrame::Planes& planes)
          -> std::expected<void, std::string> {
        for (std::size_t row = 0; row < frame_height; ++row) {
          std::ranges::fill(
              planes.luma.subspan(row * planes.luma_pitch, frame_width),
              std::byte{code});
        }
        for (std::size_t row = 0; row < frame_height / 2; ++row) {
          std::ranges::fill(
              planes.chroma.subspan(row * planes.chroma_pitch, frame_width),
              std::byte{128});
        }
        return {};
      };
      support::ScopedCallback release{[ledger = ledger_]() noexcept {
        --ledger->outstanding;
        ++ledger->released;
      }};
      return examples::DecodedFrame{
          examples::DecodedFrame::Picture{.width = frame_width,
                                          .height = frame_height,
                                          .timestamp = timestamp},
          std::move(copy), std::move(release)};
    }

   private:
    std::size_t capacity_;
    std::shared_ptr<Ledger> ledger_;
    std::deque<std::chrono::nanoseconds> queued_;
    std::size_t produced_{0};
  };

  static auto sample_path() -> std::filesystem::path {
    return std::filesystem::path{ARISA_GPU_STAGE_MEDIA} / "stage.h264";
  }

  static auto bytes(std::initializer_list<int> values)
      -> std::vector<std::byte> {
    std::vector<std::byte> out;
    out.reserve(values.size());
    for (const int value : values) {
      out.push_back(static_cast<std::byte>(value));
    }
    return out;
  }

  static auto grey(std::uint8_t code) -> int {
    const YuvConversion conversion{ColourMatrix::bt709, ColourRange::limited};
    const YuvConversion::Rgb rgb = conversion.rgb(
        static_cast<float>(code) / 255.0F, 128.0F / 255.0F, 128.0F / 255.0F);
    return static_cast<int>(std::lround(rgb[0] * 255.0F));
  }

  static void show_yuv(VulkanGpu& gpu, TextureId texture) {
    ASSERT_TRUE(gpu.publications().has_value());
    const auto published = gpu.publications()->get().find(texture);
    ASSERT_TRUE(published.has_value());
    ASSERT_TRUE(published->planar());
    const ShapeVertex base{.colour = 0xFFFFFFFFU,
                           .kind = ShapeKind::yuv,
                           .effect = published->conversion.levels(),
                           .detail = published->conversion.weights()};
    std::array<ShapeVertex, 4> vertices{base, base, base, base};
    vertices[0].position = {0.0F, 0.0F};
    vertices[0].uv = {0.0F, 0.0F};
    vertices[1].position = {64.0F, 0.0F};
    vertices[1].uv = {1.0F, 0.0F};
    vertices[2].position = {64.0F, 48.0F};
    vertices[2].uv = {1.0F, 1.0F};
    vertices[3].position = {0.0F, 48.0F};
    vertices[3].uv = {0.0F, 1.0F};
    const std::array<Index, 6> indices{0, 1, 2, 0, 2, 3};
    const std::array commands{
        DrawCommand{.texture = texture,
                    .clip = ClipRect{.x = 0, .y = 0, .width = 64, .height = 48},
                    .index_offset = 0,
                    .index_count = 6,
                    .vertex_offset = 0,
                    .detail = published->chroma}};
    auto frame = gpu.begin_frame();
    ASSERT_TRUE(frame.has_value());
    frame->clear(Colour{.alpha = 1.0F});
    auto geometry = frame->allocate_shapes(vertices.size(), indices.size());
    ASSERT_TRUE(geometry.has_value());
    std::ranges::copy(vertices, geometry->vertices.begin());
    std::ranges::copy(indices, geometry->indices.begin());
    ASSERT_TRUE(
        frame->draw(ShapeDrawData{.geometry = *geometry, .commands = commands})
            .has_value());
    ASSERT_TRUE(frame->present().has_value());
  }

  static auto finish(VulkanGpu& gpu) -> bool {
    const GpuSerial serial{gpu.statistics().frames_presented};
    const auto deadline = std::chrono::steady_clock::now() + completion_wait;
    while (!gpu.completed(serial) &&
           std::chrono::steady_clock::now() < deadline) {
      std::this_thread::yield();
    }
    return gpu.completed(serial);
  }

  static auto red_at(VulkanGpu& gpu, std::uint32_t x, std::uint32_t y) -> int {
    const auto image = gpu.capture();
    EXPECT_TRUE(image.has_value());
    if (!image.has_value()) {
      return -1;
    }
    const auto seen = image->pixel(x, y);
    EXPECT_TRUE(seen.has_value());
    return seen.has_value() ? static_cast<int>((*seen)[0]) : -1;
  }
};

TEST_F(VulkanDecoderTest, TheStageSampleSplitsIntoOneAccessUnitPerPicture) {
  const auto stream = examples::H264AnnexB::load(sample_path());
  ASSERT_TRUE(stream.has_value()) << stream.error();
  EXPECT_EQ(stream->access_units(), 120U);
  const std::vector<std::span<const std::byte>> sets =
      examples::H264AnnexB::nal_units(stream->parameter_sets());
  ASSERT_EQ(sets.size(), 2U);
  EXPECT_EQ(examples::H264AnnexB::nal_type(sets[0]), 7U);
  EXPECT_EQ(examples::H264AnnexB::nal_type(sets[1]), 8U);
  const std::array<std::byte, 4> start{std::byte{0}, std::byte{0}, std::byte{0},
                                       std::byte{1}};
  EXPECT_TRUE(std::ranges::equal(stream->parameter_sets().first(4), start));
  std::size_t keyframes = 0;
  for (std::size_t index = 0; index < stream->access_units(); ++index) {
    const std::span<const std::byte> unit = stream->access_unit(index);
    ASSERT_GT(unit.size(), 4U);
    EXPECT_TRUE(std::ranges::equal(unit.first(4), start));
    const std::vector<std::span<const std::byte>> nals =
        examples::H264AnnexB::nal_units(unit);
    ASSERT_FALSE(nals.empty());
    EXPECT_EQ(examples::H264AnnexB::nal_type(nals.front()), 9U);
    const auto pictures = std::ranges::count_if(nals, [](const auto& nal) {
      const std::uint8_t type = examples::H264AnnexB::nal_type(nal);
      return type == 1 || type == 5;
    });
    EXPECT_EQ(pictures, 1);
    if (stream->keyframe(index)) {
      ++keyframes;
      EXPECT_EQ(index % 60, 0U);
    }
  }
  EXPECT_EQ(keyframes, 2U);
  EXPECT_LT(std::filesystem::file_size(sample_path()), 1U << 20U);
}

TEST_F(VulkanDecoderTest, AccessUnitsSplitOnPicturesAndNonPictureUnits) {
  const std::vector<std::byte> stream =
      bytes({0x00, 0x00, 0x01, 0x67, 0x42, 0x00, 0x1E, 0xAB, 0x00, 0x00,
             0x00, 0x01, 0x68, 0xCE, 0x3C, 0x80, 0x00, 0x00, 0x01, 0x65,
             0x88, 0x84, 0x21, 0x00, 0x00, 0x01, 0x41, 0x9A, 0x80, 0x00,
             0x00, 0x01, 0x41, 0x1A, 0x80, 0x00, 0x00, 0x01, 0x06, 0x05,
             0x01, 0x80, 0x00, 0x00, 0x01, 0x41, 0x9B, 0x80, 0x00, 0x00});
  const auto parsed = examples::H264AnnexB::parse(stream);
  ASSERT_TRUE(parsed.has_value()) << parsed.error();
  EXPECT_TRUE(std::ranges::equal(
      parsed->parameter_sets(),
      bytes({0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0x00, 0x1E, 0xAB, 0x00, 0x00,
             0x00, 0x01, 0x68, 0xCE, 0x3C, 0x80})));
  ASSERT_EQ(parsed->access_units(), 3U);
  EXPECT_TRUE(parsed->keyframe(0));
  EXPECT_FALSE(parsed->keyframe(1));
  EXPECT_FALSE(parsed->keyframe(2));
  EXPECT_EQ(examples::H264AnnexB::nal_units(parsed->access_unit(0)).size(), 3U);
  EXPECT_TRUE(
      std::ranges::equal(parsed->access_unit(1),
                         bytes({0x00, 0x00, 0x00, 0x01, 0x41, 0x9A, 0x80, 0x00,
                                0x00, 0x00, 0x01, 0x41, 0x1A, 0x80})));
  EXPECT_TRUE(
      std::ranges::equal(parsed->access_unit(2),
                         bytes({0x00, 0x00, 0x00, 0x01, 0x06, 0x05, 0x01, 0x80,
                                0x00, 0x00, 0x00, 0x01, 0x41, 0x9B, 0x80})));
  EXPECT_TRUE(parsed->access_unit(3).empty());
  EXPECT_FALSE(
      examples::H264AnnexB::parse(bytes({0x00, 0x00, 0x01, 0x65, 0x88, 0x84}))
          .has_value());
  EXPECT_FALSE(
      examples::H264AnnexB::parse(
          bytes({0x00, 0x00, 0x01, 0x67, 0x42, 0x00, 0x00, 0x01, 0x68, 0xCE}))
          .has_value());
}

TEST_F(VulkanDecoderTest, DecodedFramesRefuseSmallPlanesAndReleaseOnce) {
  int released = 0;
  int copies = 0;
  {
    examples::DecodedFrame frame{
        examples::DecodedFrame::Picture{
            .width = 4, .height = 4, .timestamp = 0ns},
        [&copies](const examples::DecodedFrame::Planes&)
            -> std::expected<void, std::string> {
          ++copies;
          return {};
        },
        support::ScopedCallback{[&released]() noexcept { ++released; }}};
    std::array<std::byte, 16> luma{};
    std::array<std::byte, 8> chroma{};
    EXPECT_TRUE(frame
                    .copy_to({.luma = luma,
                              .luma_pitch = 4,
                              .chroma = chroma,
                              .chroma_pitch = 4})
                    .has_value());
    EXPECT_FALSE(frame
                     .copy_to({.luma = std::span{luma}.first(15),
                               .luma_pitch = 4,
                               .chroma = chroma,
                               .chroma_pitch = 4})
                     .has_value());
    EXPECT_FALSE(frame
                     .copy_to({.luma = luma,
                               .luma_pitch = 3,
                               .chroma = chroma,
                               .chroma_pitch = 4})
                     .has_value());
    EXPECT_FALSE(frame
                     .copy_to({.luma = luma,
                               .luma_pitch = 4,
                               .chroma = std::span{chroma}.first(7),
                               .chroma_pitch = 4})
                     .has_value());
    std::vector<examples::DecodedFrame> moved;
    moved.push_back(std::move(frame));
    EXPECT_EQ(released, 0);
  }
  EXPECT_EQ(copies, 1);
  EXPECT_EQ(released, 1);
}

TEST_F(VulkanDecoderTest, HostImagesSampleInPlaceInTheGeneralLayout) {
  auto gpu = offscreen({.size = small, .images = 2});
  if (!gpu.has_value()) {
    GTEST_SKIP() << skip_reason(gpu.error());
  }
  const VulkanDevice& device = gpu->device();
  auto luma = VulkanHostImage::create(
      device, {.width = 64, .height = 48, .format = VK_FORMAT_R8_UNORM});
  auto chroma = VulkanHostImage::create(
      device, {.width = 32, .height = 24, .format = VK_FORMAT_R8G8_UNORM});
  if (!luma.has_value() || !chroma.has_value()) {
    GTEST_SKIP() << "the device cannot sample linear host images";
  }
  EXPECT_GE(luma->row_pitch(), 64U);
  EXPECT_GE(chroma->row_pitch(), 64U);
  EXPECT_FALSE(
      VulkanHostImage::create(
          device, {.width = 0, .height = 48, .format = VK_FORMAT_R8_UNORM})
          .has_value());
  constexpr std::uint8_t left = 50;
  constexpr std::uint8_t right = 200;
  for (std::size_t row = 0; row < 48; ++row) {
    const std::span<std::byte> line =
        luma->bytes().subspan(row * luma->row_pitch(), 64);
    std::ranges::fill(line.first(32), std::byte{left});
    std::ranges::fill(line.subspan(32), std::byte{right});
  }
  for (std::size_t row = 0; row < 24; ++row) {
    std::ranges::fill(chroma->bytes().subspan(row * chroma->row_pitch(), 64),
                      std::byte{128});
  }
  luma->flush();
  chroma->flush();
  auto recorder = examples::VulkanRecorder::create(device);
  ASSERT_TRUE(recorder.has_value());
  {
    auto commands = recorder->begin();
    ASSERT_TRUE(commands.has_value());
    examples::VulkanRecorder::to_host_sampled(*commands, luma->image());
    examples::VulkanRecorder::to_host_sampled(*commands, chroma->image());
    ASSERT_TRUE(recorder->submit(ReadySemaphore{}).has_value());
  }
  const ImportDescription description{.width = 64,
                                      .height = 48,
                                      .format = TextureFormat::nv12,
                                      .filter = SamplerFilter::nearest,
                                      .premultiplied = false,
                                      .colour_matrix = ColourMatrix::bt709,
                                      .range = ColourRange::limited};
  const std::array planes{luma->image(), chroma->image()};
  EXPECT_FALSE(gpu->import_texture(std::span<const VkImage>{planes},
                                   description,
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
                   .has_value());
  int released = 0;
  {
    auto imported = gpu->import_texture(
        std::span<const VkImage>{planes}, description,
        VulkanGpu::host_import_layout,
        support::ScopedCallback{[&released]() noexcept { ++released; }});
    ASSERT_TRUE(imported.has_value());
    imported->publish();
    show_yuv(*gpu, imported->id());
    ASSERT_TRUE(finish(*gpu));
    const int seen_left = red_at(*gpu, 8, 24);
    const int seen_right = red_at(*gpu, 56, 24);
    std::println("host image NV12: left {} (wanted {}), right {} (wanted {})",
                 seen_left, grey(left), seen_right, grey(right));
    EXPECT_NEAR(seen_left, grey(left), 3);
    EXPECT_NEAR(seen_right, grey(right), 3);
  }
  ASSERT_TRUE(finish(*gpu));
  EXPECT_EQ(released, 1);
}

TEST_F(VulkanDecoderTest,
       AFakeDecoderPlaysThroughTheCopyRouteAndDropsLateFrames) {
  auto gpu = offscreen({.size = small, .images = 2});
  if (!gpu.has_value()) {
    GTEST_SKIP() << skip_reason(gpu.error());
  }
  {
    auto probe = VulkanHostImage::create(
        gpu->device(),
        {.width = 64, .height = 48, .format = VK_FORMAT_R8_UNORM});
    if (!probe.has_value()) {
      GTEST_SKIP() << "the device cannot sample linear host images";
    }
  }
  auto stream = examples::H264AnnexB::load(sample_path());
  ASSERT_TRUE(stream.has_value()) << stream.error();
  const std::size_t units = stream->access_units();
  auto ledger = std::make_shared<Ledger>();
  constexpr std::size_t capacity = 4;
  auto video = examples::VulkanDecodedVideo::create(
      *gpu, std::make_unique<FakeDecoder>(capacity, ledger), std::move(*stream),
      {.width = frame_width,
       .height = frame_height,
       .frame_interval = interval,
       .colour_matrix = ColourMatrix::bt709,
       .range = ColourRange::limited});
  ASSERT_TRUE(video.has_value()) << video.error();
  std::optional<TextureId> shown;
  std::uint64_t last_shown = 0;
  constexpr int steps = 160;
  for (int step = 0; step < steps; ++step) {
    const std::chrono::nanoseconds now = interval * step;
    auto advanced = video->advance(now);
    ASSERT_TRUE(advanced.has_value()) << advanced.error();
    if (advanced->has_value()) {
      shown = **advanced;
    }
    EXPECT_LE(ledger->outstanding, examples::VulkanDecodedVideo::held_frames);
    if (shown.has_value()) {
      show_yuv(*gpu, *shown);
      ASSERT_TRUE(finish(*gpu));
    }
  }
  const examples::VulkanDecodedVideo::Statistics& played = video->statistics();
  std::println(
      "fake decoder: submitted {}, decoded {}, shown {}, dropped {}, "
      "ring waits {}, copy {} us total",
      played.submitted, played.decoded, played.shown, played.dropped,
      played.ring_waits,
      std::chrono::duration_cast<std::chrono::microseconds>(played.copy_time)
          .count());
  EXPECT_EQ(played.shown, static_cast<std::uint64_t>(steps));
  EXPECT_EQ(played.dropped, 0U);
  EXPECT_EQ(played.ring_waits, 0U);
  EXPECT_LE(ledger->most_outstanding, capacity);
  EXPECT_GT(ledger->sample_sizes.size(), units);
  EXPECT_EQ(ledger->released + ledger->outstanding, played.decoded);
  EXPECT_EQ(played.shown + played.dropped + ledger->outstanding,
            played.decoded);
  ASSERT_TRUE(shown.has_value());
  ASSERT_TRUE(finish(*gpu));
  const std::uint8_t code =
      luma_codes[(played.shown + played.dropped - 1) % luma_codes.size()];
  EXPECT_NEAR(red_at(*gpu, 32, 24), grey(code), 3);
  last_shown = played.shown;
  const std::chrono::nanoseconds late = interval * (steps + 12);
  auto skipped = video->advance(late);
  ASSERT_TRUE(skipped.has_value()) << skipped.error();
  EXPECT_TRUE(skipped->has_value());
  EXPECT_GT(video->statistics().dropped, 0U);
  EXPECT_EQ(video->statistics().shown, last_shown + 1);
  EXPECT_LE(ledger->outstanding, examples::VulkanDecodedVideo::held_frames);
}

}
