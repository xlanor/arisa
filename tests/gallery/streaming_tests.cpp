#include <arisa/components/button.hpp>
#include <arisa/components/hstack.hpp>
#include <arisa/components/image.hpp>
#include <arisa/components/text.hpp>
#include <arisa/components/vstack.hpp>
#include <arisa/components/zstack.hpp>
#include <arisa/composition/component.hpp>
#include <arisa/composition/context.hpp>
#include <arisa/composition/element.hpp>
#include <arisa/composition/length.hpp>
#include <arisa/composition/observable.hpp>
#include <arisa/provider/diagnostics/allocation_meter.hpp>
#include <arisa/provider/gpu/external_texture.hpp>
#include <arisa/provider/gpu/pixel_rows.hpp>
#include <arisa/provider/gpu/texture.hpp>
#include <arisa/provider/headless/headless_factory.hpp>
#include <arisa/provider/headless/software_raster.hpp>
#include <arisa/runtime/application.hpp>
#include <arisa/runtime/application_options.hpp>
#include <arisa/theme/colour_value.hpp>
#include <arisa/theme/shape.hpp>
#include <arisa/theme/space.hpp>

#include <gallery_fixture.hpp>
#include <gallery_fonts.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <format>
#include <memory>
#include <optional>
#include <print>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

namespace arisa::runtime {

class StreamingBudgetTest : public GalleryFixture {
 protected:
  static constexpr std::uint32_t video_width = 1280;
  static constexpr std::uint32_t video_height = 720;
  static constexpr std::size_t video_bytes =
      static_cast<std::size_t>(video_width) * video_height * 4;
  static constexpr int warmup_frames = 120;
  static constexpr int measured_frames = 600;
  static constexpr int frames_per_second = 60;

  class Decoder {
   public:
    explicit Decoder(provider::GpuProvider& gpu)
        : gpu_(gpu),
          texture_(std::move(
              gpu.create_texture(provider::TextureDescription{
                                     .width = video_width,
                                     .height = video_height,
                                     .format = provider::TextureFormat::rgba8})
                  .value())),
          pixels_(video_bytes) {}

    auto texture() const noexcept -> provider::TextureId {
      return texture_.id();
    }

    auto frames() const noexcept -> const composition::Observable<int>& {
      return frames_;
    }

    auto decode() -> bool {
      const auto shade = static_cast<std::byte>(decoded_ % 256);
      std::ranges::fill(pixels_, shade);
      const bool uploaded =
          gpu_.get()
              .update_texture(texture_,
                              provider::TextureRegion{.x = 0,
                                                      .y = 0,
                                                      .width = video_width,
                                                      .height = video_height},
                              provider::PixelRows{pixels_})
              .has_value();
      ++decoded_;
      frames_.set(decoded_);
      return uploaded;
    }

   private:
    std::reference_wrapper<provider::GpuProvider> gpu_;
    provider::Texture texture_;
    std::vector<std::byte> pixels_;
    composition::Observable<int> frames_{0};
    int decoded_{0};
  };

  class Video final : public composition::Component {
   public:
    explicit Video(const Decoder* decoder) noexcept : decoder_(decoder) {}

    auto operator==(const Video&) const -> bool = default;

    auto body(composition::Context& context) const
        -> composition::Element override {
      static_cast<void>(context.read(decoder_->frames()));
      return components::Image::External(decoder_->texture())
          .frame({.width = static_cast<float>(video_width),
                  .height = static_cast<float>(video_height)});
    }

   private:
    const Decoder* decoder_;
  };

  class Overlay final : public composition::Component {
   public:
    explicit Overlay(const Decoder* decoder) noexcept : decoder_(decoder) {}

    auto operator==(const Overlay&) const -> bool = default;

    auto body(composition::Context&) const -> composition::Element override {
      return components::ZStack(
          composition::Element{Video{decoder_}},
          components::VStack(
              components::Text("Live"),
              components::HStack(components::Button("Pause", [] {}),
                                 components::Button("Exit", [] {})))
              .padding(theme::space::lg)
              .background(theme::ColourRole::secondary_system_background,
                          theme::Shape::capsule()));
    }

   private:
    const Decoder* decoder_;
  };

  class ImportedDecoder {
   public:
    explicit ImportedDecoder(provider::headless::NullGpu& gpu)
        : image_(std::make_shared<provider::headless::SoftwareRaster::Canvas>(
              video_width, video_height)),
          texture_(std::move(
              gpu.import_texture(image_,
                                 provider::ImportDescription{
                                     .width = video_width,
                                     .height = video_height,
                                     .format = provider::TextureFormat::rgba8})
                  .value())) {}

    auto texture() const noexcept -> provider::TextureId {
      return texture_.id();
    }

    void decode() {
      const float shade = static_cast<float>(decoded_ % 256) / 255.0F;
      image_->fill(provider::Colour{
          .red = shade, .green = shade, .blue = shade, .alpha = 1.0F});
      texture_.publish();
      ++decoded_;
    }

   private:
    std::shared_ptr<provider::headless::SoftwareRaster::Canvas> image_;
    provider::headless::NullGpu::Imported texture_;
    int decoded_{0};
  };

  class ImportedOverlay final : public composition::Component {
   public:
    explicit ImportedOverlay(provider::TextureId texture) noexcept
        : texture_(texture) {}

    auto operator==(const ImportedOverlay&) const -> bool = default;

    auto body(composition::Context&) const -> composition::Element override {
      return components::ZStack(
          components::Image::External(texture_).frame(
              {.width = static_cast<float>(video_width),
               .height = static_cast<float>(video_height)}),
          components::VStack(
              components::Text("Live"),
              components::HStack(components::Button("Pause", [] {}),
                                 components::Button("Exit", [] {})))
              .padding(theme::space::lg)
              .background(theme::ColourRole::secondary_system_background,
                          theme::Shape::capsule()));
    }

   private:
    provider::TextureId texture_;
  };

  struct Tally {
    std::size_t uploads{0};
    std::size_t uploaded_bytes{0};
    std::size_t passes{0};
    std::size_t render_targets{0};
    std::size_t presented{0};
    std::size_t discarded{0};
    std::size_t sampled{0};
    std::uint64_t allocations{0};
    std::uint64_t evaluations{0};
    std::size_t imports{0};
    std::vector<std::chrono::nanoseconds> cpu;
  };

  static auto sampled(const provider::headless::NullGpu& gpu,
                      provider::TextureId texture) -> bool {
    const auto shapes = gpu.last_shapes();
    return std::ranges::any_of(shapes.commands,
                               [texture](const provider::DrawCommand& command) {
                                 return command.texture == texture;
                               });
  }
};

TEST_F(StreamingBudgetTest, OverlayNeverCopiesOrBlocksTheVideoTexture) {
  provider::headless::HeadlessProviderFactory providers;
  load_fonts(providers);
  Decoder decoder{providers.gpu()};
  auto created = Application::create(
      providers, Overlay{&decoder},
      ApplicationOptions{
          .executors = std::nullopt,
          .fonts = examples::GalleryFonts::load(providers.assets()),
          .atlas = {},
          .interaction = {},
          .appearance = std::nullopt});
  ASSERT_TRUE(created.has_value());
  Application application = std::move(*created);
  const provider::headless::NullGpu& gpu = providers.gpu();
  Tally tally;
  tally.cpu.reserve(measured_frames);
  for (int index = 0; index < warmup_frames + measured_frames; ++index) {
    ASSERT_TRUE(decoder.decode());
    const bool measured = index >= warmup_frames;
    const std::size_t uploads = gpu.uploads();
    const std::size_t bytes = gpu.uploaded_bytes();
    const std::size_t passes = gpu.passes();
    const std::size_t targets = gpu.render_targets();
    const std::size_t presented = gpu.frames_presented();
    const std::size_t discarded = gpu.frames_discarded();
    const std::uint64_t before = provider::AllocationMeter::count().value_or(0);
    ASSERT_TRUE(
        providers.clock()
            .advance(std::chrono::microseconds{1'000'000 / frames_per_second})
            .has_value());
    const auto started = std::chrono::steady_clock::now();
    ASSERT_TRUE(application.frame().has_value());
    const auto wall = std::chrono::steady_clock::now() - started;
    const std::uint64_t used =
        provider::AllocationMeter::count().value_or(0) - before;
    if (!measured) {
      continue;
    }
    tally.uploads += gpu.uploads() - uploads;
    tally.uploaded_bytes += gpu.uploaded_bytes() - bytes;
    tally.passes += gpu.passes() - passes;
    tally.render_targets += gpu.render_targets() - targets;
    tally.presented += gpu.frames_presented() - presented;
    tally.discarded += gpu.frames_discarded() - discarded;
    tally.sampled += sampled(gpu, decoder.texture()) ? 1 : 0;
    tally.allocations += used;
    tally.cpu.push_back(
        std::chrono::duration_cast<std::chrono::nanoseconds>(wall));
  }
  EXPECT_EQ(tally.uploads, 0U);
  EXPECT_EQ(tally.uploaded_bytes, 0U);
  EXPECT_EQ(tally.passes, 0U);
  EXPECT_EQ(tally.render_targets, 0U);
  EXPECT_EQ(tally.presented, static_cast<std::size_t>(measured_frames));
  EXPECT_EQ(tally.discarded, 0U);
  EXPECT_EQ(tally.sampled, static_cast<std::size_t>(measured_frames));
  std::ranges::sort(tally.cpu);
  const auto micro = [](std::chrono::nanoseconds value) {
    return std::chrono::duration<double, std::micro>(value).count();
  };
  std::chrono::nanoseconds total{0};
  for (const std::chrono::nanoseconds each : tally.cpu) {
    total += each;
  }
  const std::string report = std::format(
      "streaming overlay: {} frames, wall mean {:.0f} us p95 {:.0f} us, {} "
      "allocations per frame",
      measured_frames,
      micro(total / static_cast<std::int64_t>(tally.cpu.size())),
      micro(tally.cpu[(tally.cpu.size() * 95) / 100]),
      tally.allocations / static_cast<std::uint64_t>(measured_frames));
  RecordProperty("overlay", report);
  std::println("{}", report);
}

TEST_F(StreamingBudgetTest, ImportedVideoPublishesWithoutEvaluatingOrCopying) {
  provider::headless::HeadlessProviderFactory providers;
  load_fonts(providers);
  ImportedDecoder decoder{providers.gpu()};
  auto created = Application::create(
      providers, ImportedOverlay{decoder.texture()},
      ApplicationOptions{
          .executors = std::nullopt,
          .fonts = examples::GalleryFonts::load(providers.assets()),
          .atlas = {},
          .interaction = {},
          .appearance = std::nullopt});
  ASSERT_TRUE(created.has_value());
  Application application = std::move(*created);
  const provider::headless::NullGpu& gpu = providers.gpu();
  Tally tally;
  tally.cpu.reserve(measured_frames);
  const std::size_t imports = gpu.imports();
  for (int index = 0; index < warmup_frames + measured_frames; ++index) {
    decoder.decode();
    const bool measured = index >= warmup_frames;
    const std::size_t uploads = gpu.uploads();
    const std::size_t bytes = gpu.uploaded_bytes();
    const std::size_t passes = gpu.passes();
    const std::size_t presented = gpu.frames_presented();
    const std::uint64_t before = provider::AllocationMeter::count().value_or(0);
    ASSERT_TRUE(
        providers.clock()
            .advance(std::chrono::microseconds{1'000'000 / frames_per_second})
            .has_value());
    const auto started = std::chrono::steady_clock::now();
    ASSERT_TRUE(application.frame().has_value());
    const auto wall = std::chrono::steady_clock::now() - started;
    const std::uint64_t used =
        provider::AllocationMeter::count().value_or(0) - before;
    if (!measured) {
      continue;
    }
    tally.uploads += gpu.uploads() - uploads;
    tally.uploaded_bytes += gpu.uploaded_bytes() - bytes;
    tally.passes += gpu.passes() - passes;
    tally.presented += gpu.frames_presented() - presented;
    tally.sampled += sampled(gpu, decoder.texture()) ? 1 : 0;
    tally.evaluations += application.last_frame().evaluations;
    tally.allocations += used;
    tally.cpu.push_back(
        std::chrono::duration_cast<std::chrono::nanoseconds>(wall));
  }
  EXPECT_EQ(tally.uploads, 0U);
  EXPECT_EQ(tally.uploaded_bytes, 0U);
  EXPECT_EQ(tally.passes, 0U);
  EXPECT_EQ(tally.evaluations, 0U);
  EXPECT_EQ(gpu.imports(), imports);
  EXPECT_EQ(tally.presented, static_cast<std::size_t>(measured_frames));
  EXPECT_EQ(tally.sampled, static_cast<std::size_t>(measured_frames));
  std::ranges::sort(tally.cpu);
  const auto micro = [](std::chrono::nanoseconds value) {
    return std::chrono::duration<double, std::micro>(value).count();
  };
  std::chrono::nanoseconds total{0};
  for (const std::chrono::nanoseconds each : tally.cpu) {
    total += each;
  }
  const std::string report = std::format(
      "imported video: {} frames, wall mean {:.0f} us p95 {:.0f} us, {} "
      "allocations per frame",
      measured_frames,
      micro(total / static_cast<std::int64_t>(tally.cpu.size())),
      micro(tally.cpu[(tally.cpu.size() * 95) / 100]),
      tally.allocations / static_cast<std::uint64_t>(measured_frames));
  RecordProperty("imported", report);
  std::println("{}", report);
}

}
